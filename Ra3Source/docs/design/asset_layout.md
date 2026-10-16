# RA3 编译资源实例布局

## 1. 范围与证据

本页记录 `BinaryAssetBuilder` 从 XSD 源模型生成 32 位实例块的已证实规则，以及
`tools/analysis/xsd_layout.py` 对发行版全部资源的验证结果。工具只读
`D:\Game\Ra3\Game\Data` 中 SkuDef 挂载的 14 个 BIG，不运行游戏，也不修改或整包
复制游戏数据。

主要参考是 EA 以 GPL v3 发布的 Mod SDK（仓库外
`work/references/modding/Red Alert 3`）：

- `Schemas/Base/AssetBase.xsd`：资源基类、AssetReference、WeakReference、Poid、
  FileReference、DataBlob；
- `Schemas/Includes/Base.xsd`：数值、向量和 XSD list 基础类型；
- `Schemas/Includes/Ref.xsd`：强/弱资源引用及其目标类型；
- `Schemas/AssetTypeFontSubstitution.xsd`、`AssetTypeLanguage.xsd`、
  `AssetTypeStringHashTable.xsd`、`AssetTypeAptAssets.xsd`、
  `AssetTypeTexture.xsd`、`AssetTypeAudio.xsd`：首批布局证据；
- `Schemas/AssetType*.xsd` 及其 include：全类型模型。

没有把 XSD 或 SDK 源文件复制进仓库。本文的“已证实”是 XSD 结构和发行字节共同
成立；仅由 XSD 推测但尚未通过实例的内容仍列为未决项。

## 2. 已证实的通用规则

发行实例使用小端 32 位 ABI，当前所有已验证字段按 4 字节边界排布。

| XSD 构造 | 编译表示 | 证据 |
|---|---|---|
| 资源根对象 | 首 4 字节为 runtime-type/token 槽，随后是资源字段 | `FontSubstitution`、`Language`、`FontDirectory` 等 |
| `xs:extension` | 基类字段在前，派生类字段在后 | 多层复杂类型实测 |
| 同一 complexType | attributes 在前，sequence/choice 元素在后 | `FontSubstitution` 根和 `Subtitution` 子对象 |
| `xs:string` | `{u32 length, u32 relativePointer}`；长度不含 NUL | `FontSubstitution.Name`、`Language` |
| `FileReference` | 单个 NUL 字符串相对指针 | Xml typeGroup 的发行实例 |
| `DataBlob` | `{u32 relativePointer, u32 byteSize}` | Apt 三种数据资产 |
| 标量、枚举、bool | 通常为 4 字节；显式 byte/short 保留 1/2 字节，再按下一字段对齐 | XSD primitive 与实例尺寸 |
| 固定单个 complex element | 内联；递归使用同样的 attribute-first 规则 | `SeparatorList`、`FontDesc` |
| 可选 complex element | 通常是可空的单指针；非空槽进 `.relo`，目标仍按 complex 布局 | 多类可选配置对象全量回归 |
| `maxOccurs != 1` / `unbounded` | `{u32 count, u32 relativePointer}`，元素连续存放 | `FontSubstitution.Substitute`、`Language.FontInfo` |
| 资源引用 | 4 字节槽；非零值是从 1 开始的引用序号，位置必须在 `.imp` | 多个已通过类型 |
| pointer/blob/list | 指针是实例起点相对偏移；非空槽必须出现在 `.relo` | 全部通过实例 |
| `pipelineOnly="true"` | 不进入运行时布局 | StringHashTable、Texture |
| BaseAsset XML 元数据 | `id/typeHashCode/buildRule/inheritFrom` 不作为普通运行时字段落盘 | 所有简单资源根 |
| 零长记录 | 表示由依赖流提供的外部占位；其 `.relo/.imp` 必须也为空 | 跨 static/map/W3D 流全量实测 |

`FontSubstitution` 是最清楚的闭环样本。根布局为：

```text
0x00 runtime token
0x04 Name.length
0x08 Name.pointer
0x0C Substitute.count
0x10 Substitute.pointer
sizeof = 20
```

每个 `Subtitution` 为 20 字节：`Size` 4 字节，随后内联 16 字节
`FontDesc`（字符串、字号、bool）。五个发行实例的根大小、所有嵌套字符串和数组
边界、全部 relo 位置均吻合。

有两项是 typeGroup 的专用转换而不是普通 XSD 映射：

- `Texture` 的源 `FileReference` 被编译为 `{pointer,size}` DDS blob，根大小 12；
  13,835 条记录全部通过。
- `AudioFile` 使用同文件的 `AudioFileRuntime` 语义；发行数据证明根为 32 字节：
  subtitle 字符串、sample count/rate、header blob、channel count。11,920 条全部通过。

第二轮又证实了以下编译转换：

- 通用 attribute-first 规则有少量 runtime-wrapper 顺序例外：三种 texture/radar
  image 的 `Rotated`、`W3DCollisionBox.JoypadPickingOnly` 和
  `BuildSystemSettings.BuildabilityUsesStaticDecalSize` 位于后续内联/标量字段
  之后；严格 bool 检查已对全部发行实例确认这些偏移。
- `xas:tag` 的 `Angle/Percentage/Time/Duration/Velocity/StringHash/StickyString`
  虽以 XML 字符串为载体，落盘都是 4 字节运行时值；不能沿 `xs:string`
  restriction 推成字符串指针。
- `xs:list` 的 item 为枚举时编译为 32 位 bit flags；普通 primitive/string list
  仍为 `{count,pointer}`。
- 单选 choice 的多个 `AssetReference` 分支共用一个 4 字节引用槽。
  `W3DContainer.RenderObject` 因而没有额外 discriminant，目标类型由 manifest
  引用目录中的 `(typeId,instanceId)` 给出；7,014 条全部通过。
- 重复多态 choice 是 `{count,pointer}` 指向一个**指针数组**；根列表指针和每个
  数组项都进 `.relo`。数组项所指对象首 `u32` 是具体 XSD 类型名的 typeId，
  token 本身不进 `.relo`。例如 `W3DAnimation` 的
  `0x60D8CF69/0x77F97305` 分别是
  `AnimationChannelTimecoded/AnimationChannelDelta`。
- `Poid` 是 4 字节对象内 token，不是 C 字符串或资源导入，也不进普通
  `.relo`。`W3DHierarchy` 的发行非空实例只把根数组指针列入 `.relo`；
  6,946 条全部通过，推翻了上一版“Poid 是 detokenizer relocation”的假设。
- `Tokenized` typeGroup 会把普通字符串递归收缩为 4 字节 token/hash；这已修正
  `GameObject/WeaponTemplate` 的静态偏移，但两类仍有模块专用布局，尚未全量通过。
- `FXParticleSystemTemplate` 的根是 168 字节专用布局。clean 函数
  `0x00673A50` 直接读取 `+0x80..+0xA4` 的模块槽并按 typeId 构造运行时模块；
  当前根偏移已吻合，嵌套模块仍有未解差异。

第三轮闭环了三种专用 runtime wrapper：

- `AudioEvent` 的根是 156 字节。Attack/Sound/Decay 仍是三个独立列表，但从
  `+0x80/+0x88/+0x90` 开始，每项固定 12 字节
  `{AudioFile import, weight, volume}`；非空列表指针恰为
  `+0x84/+0x8C/+0x94` 的 relo，全部 item import 从 `+0x98` 起按 12 字节步进。
  range 指针指向紧凑运行时值；66 个 PitchShift 目标位于实例最后 4 字节，证明
  不能强制套用源 XSD 的固定 8 字节 `RealRange`。
- `W3DAnimation` 的 Timecoded 通道为 36 字节、Delta 通道为 44 字节，均含开头
  typeId。clean 构造函数 `0x009AF0F0` 直接读取 Timecoded 的 pivot `+0x10`、
  两个列表 `+0x14/+0x18` 和 `+0x1C/+0x20`；`0x009AD5A0` 读取 Delta 的
  scale `+0x14`、init `+0x18/+0x1C`、numBits `+0x20`、压缩字节
  `+0x24/+0x28`。35 个空 channel 数组使用合法的一过尾指针 24，与全量
  3,250 个一过尾 relo 目标的不变量一致。
- `FXParticleSystemTemplate` 的模块槽是指向“typeId + concrete payload”的多态
  指针，Event 是多态指针数组；这与 `0x00673A50` 的分派完全一致。
  1,291 个 Color 块的列表指针全部位于块 `+8`，RenderObject draw 模块的
  163 个 ObjectSet 指针全部位于模块 `+0x0C`。6 个 EmitterSound 使用根
  `+0x28` 指针指向单独的 import cell；7 个非零粒子系统弱引用是 token，
  不在 `.imp`。上述槽集合与 4,110 条记录的 relo/imp 完全相等。

### 2.1 程序中的 relocation / detokenizer 边界

发行字节与 clean 函数共同表明，流程不是“在同一个槽里区分相对指针和 token”：

1. `.relo` 只登记相对**指针槽**；装载阶段把槽值加实例基址。
2. 若该指针指向多态对象，目标对象首字才是 typeId token。
3. 类型专用运行时构造器读取 token、分配真实对象并调用对应复制构造。

`0x009B15A0` 遍历已重定位的 W3D 通道指针数组，`0x009B0540` 完成两种运行时
通道工厂分派；`0x00674080 -> 0x00673A50` 是粒子系统的同类路径。
`ReloDetokenizers.h:127` 对应的
`0x0043C630/0x0045E9A0/0x00477510/0x00477A50` 是未知 token 的致命诊断与
清理实例。`0x00447E30` 会把 12 字节 typed-fixup 描述符中的相对偏移加实例基址
后按类型分派，但它不是原始 `.relo` 数组循环。

原始逐项 `.relo` 循环位于 `ResourceManagerFileStream` 周边未恢复入口中；
相关台账项为 `entry-lost`，本轮按 SecuROM 规则停止。可确认的 clean 边界是
`0x00447D70`（记录分配/委托）和 `0x004479E0`（打开三成员并校验 checksum）。
因此逐项循环的精确入口地址仍是未决项，不能越过台账边界猜定。

## 3. 工具与验证方法

`xsd_layout.py` 加载 821 个 XSD 文件，建立 complex/simple type、extension、
attribute、element、匿名类型、enum、XSD list、choice 和引用类型目录。当前读到
1,870 个命名/匿名类型、314 个带 `xas:typeGroup` 的候选资源类型；发行清单实际
出现 257 类。

对每条 manifest 记录执行：

1. 按记录尺寸从 `.bin/.relo/.imp` 精确切块；未压缩的 1.17 GiB worldbuilder
   成员使用 BIG 内随机只读，不整体载入。
2. 检查实例至少容纳预测根布局；零长外部占位单独检查。
3. 从根递归遍历内联对象和 list 指向对象；字符串、blob、数组的全部范围必须留在
   当前实例内。
4. 从布局得到预期 pointer/reference 槽集合；要求每个非空 pointer 有且仅有
   `.relo` 项，每个非空引用有且仅有 `.imp` 项。
5. 引用槽值必须是合法的 1-based asset-reference 序号。
6. 每个流结束时再次检查四件套累计块长恰好等于成员长度。

输出：

- `docs/analysis/ra3_1.0/asset_layouts.tsv`：静态根/内联字段路径、偏移、尺寸和 kind；
- `docs/analysis/ra3_1.0/asset_layout_coverage.tsv`：逐类型实例数、通过数、根大小和
  主要失败原因。

### 3.1 当前覆盖

第三轮全量结果（2026-10-17）：

| 指标 | 结果 |
|---|---:|
| 四件套流 | 257 / 257 |
| 发行资源类型完全通过 | **160 / 257** |
| 发行 manifest 记录通过 | **101,583 / 118,232** |
| 未通过记录 | 16,649 |

相对第二轮的 **157 / 257、93,200 / 118,232**，增加 3 个完全通过类型和
8,383 条通过记录。原有 157 个完全通过类型没有回归。

这里“类型完全通过”要求该类型每条发行记录（包括零长外部占位）都通过；“记录通过”
不等于已经理解其上层游戏语义，只表示本文列出的二进制布局、边界、relo 和 imp
不变量全部成立。

## 4. 主要失败类型

完整逐类型原因见 coverage TSV。按未通过记录数排序的主要缺口：

| 类型 | 通过 / 总数 | 原因 |
|---|---:|---|
| W3DMesh | 12,033 / 17,058 | `W3D` 顶点块是专用压缩/指针目录，且 FXShader 内部仍有运行时差异 |
| GameObject | 8,511 / 12,280 | Tokenized 字符串已收缩，但 Browser、GameDependency 和模块集合仍是专用表示 |
| ObjectCreationList | 901 / 1,787 | 56 字节根已吻合；CreateObject nugget 的可选 Vector/引用列表并非通用表示 |
| AIStrategicStateDefinition | 0 / 704 | Tactic/Heuristic 的嵌套多态与可选对象仍未建模 |
| VideoEventList | 0 / 690 | Movie/TitleCard 多态事件的运行时对象布局未解 |

本轮优先类型的前后对比：

| 类型 | 第二轮 | 第三轮 |
|---|---:|---:|
| AudioEvent | 46 / 5,234 | **5,234 / 5,234** |
| W3DMesh | 12,033 / 17,058 | 12,033 / 17,058 |
| GameObject | 8,511 / 12,280 | 8,511 / 12,280 |
| W3DAnimation | 4,099 / 5,924 | **5,924 / 5,924** |
| FXParticleSystemTemplate | 2,740 / 4,110 | **4,110 / 4,110** |
| ObjectCreationList | 901 / 1,787 | 901 / 1,787 |

这些失败不是越界后继续“猜读”；验证器在第一层不一致时记录失败。TSV 中部分
大类型的通过数来自合法零长依赖占位，不能据此声称其非空 runtime 布局已解决。

## 5. 再生

从 `Ra3Source` 所在 worktree 根目录运行：

```powershell
python Ra3Source/tools/analysis/xsd_layout.py `
  --schemas "D:/Game/Ra3/RevRa3/work/references/modding/Red Alert 3/Schemas" `
  --validate --progress
```

默认读取 `D:\Game\Ra3\Game\Data`，并覆盖两份分析 TSV。可用 `--data-dir`、
`--coverage`、`--layouts` 指定其他只读输入和输出。当前机器全量运行约 4 分钟，
耗时主要来自纯 Python RefPack 解压 320 MiB 的 `static.bin`。

只检查 XSD 解析和预测根尺寸（不读游戏数据）：

```powershell
python Ra3Source/tools/analysis/xsd_layout.py `
  --schemas "D:/Game/Ra3/RevRa3/work/references/modding/Red Alert 3/Schemas"
```

## 6. 未决问题与下一步

1. `W3DMesh` 的 VertexData 专用块和 FXShader 常量数组仍需分别建模，不能套普通
   XSD list。其 5,025 个非占位实例全部保留失败。
2. `ObjectCreationList` 的 248 字节 CreateObject runtime nugget 和其他 nugget
   仍有专用可选 Vector/token 列表；在字段语义闭环前不以 opaque 槽放宽验证。
3. `GameObject/WeaponTemplate` 的 Tokenized 不只影响字符串，还会重写模块集合和
   若干 tagged 数值；当前递归字符串 token 规则只是已证实子集。
4. 原始 `.relo` 逐项循环的精确入口落在台账 `entry-lost` 边界内；后续只有出现
   新的 clean 调用边界时才能继续，不能分析 SecuROM thunk/body。
5. worldbuilder 流中的少量系统/UI 类型与另外两份平台流根尺寸不同，可能是
   runtime wrapper 或 schema 版本差异；在找到版本选择依据前保留为失败。
6. `typeHash` 很可能可用于选择精确 layout 版本，但其生成算法仍未证实。

