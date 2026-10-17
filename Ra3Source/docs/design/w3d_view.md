# W3DView 与 W3DRainManager 重建

## 证据与布局

本批静态证据来自 `ra3_1.0.game` clean `.text`、W3DView 虚表
`0x00BE5600–0x00BE57E8`、函数调用点和断言源路径。Generals 的
`W3DView.h/.cpp` 与 `GameClient/View.cpp` 只用于恢复语义名称；实现为按 RA3
指令和偏移重新编写的原创代码。

RA3 的完整 W3DView 大于 `0x2740` 字节。本批只公开被已验证函数触及的布局：

- 过滤器：`+0x5C/+0x64/+0x68`，延迟标志 `+0x4B9`。
- 相机移动：角度 `+0x28`、缩放 `+0x40`、俯仰 `+0x74`、高度 `+0x78`；
  四组移动状态位于 `+0x3E8/+0x41C/+0x444/+0x468/+0x494` 附近。
- 视锥平面数组：`+0x308/+0x30C`，每项 16 字节。
- 相机抖动：`+0x2728` 起三个 float 和五个字节标志。
- 雨段为 0x50 字节，`+0x14` 为 enabled，`+0x18` 为可见性位，
  `+0x1C` 为球心，`+0x28` 为半径。雨滴四顶点共 0x60 字节。

## 离线验证函数

| VA | Ra3Source 符号 | 行为 | 用例 |
|---|---|---|---:|
| `0x00562250` | `scaledHeight` | `+0xA8 * +0x40` | 500 |
| `0x00562260` | `guardBandBias` | 返回 `this+0x150` | 50 |
| `0x00562290` | `setWireframePending` | 写 `+0x351` 原始字节 | 50 |
| `0x005622A0` | `clearFilter` | 清过滤状态 | 500 |
| `0x005622C0` | `setFilter` | 设置模式、类型和延迟状态 | 500 |
| `0x00562570` | `cameraMovementFinished` | 汇总八个移动状态 | 500 |
| `0x005625C0` | `cameraLockEquals` | 比较锁定 ObjectId | 500 |
| `0x00562910/20/30` | `cameraShakeX/Y/Intensity` | x87 读取三个 float | 各 1500 |
| `0x00562940` | `cameraShakeActive` | 返回原始标志字节 | 50 |
| `0x00562950` | `clearCameraShake` | 清 0x11 字节状态 | 500 |
| `0x00562990/A0/B0/C0` | `setCameraShakeFlag0..3` | 写原始标志字节 | 各 50 |
| `0x00568B60` | `setRainEnabled` | 遍历 0x50 字节雨段 | 500 |
| `0x00568E40` | `fillRainIndices` | 16000 个四边形的六索引 | 16000 项 |
| `0x005ABC30` | `configureEase` | ParabolicEase 参数归一化 | 1500 |

三个抖动 getter 在 CW `007F/027F/037F` 下比较完整 80 位 ST0。signaling NaN
使用本机 `fld m32/fstp tbyte` 结果弥补 Unicorn 不 quiet 的已知偏差。
`scaledHeight` 在游戏实测 CW `007F` 比较完整 ST0；宽精度控制字会保留超过
float 的乘积尾数，留给主控实机 naked shadow。

模块还包含雨滴顶点生成、雨索引生成、球体/视锥测试、相机样本平移、四组相机
移动参数与插值实现。它们已构建，但尚未记入函数台账，因为还没有逐入口离线
差分，不能把“按反汇编实现”混同于已证明等价。

## Hook

11 个目标均 `explicitOnly=true`。写状态的 wrapper 只比较原函数实际写入的字段；
只读目标比较返回值。三个 x87 getter没有挂钩，避免 C++ `float` wrapper 丢失
80 位返回值。`0x00568B60` 依赖运行时全局开关，当前也不挂。

`0x00568B90` 会调用客户端 RNG 16000×7 次并锁定 D3D 顶点缓冲；
`0x00568E40` 直接锁定索引缓冲。前者不适合普通 shadow，后者虽已离线验证，
仍因 D3D 锁定副作用不挂钩。

## 未解决问题

- `0x0056CE60`（Generals 对应 `Add_Camera_Shake`）依赖客户端随机流和运行时全局，
  需快照 RNG 后做状态式 shadow。
- 相机屏幕/世界坐标换算的大部分虚函数位于本窗口之外的 `0x00529xxx` 等地址，
  不属于本批 VA 边界。
- `0x0056AF30` 是 inner-patched 边界，本批未分析或移植。
