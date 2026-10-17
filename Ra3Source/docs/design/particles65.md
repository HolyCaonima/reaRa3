# FXParticleSystem 更新、绘制、物理与发射速度模块重建 (`particles65`)

本批次在已合并 `fxps` 批次（45 个函数）的基础上，继续重建 `ra3_1.0.game` 客户端区域 `0x0065E1E0-0x0066CE40` 中 50 个计数的干净（`securom == clean`）的 `FXParticleSystem` 模块模板初始化/复制、粒子级生命周期初始化、每帧关键帧与物理更新、外向发射速度计算以及样条/矩阵几何辅助函数。

## 归属证据与参考锚点

逐函数的所有权证明写在 `particles65_annotations.tsv` 的 `ownership:` 字段，均可由二进制脚本复核：

1. **模块 vtable 槽位（16 个）**：函数指针位于 `vtables_split.tsv` 所列 vtable 的槽位上，且该表 `slot[2]` 返回类名字符串，如 `0x00BF3558` -> `"DefaultAlphaParticleModule"`、`0x00BF3588` -> `"DefaultColorParticleModule"`、`0x00BF3F9C` -> `"DefaultUpdateParticleModule"`、`0x00BF4418` -> `"RenderObjectParticleUpdateModule"`、`0x00BF478C` -> `"WindModule"`、`0x00BF31D4` -> `"WindParticleModule"`、`0x00BF3154` -> `"SwarmPhysicsParticleModule"`、`0x00BF4AB0` -> `"OutwardEmissionVelocityModule"`。`0x00665B60` 是 Box/Sphere/TerrainFire 三种 emission volume 共用的 slot 8。
2. **直接调用边（34 个）**：函数是上述模块 vtable 方法、模板 vtable 方法（如 `0x00BF3100`、`0x00BF2CFC`、`0x00BF38D0`）或断言 `0x00BF4190`（`...\FXParticleSystem\FxpsGPUStorageModule.cpp`）的 `0x0066ABF0` 的直接被调者，注释给出调用点地址。`0x0065E490` 的唯一调用链来自 `W3DScriptedModelDraw`（vtable `0x00C55668` slot 14），所属类未证实。
3. **剔除（3 个）**：`0x0065F530`、`0x00662A00`、`0x00662CA0` 的唯一调用者在代码与数据中均无引用，无锚点边，记为延后行（实现保留、不计数）。

3. **参考资料**：
   - `work/references/generals_code/GeneralsMD/Code/GameEngine/Source/GameClient/System/FXParticleSystem/`（EA GPL Generals 粒子系统架构参考）
   - `work/references/modding/` 下 RA3 FXParticleSystem 模块 XSD 定义

## 核心结构布局

- `DefaultAlphaParticleModule`（`0x54` 字节）：`+0x00` vtable，`+0x04` `FXParticle*`，`+0x08` `currentAlpha`，`+0x0C` `alphaStepPerFrame`，`+0x10..+0x4F` 8 个 `AlphaKeyframe`（`value`, `timeFraction`），`+0x50` `nextKeyIndex`。
- `DefaultColorModule`（`0xAC` 字节）与 `DefaultColorParticleModule`（`0xA4` 字节）：8 个 16 字节 `ColorKeyframe`（`r, g, b, timeFraction`），粒子模块在 `+0x08..+0x1F` 维护当前 RGB 与每帧步长，`+0xA0` 维护 `nextKeyIndex`。
- `WindModuleTemplate`（`0x40` 字节）、`WindModule`（`0x20` 字节）、`WindParticleModule`（`0x20` 字节）：支持 PingPong（`windMotion == 2`）与 Circular（`windMotion == 3`）角度演化，以及基于距离衰减和余弦湍流的粒子受力计算。
- `DefaultUpdateParticleModule`（`0x30` 字节）与 `RenderObjectParticleUpdateModule`（`0x4C` 字节）：分别维护标量（`size` 在 `+0x10`）与三维向量（`size.x` 在 `+0x38`）的尺寸、旋转角、角速度及阻尼；`0x00660530`（`FXParticle::getSize`）通过虚表区分两类模块并在 x87 `ST0` 返回当前尺寸。
- `LightningDrawModuleTemplate`（`0xAC` 字节）与 `LightningDrawModule`（`0x40` 字节）：包含 6 个标量浮点参数与 12 个交错排列的 `RandomVariable`（2 组振幅/频率/相位对）；`0x00662B60` 将粒子状态打包为 `0x28` 字节的 `LightningSwarmVertex` 顶点序列。

## 函数清单（50 个计数，3 个延后）与 15 个 Hook 目标

| 地址 | C++ 函数 (`ra3::gameclient`) | 行为概述 | Hook 键 (`explicitOnly`) |
|---|---|---|---|
| `0x0065E1E0` | `computeDefaultAlphaRate` | 计算相邻 Alpha 关键帧间的每帧插值步长 | `fxpsalpharate` (`true`) |
| `0x0065E490` | `applyDefaultColorTint` | 按 `0xRRGGBB` 颜色与强度缩放 8 个 RGB 关键帧 | `fxpscolortint` (`true`) |
| `0x0065E710` | `computeDefaultColorRate` | 计算相邻 RGB 关键帧间的每帧插值步长 | `fxpscolorrate` (`true`) |
| `0x0065E8B0` | `updateDefaultColorParticle` | 推进粒子颜色、跨越关键帧并钳制负通道 | — |
| `0x0065EB10` | `initializeDefaultUpdateModuleTemplateFromAsset` | 从资产初始化 `DefaultUpdateModuleTemplate` | — |
| `0x0065EC20` | `copyUpdateModuleTemplateBase` | 复制基础更新模块模板（生命周期/尺寸/旋转及 `shaderType`） | — |
| `0x0065EDF0` | `updateDefaultUpdateParticle` | 每帧更新标量尺寸、Z 轴旋转角与阻尼 | — |
| `0x0065EF50` | `stepCylinderVolumeRadiusOffset` | 累加圆柱发射体积 `currentRadius` | `fxpscylstepradius` (`true`) |
| `0x0065EF80` | `copyOrthoVelocityTemplate` | 复制正交发射速度模板 3 个 `RandomVariable` | `fxpsorthotmplcopy` (`true`) |
| `0x0065F040` | `computeOutwardEmissionVelocity` | 采样径向/法向速度并派发各体积类型的外向速度 | — |
| `0x0065F100` | `initializeWavefrontVelocityTemplateFromAsset` | 从资产初始化波前发射速度模板 | — |
| `0x0065F160` | `copyWavefrontVelocityTemplate` | 复制波前发射速度模板 | `fxpswavetmplcopy` (`true`) |
| `0x0065F1E0` | `sampleWavefrontVelocityDimensions` | 采样波前发射速度 4 个随机参数 | — |
| `0x0065F460` | `copyRenderObjectDrawTemplate` | 复制 `0x40` 字节 `RenderObjectDrawModuleTemplate` | `fxpsrodrawtmplcopy` (`true`) |
| `0x0065F530` | `initializeRenderObjectUpdateModuleTemplateFromAsset` | （延后，不计数）从资产初始化 `RenderObjectUpdateModuleTemplate` | — |
| `0x0065F690` | `copyRenderObjectUpdateModuleTemplate` | 复制三维模型更新模块模板 | — |
| `0x0065F900` | `updateRenderObjectUpdateParticle` | 每帧更新三维尺寸、三维旋转角与阻尼 | — |
| `0x0065FC70` | `initializeWindModuleTemplateFromAsset` | 从资产初始化风场模块模板 | — |
| `0x0065FCF0` | `copyWindModuleTemplate` | 复制风场模块模板 | — |
| `0x0065FD60` | `updateWindModuleMotion` | 更新 PingPong / Circular 风场角度并在边界重采样 | — |
| `0x00660530` | `getParticleSizeExtended` | 在 80 位 x87 `ST0` 返回粒子当前尺寸 | — |
| `0x00660D50` | `multiplyMatrix34` | 按列优先 SSE 顺序计算 `Matrix34` 乘法 | `fxpsmulmat34` (`true`) |
| `0x00660F80` | `extractTransposeRotation33` | 从 `Matrix34` 提取转置 `3x3` 旋转矩阵 | `fxpsextractrot33` (`true`) |
| `0x00661030` | `evaluateCubicHermiteSpline` | 计算三维三次 Hermite 样条插值点 | — |
| `0x00661370` | `initializeDefaultAlphaParticle` | 采样 8 个 Alpha 关键帧并计算初始步长 | — |
| `0x00661590` | `initializeDefaultColorParticle` | 采样颜色缩放并初始化粒子颜色关键帧与步长 | — |
| `0x00661B20` | `copyDefaultUpdateModuleTemplate` | 复制默认更新模块模板 | — |
| `0x00662990` | `copyTerrainFireTemplate` | 复制地形火焰发射模板 | — |
| `0x00662A00` | `initializeTerrainFireTemplateFromAsset` | （延后，不计数）从资产初始化地形火焰发射模板 | — |
| `0x00662A60` | `sampleTerrainFirePoint` | 采样地形火焰三维发射点 | — |
| `0x00662B60` | `packLightningParticleVertices` | 打包闪电粒子段顶点缓冲 | — |
| `0x00662CA0` | `initializeLightningDrawTemplateFromAsset` | （延后，不计数）从资产初始化闪电绘制模板 | — |
| `0x00662E10` | `copyLightningDrawTemplate` | 复制 `0xAC` 字节闪电绘制模板 | — |
| `0x00662F50` | `initializeDefaultLightningDrawTemplate` | 初始化默认闪电绘制模板 | — |
| `0x00664FA0` | `inverseTransformPointByMatrix34` | 用 `Matrix34` 逆变换三维点 | `fxpsinvmat34pt` (`true`) |
| `0x006650E0` | `updateDefaultAlphaParticle` | 更新粒子 Alpha 并在 `shaderType == 1` 时钳制到 `[0,1]` | — |
| `0x00665420` | `computeCylinderOutwardVelocity` | 计算圆柱体积外向发射速度 | `fxpscyloutwardvel` (`true`) |
| `0x00665540` | `computeLineOutwardVelocity` | 计算线段体积正交径向与法向发射速度 | — |
| `0x006657D0` | `computeSplineOutwardVelocity` | 计算样条体积切向与径向发射速度 | `fxpssplineoutwardvel` (`true`) |
| `0x00665B60` | `computeRadialOutwardVelocity` | 计算球/盒/点体积径向外向发射速度 | `fxpsradialoutwardvel` (`true`) |
| `0x00666D70` | `updateSwarmPhysicsParticle` | 向系统原点加速吸引粒子群速度与位置 | — |
| `0x00669060` | `buildEulerXYZMatrix33` | 由欧拉角构建 `3x3` 旋转矩阵 | `fxpseulermat33` (`true`) |
| `0x00669150` | `transposeMatrix33` | 用 x87 `fld/fstp` 转置 `3x3` 矩阵 | `fxpstransposemat33` (`true`) |
| `0x006699D0` | `initializeParticleBase` | 初始化 `FXParticleBase` 并递增引用计数与活跃粒子计数 | — |
| `0x00669AB0` | `initializeDefaultColorModule` | 从模板初始化 `DefaultColorModule` | — |
| `0x00669CA0` | `getDefaultColorScaleRange` | 读取 `DefaultColorModule` 随机缩放上下界 | — |
| `0x0066A740` | `initializeDefaultUpdateParticle` | 采样默认更新模块初始尺寸/旋转/阻尼参数 | — |
| `0x0066ABF0` | `initializeParticle` | 初始化 `FXParticle`、采样 `[0,15]` 随机相位并计算死亡时间戳 | — |
| `0x0066AE60` | `initializeLifeEventModule` | 采样并截断生命周期事件触发帧 | — |
| `0x0066AF10` | `initializeLightningDrawModule` | 映射着色器通道并采样 12 个闪电随机参数 | — |
| `0x0066BAE0` | `initializeRenderObjectUpdateParticle` | 采样三维模型更新模块初始三维尺寸/旋转/阻尼 | — |
| `0x0066CD60` | `initializeWindModule` | 初始化风场模块并采样初始 PingPong 角度区间 | — |
| `0x0066CE40` | `updateWindParticle` | 计算风场距离衰减与余弦湍流对粒子的作用力 | — |

## Hook 表

| Key | VA | Expected 16 Bytes | Stolen | explicitOnly |
|---|---|---|---:|---|
| `fxpsalpharate` | `0x0065E1E0` | `83 EC 10 8B 51 50 F3 0F 10 4C D1 0C 0F 57 C0 0F` | 6 | `true` |
| `fxpscolortint` | `0x0065E490` | `83 EC 0C 80 B9 9C 00 00 00 00 0F 84 F5 01 00 00` | 10 | `true` |
| `fxpscolorrate` | `0x0065E710` | `83 EC 10 0F 57 C0 56 8B B1 A0 00 00 00 8B C6 C1` | 6 | `true` |
| `fxpscylstepradius` | `0x0065EF50` | `8B 41 08 F3 0F 10 40 10 F3 0F 58 41 0C F3 0F 10` | 8 | `true` |
| `fxpsorthotmplcopy` | `0x0065EF80` | `8B C1 8B 4C 24 04 C7 00 60 2A BF 00 8B 51 04 89` | 6 | `true` |
| `fxpswavetmplcopy` | `0x0065F160` | `8B C1 8B 4C 24 04 C7 00 60 2A BF 00 8B 51 04 89` | 6 | `true` |
| `fxpsrodrawtmplcopy` | `0x0065F460` | `8B C1 8B 4C 24 04 C7 00 E0 30 BF 00 0F B6 51 04` | 6 | `true` |
| `fxpsmulmat34` | `0x00660D50` | `8B 44 24 04 8B 54 24 08 F3 0F 10 52 20 F3 0F 10` | 8 | `true` |
| `fxpsextractrot33` | `0x00660F80` | `83 EC 24 8B 44 24 2C F3 0F 10 40 08 F3 0F 11 44` | 7 | `true` |
| `fxpsinvmat34pt` | `0x00664FA0` | `8B 54 24 0C 8B 4C 24 08 83 EC 18 3B D1 75 18 8B` | 8 | `true` |
| `fxpscyloutwardvel` | `0x00665420` | `8B 44 24 08 F3 0F 10 08 F3 0F 10 40 04 0F 28 D1` | 8 | `true` |
| `fxpssplineoutwardvel` | `0x006657D0` | `83 EC 0C 0F 57 E4 F3 0F 10 59 24 F3 0F 10 49 28` | 6 | `true` |
| `fxpsradialoutwardvel` | `0x00665B60` | `83 EC 0C 8B 44 24 14 F3 0F 10 48 04 F3 0F 10 50` | 7 | `true` |
| `fxpseulermat33` | `0x00669060` | `83 EC 14 8B 44 24 1C D9 00 F3 0F 10 25 08 C0 BD` | 7 | `true` |
| `fxpstransposemat33` | `0x00669150` | `8B 4C 24 08 8B 44 24 04 56 83 C1 18 8D 50 08 BE` | 8 | `true` |

## 浮点、随机流与推迟项说明

- **x87 / SSE 混合精度与指令顺序**：严格保留原二进制的 x87 80 位栈运算（如 `fcos`/`fsin`、`fild` 无符号 32 位整数修正 `fadd 2^32`、`fistp` 向零截断控制字 `or ah, 0x0C`）和 SSE 标量运算顺序（如 `Matrix34::multiply` 第 2 列与其余列不同的累加顺序）。
- **Client RandomVariable 流同步**：所有调用 `0x005A96A0`（`RandomVariable::getValue`）和 `0x005A95A0`（`GetClientRandomValueReal`）的函数均同时验证输出结构与 `0x00CB73F0` 处 24 字节客户端随机流状态。
- **Hook 安全策略**：仅对 15 个无全局随机流副作用、无引用计数递增、写集完全自包含的函数注册 `explicitOnly = true` 的 Hook 目标。
- **推迟项（Deferred）**：`0x0066CDE0` 等内存池分配包装器（调用 `0x0041C160`）、`0x006658A0`（依赖外部样条曲线缓存容器）以及 `0x00666710`/`0x00666980`（粒子系统管理器链表与完整发射管线）记入未决项，留待后续批次处理。
