# 路线 A Hook 候选目录

## 1. 目的与边界

本目录用于批量筛选 `ra3_1.0.game` 中适合进程内替换的函数。它只分析
`function_ledger.tsv` 中 `section == .text && securom == clean` 的函数，不读取
`Stext`、`Sdata` 或 `.securom` 的实现，也不启动游戏。结论来自静态分析；`tier`
表示“值得继续确认 ABI 与语义”，不是已经可以直接写入 `Targets.cpp`。

刷新后 Ghidra 项目共有 48,986 个台账函数，比旧台账增加
`0x004B1C80`，没有地址消失。候选导出实际处理 40,333 个 clean `.text` 函数。
SecuROM 分类沿用已审定的 `work/analysis/ghidra/securom_*.tsv`，本次没有重新判定
保护状态。

## 2. 可复现方法

### 2.1 Ghidra 导出

`ExportHookCandidates.java` 对每个 clean `.text` 函数记录：

- 大小、指令数、所有 `ret` 形式、内部/外部调用、未解析调用；
- 绝对地址读写、`fs:`、x87/SSE、后向边（循环）、直接调用者数；
- 最多三个字符串、反编译器参数数与调用约定、助记符计数。

生成器再直接读取只读游戏镜像，计算入口 16 字节、最短完整 5 字节 stolen
prefix、位置无关性，并保守排除间接尾跳转。绝对地址写检测不把
`[eax]` 一类“写入调用者传入内存”算成全局写；后者是许多构造器和输出参数函数的
正常语义。直接调用者数只统计 Ghidra 已建立的静态调用引用。

从 `D:\Game\Ra3\RevRa3-hookcat` 执行（所有 Ghidra 命令均无 `-Save`）：

```powershell
$env:GHIDRA_INSTALL_DIR = 'D:\Game\Ra3\RevRa3\ghidra_12.1.4_PUBLIC'
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot'
$project = 'D:\Game\Ra3\RevRa3\work\ghidra'
$scratch = 'D:\Game\Ra3\RevRa3\work\hookcat'

.\Ra3Source\tools\ghidra\run_headless.ps1 `
  -ProjectDir $project -Script ExportProgramOverview.java `
  -ScriptArgs "$scratch\ghidra_export"

Copy-Item D:\Game\Ra3\RevRa3\work\analysis\ghidra\securom_flow_sites.tsv `
  "$scratch\ghidra_export\securom_flow_sites.tsv"
Copy-Item D:\Game\Ra3\RevRa3\work\analysis\ghidra\securom_thunk_bodies.tsv `
  "$scratch\ghidra_export\securom_thunk_bodies.tsv"

python .\Ra3Source\tools\analysis\build_function_ledger.py `
  --inputs "$scratch\ghidra_export" `
  --out .\Ra3Source\docs\analysis\ra3_1.0\function_ledger.tsv

.\Ra3Source\tools\ghidra\run_headless.ps1 `
  -ProjectDir $project -Script ExportHookCandidates.java `
  -ScriptArgs "$scratch\hook_candidates_raw.tsv",`
    'D:\Game\Ra3\RevRa3-hookcat\Ra3Source\docs\analysis\ra3_1.0\function_ledger.tsv'

python .\Ra3Source\tools\analysis\build_hook_candidates.py `
  --export "$scratch\hook_candidates_raw.tsv" `
  --ledger .\Ra3Source\docs\analysis\ra3_1.0\function_ledger.tsv `
  --exe D:\Game\Ra3\Game\Data\ra3_1.0.game `
  --out .\Ra3Source\docs\analysis\ra3_1.0\hook_candidates.tsv
```

### 2.2 Tier 规则

- **Tier 1**：无内部或外部调用、无未解析调用/间接尾跳、无绝对地址写、无
  `fs:`，入口可搬移，且存在已知 `ret` 形式。
- **Tier 2**：其余条件同 Tier 1；允许只调用 Tier 1 函数，或白名单纯 CRT
  （`tolower/toupper/strlen/strcmp/memcpy/memmove/memset/memcmp` 及常用纯数学函数）。
- **`-`**：不满足上述条件，不是当前候选。

模式是确定性的有序启发式：已知导入/名称优先识别字符串比较、大小写、拷贝和填充；
x87/SSE 识别浮点；极小叶函数识别字段访问；循环和位运算识别容器遍历、
hash/CRC 或整数位运算。`hash-crc` 仅表示“形状类似”，不能单凭此列断言算法就是
CRC。当前有明确符号的家族包括 `AptNativeHash::*`、`EAStringC::*Hash*`；已确认的
`0x00417DB0` 是转小写 SuperFastHash 变体。

## 3. 统计

| pattern | Tier 1 | Tier 2 | 合计 |
|---|---:|---:|---:|
| field-accessor | 2,550 | 0 | 2,550 |
| floating-point | 1,231 | 354 | 1,585 |
| other | 833 | 300 | 1,133 |
| loop-transform | 747 | 193 | 940 |
| integer-bitmath | 462 | 92 | 554 |
| hash-crc | 297 | 103 | 400 |
| memory-copy | 0 | 93 | 93 |
| string-utility | 67 | 4 | 71 |
| memory-fill | 0 | 34 | 34 |
| string-compare | 0 | 16 | 16 |
| case-conversion | 0 | 7 | 7 |
| container-walk | 0 | 1 | 1 |
| **总计** | **6,187** | **1,197** | **7,384** |

候选中 849 个函数出现 x87，1,360 个出现 SSE，489 个同时出现二者。SSE 标记也会
覆盖仅用向量寄存器批量清零的构造器，所以是否存在浮点语义仍须看反编译结果。

## 4. 直接调用者最多的候选

ABI 列原样保留“Ghidra 反编译调用约定（参数数）/机器码返回形式”。其中
`__stdcall(...)/ret` 是明显的推断冲突：裸 `ret` 表示调用者清栈，应在接入前按调用点
复核为 cdecl/寄存器约定；`ret N` 表示被调者清栈，thiscall 还需确认 `ecx`。下表的
含义均逐个查看过反编译结果。

| 地址 | Tier | 调用者 | ABI | 推断语义 |
|---|---:|---:|---|---|
| `0050B5F0` | 1 | 876 | `__thiscall(3)/ret 8` | 哈希桶链查找，返回节点与桶位置 |
| `009C9980` | 1 | 564 | `__stdcall(1)/ret` | 红黑树迭代器前驱 |
| `00686FA0` | 1 | 217 | `__fastcall(1)/ret` | 返回 `this+0x418` |
| `0078D880` | 2 | 194 | `__fastcall(1)/ret 4` | 查表并返回命中节点 value |
| `00B61ED0` | 1 | 192 | `__fastcall(1)/ret` | 写虚表并清零两个字段 |
| `0098F720` | 1 | 177 | `__thiscall(2)/ret 4` | 追加单链表尾节点 |
| `00417070` | 1 | 176 | `__fastcall(1)/ret` | 清零两个相邻字段 |
| `00788640` | 1 | 173 | `__fastcall(1)/ret` | 仅写虚表的构造初始化 |
| `005A9F60` | 1 | 163 | `__thiscall(2)/ret 8` | 以 999999/0 初始化对象 |
| `007E1E30` | 1 | 147 | `__thiscall(2)/ret 4` | 按位掩码选择并消费 20 槽之一 |
| `006C3A10` | 1 | 137 | `__thiscall(3)/ret 8` | 无符号键红黑树 `lower_bound` |
| `004D29D0` | 1 | 136 | `__fastcall(1)/ret` | 双向链表摘除并重置为自环 |
| `009C9A50` | 1 | 134 | `__stdcall(4)/ret` | 红黑树插入旋转/着色修复 |
| `004D2EE0` | 1 | 129 | `__fastcall(2)/ret` | **x87** 浮点转 64 位整数及舍入 |
| `00AAFE10` | 1 | 127 | `__thiscall(3)/ret 8` | 有符号键红黑树 `lower_bound` |
| `00B66F80` | 2 | 103 | `__stdcall(2)/ret` | 红黑树删除与再平衡 |
| `005A94C0` | 2 | 101 | `__stdcall(2)/ret` | 闭区间伪随机整数 |
| `009C99C0` | 1 | 98 | `__stdcall(1)/ret` | 红黑树迭代器后继 |
| `007E20F0` | 1 | 91 | `__fastcall(1)/ret` | 按状态在两个对象字段间选择 |
| `0094AF80` | 1 | 89 | `__stdcall(2)/ret` | 定长记录数组边界检查与寻址 |
| `00B4D0F0` | 1 | 89 | `__stdcall(1)/ret` | 返回首字段 |
| `005A9550` | 2 | 89 | `__stdcall(2)/ret` | **x87/SSE** 区间伪随机浮点数 |
| `00687C90` | 2 | 87 | `__fastcall(1)/ret` | 条件满足时触发嵌套对象辅助逻辑 |
| `009971C0` | 1 | 80 | `__stdcall(0)/ret` | 返回全局单例/状态指针 |
| `005B6150` | 1 | 78 | `__thiscall(5)/ret 10h` | 大型对象构造；SSE 只用于清零 |
| `007D2CB0` | 2 | 78 | `__thiscall(4)/ret 0Ch` | 对象构造并清理一个区间 |
| `007E1C70` | 1 | 70 | `__thiscall(2)/ret 4` | 返回 `this+0x34` 的索引槽 |
| `00A77AE0` | 1 | 70 | `__thiscall(2)/ret 4` | 初始化二字段结构为 `{1,arg}` |
| `00A77BD0` | 1 | 70 | `__thiscall(2)/ret 4` | 复制 NUL 字符串到 `this+0x20` |
| `004CB400` | 2 | 70 | `__thiscall(2)/ret 4` | 比较两个带 16 位长度的字节串 |
| `007E1BA0` | 1 | 68 | `__fastcall(1)/ret` | 测试两个状态字节都为零 |
| `004C9EA0` | 1 | 67 | `__stdcall(3)/ret 0Ch` | 定长字节序列三路比较 |
| `00983A90` | 1 | 66 | `__thiscall(2)/ret 4` | 写虚表并初始化对象字段 |
| `00B23C00` | 1 | 65 | `__fastcall(1)/ret` | 清零首字段 |
| `006A4F80` | 2 | 64 | `__thiscall(3)/ret 8` | 事件 0x16A 时派发 +1/-1 |
| `006A54D0` | 1 | 63 | `__fastcall(1)/ret` | 由状态位/对象身份计算状态码 |
| `006C0ED0` | 2 | 59 | `__fastcall(1)/ret` | 组合状态位与谓词得到布尔值 |
| `0078DA40` | 2 | 56 | `__thiscall(2)/ret 4` | 遍历关联 ID 判断两对象是否关联 |
| `004D9100` | 1 | 55 | `__stdcall(2)/ret` | **x87/SSE** 三维 float 向量归一化 |
| `00548BE0` | 2 | 55 | `__thiscall(4)/ret 0Ch` | 二维边界检查后计算网格元素地址 |
| `005D5AE0` | 1 | 53 | `__thiscall(2)/ret 4` | 范围检查后读取六元素数组 |
| `007E4AB0` | 1 | 53 | `__thiscall(2)/ret 4` | 动态数组检查后读取指针元素 |
| `004E41B0` | 1 | 51 | `__thiscall(3)/ret 8` | 复制 8 字节元素区间并收缩尾部 |
| `00B11D40` | 1 | 50 | `__fastcall(1)/ret` | 返回 `this+0xAC` |
| `00861A70` | 2 | 49 | `__stdcall(1)/ret 4` | 用带长度字符串在线性链中查找 |
| `0040A390` | 1 | 48 | `__stdcall(2)/ret` | 按正/负索引访问 16 字节记录 |
| `00417080` | 1 | 48 | `__thiscall(2)/ret 4` | 复制两个相邻 32 位字段 |
| `007E1DF0` | 1 | 48 | `__thiscall(2)/ret 4` | 在 20 槽中找类型位精确匹配项 |
| `00997400` | 1 | 47 | `__stdcall(0)/ret` | 返回全局对象地址 |
| `006873C0` | 1 | 46 | `__fastcall(1)/ret` | 返回 `this+0x454` |
| `009C9DD0` | 2 | 46 | `__thiscall(5)/ret 10h` | **x87/SSE + ceil**，按比例更新容量 |

高调用数集中在 EASTL 风格红黑树、哈希桶、字段访问器和小型容器操作。它们的优点是
覆盖面大，缺点是 ABI 错误会广泛破坏状态；应先选三路比较、字段 getter、纯整数范围
函数，而不是直接替换树旋转/删除。

## 5. 家族、现有实现与浮点风险

- **字符串/内存**：16 个比较、71 个字符串工具、93 个拷贝、34 个填充和 7 个大小写
  候选。`0x004C9EA0` 是不依赖 CRT 的定长三路比较，适合先做 shadow。
- **哈希**：启发式得到 400 个 `hash-crc` 形状；有符号的
  `AptNativeHash::*`、`EAStringC::*Hash*` 是可继续成组确认的家族。未发现仅凭符号即可
  确认的 CRC 叶函数，不能把 400 个都称为 CRC。
- **容器**：红黑树前驱/后继、`lower_bound`、插入/删除修复及哈希桶查找占据调用者榜首。
  大部分能从指针形状确认算法家族，但节点布局和 ABI 仍需逐调用点验证。
- **已有 Ra3Source 等价物**：`0x00417DB0` → `ra3::asset::hashName`；
  `0x009F3650` → `ra3::io::comparePathNames`；`0x009F72E0` →
  `ra3::io::decompressRefPack`；`0x009F74F0` →
  `ra3::io::readRefPackHeader`。另有 `0x009F5E90..0x009F6AA0` 的
  BigArchive/BigHandler 叶函数簇在台账 notes 中已有对应线索。前 3 个已经走过实机
  shadow/replace 的以 `injection.md` 为准。
- **浮点**：x87 的 80 位中间精度、控制字舍入模式，以及 SSE 的 32/64 位中间舍入会让
  bit-exact shadow 比较困难。优先避开 `0x004D2EE0`、`0x005A9550`、
  `0x004D9100`、`0x009C9DD0`；若必须接入，应明确保存/恢复 FPU 控制字，并按位比较
  NaN、负零和舍入边界。仅用 SSE 清零的构造器不属于此风险。

## 6. 仍需确认

1. 高排名函数的 Ghidra 调用约定与裸 `ret` 经常冲突；接入前必须检查代表性调用点的
   压栈、`ecx/edx` 使用和返回后的栈平衡。
2. 静态“无绝对地址写”不能证明完全纯：通过参数指针写内存是允许的输出语义，但别名
   可能指向全局对象。shadow 需要为每种函数设计不扰动原状态的比较方式。
3. 间接调用虽由 Ghidra 引用解析，虚调用和未建表跳转仍可能漏失；生成器已保守排除
   未解析调用和间接尾跳，但不能证明反汇编覆盖完整。
4. `pattern` 是排序辅助，不是函数命名结论；尤其 `hash-crc`、`floating-point` 和
   `field-accessor` 应结合反编译及调用者再确认。
