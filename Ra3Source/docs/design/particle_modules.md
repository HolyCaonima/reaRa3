# 粒子模块重建

本模块重建 `ra3_1.0.game` 的 FXParticleSystem 纯计算、资源到运行时布局复制和只读字段函数。命名参考 Generals 的 `GameClient/System/ParticleSys.cpp` 与 `ParticleSys.h`；地址、字段偏移和运算顺序均以 RA3 反汇编为准。

## 布局

- 所有模块视图的资源数据指针位于 `+0x08`。
- `CylinderVolumeModuleView +0x0c` 是当前半径。
- box 资源：`hollow +0x08`，三维半尺寸 `+0x0c`。
- cylinder 资源：`hollow +0x08`，长度 `+0x14`，偏移 `+0x18`。
- line 资源：起点 `+0x0c`，终点 `+0x18`。
- sphere 资源：`hollow +0x08`，半径 `+0x0c`。
- GPU draw 资源读取 `+0x0c/+0x10/+0x14/+0x24/+0x28/+0x2d/+0x30..+0x40`。
- `EmitterModuleRuntimeData` 为 0x24 字节；前三种运行时构造只写 vptr、类型、`+0x08` 单字节和六个 float，不改 `+0x09..+0x0b`。
- `PointVelocityTemplateData` 为 0x2c 字节，含三个 12 字节 `RandomVariable`。
- `BoxVolumeRuntimeData` 为 0x24 字节，随机变量位于 `+0x14`。

## 函数

| 地址 | Ra3Source 名称 | 行为 / Generals 对应 | hook 键 |
|---|---|---|---|
| 00661160 | `initializeDefaultAlphaTemplate` | 0x98 字节 alpha 模板复制 | — |
| 006611c0 | `defaultAlphaTemplateTypeName` | 类型名 | — |
| 00661200 | `windTemplateTypeName` | 类型名 | — |
| 00661420 | `defaultAlphaParticleTypeName` | 类型名 | — |
| 006619e0 | `defaultColorParticleTypeName` | 类型名 | — |
| 006619f0 | `colorWithinRange` | 三通道 x87 范围判断 | — |
| 00661ab0 | `initializeBoxVolumeRuntime` | box 资源到 0x24 运行时布局 | — |
| 00661b00 | `boxVolumeDebugDimensions` | 取 box 的两个调试尺寸 | — |
| 00661b80 | `boxVolumeTypeName` | 类型名 | — |
| 00661cf0 | `emitBoxPoint` | Generals box 随机点/空心表面 | — |
| 00662020 | `cylindricalVelocityTypeName` | 类型名 | — |
| 00662030 | `cylindricalVelocity` | Generals cylindrical velocity | — |
| 006620a0 | `initializeCylindricalVelocityRuntime` | type 5 运行时布局 | `fxpscylvelocityruntime` |
| 006621a0 | `emitCylinderPoint` | Generals cylinder 随机点 | — |
| 00662330 | `initializeLineVolumeRuntime` | type 1 运行时布局 | `fxpslinevolumeruntime` |
| 00662380 | `lineVolumeTypeName` | 类型名 | — |
| 00662460 | `emitLinePoint` | Generals line 线性插值 | — |
| 00662570 | `initializePointVelocityTemplate` | 三个随机变量复制 | `fxpspointvelocitytemplate` |
| 006625b0 | `pointVelocity` | 三个随机变量产生速度向量 | — |
| 00662670 | `pointVolumeTypeName` | 类型名 | — |
| 006626f0 | `zeroVector3` | 写零三维向量 | — |
| 006627c0 | `initializeSplineVolumeRuntime` | type 2 运行时布局 | `fxpssplinevolumeruntime` |
| 00662810 | `splineVolumeTypeName` | 类型名 | — |
| 00665c40 | `randomUnitVector` | 三次 client random 后归一化 | — |
| 00669190 | `shuffleDwords` | client 随机流 Fisher–Yates | — |
| 0066a700 | `defaultUpdateTypeName` | 类型名 | — |
| 0066a710 | `defaultUpdateLifetimeData` | data + 0x44 | `fxpsupdatelifetime` |
| 0066a720 | `defaultUpdateSizeData` | data + 0x50 | `fxpsupdatesize` |
| 0066a730 | `defaultUpdateRotationData` | data + 0x5c | `fxpsupdaterotation` |
| 0066a840 | `defaultUpdateParticleTypeName` | 类型名 | — |
| 0066a850 | `one` | x87 返回 1.0 | — |
| 0066a860 | `zeroVector4` | 写零四维向量 | — |
| 0066a8d0 | `scaleRandomUnitVector` | 单位向量乘半径 | — |
| 0066a940 | `randomVariableUnitVector` | 随机半径乘单位向量 | — |
| 0066a9a0 | `emitSpherePoint` | Generals sphere 随机点 | — |
| 0066ab40 | `gpuDrawTypeName` | 类型名 | — |
| 0066ab50 | `gpuFramesPerRow` | GPU 字段 `+0x0c` | `fxpsgpuframesperrow` |
| 0066ab60 | `gpuTotalFrames` | GPU 字段 `+0x10` | `fxpsgputotalframes` |
| 0066ab70 | `gpuSpeedMultiplier` | x87 GPU 字段 `+0x24` | `fxpsgpuspeed` |
| 0066ab80 | `gpuSingleRow` | GPU 字段 `+0x14` | `fxpsgpusinglerow` |
| 0066ab90 | `gpuSortParticles` | GPU 字段 `+0x28` | `fxpsgpusortparticles` |
| 0066aba0 | `gpuMiscWord` | GPU 字段 `+0x40` | `fxpsgpumiscword` |
| 0066abb0 | `gpuEnableSoftParticles` | 只写 AL，EAX 高 24 位沿用 data 指针 | `fxpsgpusoftparticles` |
| 0066abc0 | `gpuMiscValues` | x87 复制 `+0x30` 的四个 float | `fxpsgpumiscvalues` |
| 0066cdd0 | `windTypeName` | 类型名 | — |

## 浮点和随机流怪癖

几何函数使用 client 随机流 `0x00CB73F0`。oracle 在 CW `0x007f/0x027f/0x037f` 下同时比较输出和 24 字节后状态。cylinder 和 cylindrical velocity 保留了原函数跨 `fcos/fsin` 的 x87 栈运算；随机变量和实数 helper 使用 `src/sim` 的扩展精度入口。原函数的 `fld m32` 会 quiet signaling NaN，Unicorn 不会，oracle 因而按本机 x87 往返规则修正预期。

`0066abb0` 的返回不是规范化 bool：反汇编先把 data 指针装入 EAX，再只覆盖 AL。实现和 oracle 都保留这个约定。

## 未解决和刻意不挂

- `006658a0` spline 采样、`0066abf0` GPU storage 初始化、`0066cd60/0066cde0/0066ce40` wind 构造/力计算尚未移植；它们涉及缓存、分配器、引用计数或更大的游戏对象图。
- 随机几何函数虽已离线逐位验证，但本批不挂 hook，避免在没有实机的情况下处理全局随机流快照和隐藏输出参数 ABI。
- 15 个已挂目标均读取游戏对象布局，全部 `explicitOnly=true`；构造函数 shadow 比较完整的实际写入布局，getter 比较返回值或输出缓冲。
