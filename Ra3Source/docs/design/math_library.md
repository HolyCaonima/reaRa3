# 数学库重建

## 1. 布局

`Ra3::math` 的公开类型保持游戏的 32 位内存布局：

| 类型 | 大小 | 布局 |
|---|---:|---|
| `Vector2` | `0x08` | `x, y` |
| `Vector3` | `0x0C` | `x, y, z` |
| `Vector4` | `0x10` | `x, y, z, w` |
| `Matrix3x4` | `0x30` | 三行、每行四个 `float`；行起点为 `0x00/0x10/0x20` |

`0x00994A10` 使用每行前三项变换方向向量，不读取 `0x0C/0x1C/0x2C`
的平移列。`0x009814E0` 所在对象的输入向量位于 `+0x10`，比例位于 `+0x24`。

## 2. 指令与精度约束

模块以 `/arch:SSE2 /fp:precise` 编译。原函数用 `mulss/addss/subss` 的地方使用
标量 SSE intrinsic，并保持逐条运算顺序。`transformDirection` 的三个矩阵行使用的
结合顺序不同；实现用内联汇编固定操作数方向，因为 NaN payload 的传播不能由普通 C++
表达式或优化器保证。

`squaredDistance`、`distance` 使用小型 x87 汇编块，分别复现
`dx²+dy²+dz²` 和结尾的 `fsqrt`。`normalize(Vector2)` 先用 SSE 求平方和，再由 x87
执行 `sqrt` 和倒数并存回单精度，最后用 `mulss`。`approximateLength` 排序绝对值后，
按 x87 顺序计算 `smallest*0.25 + middle*0.34375 + largest`。这些实现不修改控制字，
因此继承调用者的精度和舍入设置。

`squaredDistance`、`distance`、`approximateLengthExtended` 和
`squaredDistanceFromOffset38Extended` 的原 ABI 要求把计算结果留在 80 位 `ST0`；
不能经 MSVC 的 `float` 临时量返回。它们的 x86 实现和 hook 入口均使用 naked 汇编。
shadow 把游戏值和重建值分别 `fstp tbyte` 到两个十字节临时区，逐字节比较后再
`fld tbyte` 游戏值；replace 以同样方式跨统计调用保存重建值。两条路径返回时均恰有
一个未舍入值位于 x87 栈顶。其余 x87 返回目标只把内存中的单精度值 `fld` 到 `ST0`，
不存在额外尾数或扩展指数需要保留。

## 3. 离线验证

`tools/analysis/ports/verify_math.py` 通过 Unicorn 执行 shipped 函数，通过
`ra3_portcheck` 执行重建函数。x87 敏感函数在 `CW=0x007F`（24 位）、
`0x027F`（53 位）、`0x037F`（64 位）下各跑 2,000 个随机有限输入及 14 组有限
边界值；每个组合均 `2014/2014`。上述四个扩展返回函数把 Unicorn 的物理 FP 寄存器
按 TOP 映射为 `ST0`，与 portcheck 立即 `fstp tbyte` 输出的完整十字节值比较；用例包含
会超出单精度指数范围的极大差值和 denormal。其余数学函数跑 2,000 个
随机有限输入及 31 组边界值，均 `2031/2031`；方向变换为 `2000/2000`，
CRC-32 为 `2004/2004`。Debug 与 Release 各执行一次。

Unicorn 对 signaling NaN 的 x87 `fld/fstp` 静默化以及多个 NaN 参与 SSE 运算时的
payload 选择与真实 x86 硬件不一致：相同指令序列会产生不同 payload。因此 x87 敏感
函数和矩阵变换的 NaN payload 仍需实机 shadow；这里不把该部分记为离线 bit-exact
证据。普通 quiet NaN、正负零、denormal 和正负无穷已覆盖于不经过上述不可靠路径的
函数。

每个使用 x87 的 hook 在入口记录一次调用者的控制字和 MXCSR。汇总行输出首次观察值
`cw=0x.... mxcsr=0x........`，并以 `cw_changes` 统计后续控制字不同的调用；hook
不写控制字或 MXCSR。该证据用于实机确认 D3D9 场景实际采用的精度控制。

## 4. 本批范围

本批重建 16 个数学函数：最大/最小/夹取、三维精确相等、二维区间精确相等、
三维距离平方与距离、三维近似长度、二维归一化、三维缩放、三维方向变换、
ARGB 解包、二维包含测试、二维分离、对象 `+0x38` 距离平方和对象 `+0x68`
夹取。三角函数、四元数插值和矩阵旋转留待可靠的 `fsin/fcos/fpatan` 预言机或实机
shadow。
