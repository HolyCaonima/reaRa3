# RA3 编译资源系统设计草案

## 1. 范围、证据与术语

本文只设计 `Ra3::asset`：读取 BIG 档案中的编译资源流，解析 `.manifest/.bin/.relo/.imp`，建立可查询的资源目录，并为以后按资源类型实例化对象提供稳定边界。它不实现渲染、音频、游戏逻辑或 XML 编译器，也不改写游戏数据。[推断]

证据标签：

- **[XSD]**：来自 `work/references/modding/Red Alert 3/Schemas` 的 RA3 XSD。
- **[Generals]**：来自 GPL v3 发布的 Generals `ArchiveFile*` 或 `W3DAssetManager*` 源码。
- **[实测]**：由现有 `ra3tool` 对只读安装 `D:\Game\Ra3\Game` 查询，或对档案指定范围只读 hexdump。
- **[全量]**：对 SkuDef 挂载的 14 个档案里全部 257 组四件套逐组校验成立（校验脚本见 §3.5）。
- **[程序]**：反编译 `ra3_1.0.game` 得到，给出函数地址。
- **[推断]**：当前证据支持但尚未由完整解析器或原程序代码确认。

“资源类型数量”在本文特指文件名匹配 `AssetType*.xsd` 的 schema 文件数：共 **114** 个，其中 `Schemas` 根目录 105 个、`Schemas/SkirmishAI` 9 个。[XSD] 一个 schema 文件可能声明多个运行时资源类型（例如 `AssetTypeAudio.xsd`、`AssetTypeW3D.xsd`），所以 114 不是运行时 type hash 的数量。[XSD]

XSD 的公共模型是：资源至少有 `id`，可有 `typeHashCode`、`buildRule`；可继承资源另有 `inheritFrom`。`AssetReference` 是强资源引用，`WeakReference` 和 `Poid` 是需要后置修复、可处理循环依赖的弱引用；`FileReference` 指向外部文件，`DataBlob` 要把外部文件内容拉入产物。[XSD] 这直接要求加载器至少分成“登记资源”和“解析引用”两个阶段。[XSD][推断]

本文初稿时函数台账里没有 `asset` 归属的行，原因是导出脚本漏掉了断言里 `文件.cpp:行号` 形式的源路径。修正后台账里有 9 个函数归到 `asset`，§3.4 列出了已读懂的那几个。[程序]

## 2. AssetType 目录

下面覆盖全部 114 个 `AssetType*.xsd` 文件。字段是用于建立首版类型目录的代表性摘要，不是完整字段清单。[XSD]

| 领域 | AssetType schema | 代表性内容与字段 |
|---|---|---|
| 核心对象与战斗 | `GameObject`、`WeaponTemplate`、`ArmorTemplate`、`DamageFX`、`AttributeModifier`、`ObjectCreationList`、`ObjectFilter`、`LocomotorTemplate`、`InvisibilityTemplate`、`ProjectilePath`、`SpecialPowerTemplate`、`UpgradeTemplate`、`ExperienceLevelTemplate`、`StanceTemplate`、`EmotionTemplate`、`FormationAssistantTemplate`、`RegisteredObjectSet`、`RedAlert`、`LocalBuildListMonitor`、`ConnectionLineManager` | 对象的显示名、阵营、KindOf、造价/建造时间、能量、几何、视野、装甲/武器/移动引用、Draw/Behavior/AI/Body 模块；武器射程、射速、弹匣、声音/FX、目标掩码和多态 Nugget；伤害、状态、过滤器、升级与特殊能力。[XSD] |
| 命令、科技与玩家 | `CommandSet`、`LogicCommand`、`LogicCommandSet`、`PlayerTemplate`、`PlayerTech`、`PlayerTechRank`、`PlayerTechUpgradeBinding`、`PlayerPowerButtonTemplate`、`UnitAbilityButtonTemplate`、`UnitTypeIcon`、`UnitIntro`、`UnitOverlayIconSettings` | 命令按钮及命令集合、对象/升级/特殊能力引用、科技等级和解锁绑定、阵营和玩家初始数据、按钮图像/文本/热键/状态、单位图标和介绍信息。[XSD] |
| Skirmish AI | `AIData`；`SkirmishAI/ArmyDefinition`、`AIBuilderDefinition`、`AIMicroManager`、`AIMicroManagerStates`、`AIPersonalityDefinition`、`AISpecialPower`、`AIStateDefinition`、`AITeamTemplate`、`TargetHeuristics` | 阵营的 MCV/采集/生产/维修模板，建造规则和预算，微操状态与任务，队伍招募范围和优先级，特殊能力启停条件，目标过滤、距离、威胁、位置与排序启发式。[XSD] |
| 渲染模型与材质 | `W3D`、`Texture`、`PackedTexture`、`OnDemandTextureImage`、`MeshInstanceTemplate`、`FXParticleSystem`、`FXList`、`ShaderOverride`、`ShadowMap`、`DrawGroupInfo`、`ImageSequence` | W3D 网格顶点/法线/切线/UV/三角形/AABTree、层级骨骼、动画通道和碰撞盒；纹理文件、类型、mipmap、输出格式；图集区域、按需图像、实例化、粒子模块、FX 动作、shader 常量和阴影参数。[XSD] |
| 地形、道路、天气与水 | `TerrainAsset`、`TerrainTextureAtlas`、`Roads`、`RadarTerrainImage`、`Environment`、`Weather`、`WaterSetting`、`WaterTextureList`、`WaterTransparency` | 地形模型/纹理集合和 atlas 区域，道路点及材质，雷达地形图，环境纹理，云雨雪/风/闪电，水颜色、法线/纹理序列、透明度和距离参数。[XSD] |
| 音频与音乐 | `Audio`、`LargeGroupAudio`、`PathMusic`、`MusicScript`、`CrowdResponse` | 音频文件的采样率、声道、样本数、压缩和流式标志；AudioEvent/MusicTrack/Dialog/Ambient/Multisound 的文件引用、音量、音高、范围、优先级、循环与限制组；路径音乐状态、转场、条件树；群体和人群响应映射。[XSD] |
| 视频、Apt 与启动内容 | `Video`、`VideoSequence`、`AptAssets`、`BootupDisplaySequence` | 视频文件、音轨/字幕和播放事件；视频事件序列； Apt 的 `.apt/.const/.dat/geometry` `DataBlob` 与 `AptID`；启动画面/视频的顺序和时长。[XSD] |
| 地图、战役与脚本内容 | `GameMap`、`MapMetaData`、`CampaignManager`、`GameScriptList`、`TutorialData`、`DLContent`、`EngagingContent`、`AchievementData`、`GameplayAchievementTrigger`、`GameplayCondition`、`IntelDatabase` | 地图 waypoint、单位/建筑/道具、位置/角度/队伍、道路、环境、属性字典和地图分析；地图名称/人数/预览等元数据；战役任务、教程步骤、脚本列表、DLC 门控、成就触发条件和情报条目。[XSD] |
| UI 布局与表现 | `UISettings`、`UIConfigList`、`HeaderTemplate`、`HealthBarTemplate`、`ButtonState`、`BasicMouseCursors`、`Credits` | UI 配置项及引用、标题/生命条样式、按钮状态图像和颜色、基础光标集合、制作人员条目和滚动显示参数。[XSD] |
| 输入与热键 | `UIMouseSettings`、`UIJoypadSettings`、`JoypadCommandBarTemplate`、`JoypadCommandBarButtonTemplate`、`Mouse`、`MouseCursor`、`MappableKey`、`HotKeySlot`、`GeneralHotKeySlots`、`DefaultHotKeys`、`DebugHotKeys`、`ForceFeedbackEvent` | 鼠标/手柄灵敏度和行为、命令栏布局与按钮、光标纹理和热点、可映射键/槽/默认绑定/调试绑定、力反馈持续时间和强度。[XSD] |
| 本地化、字体与哈希表 | `Language`、`FontDirectory`、`FontSettings`、`FontSubstitution`、`FontTranslation`、`StringHashTable`、`StringHashEnum` | 语言名称/区域和字体选择，字体文件/字号/样式、替换与字符映射，字符串到整数或枚举值的表。[XSD] |
| 多人和在线显示 | `MultiplayerSettings`、`MultiplayerColor`、`OnlineChatColors` | 多人规则和统计模式、玩家颜色及 RGB 值、聊天频道/关系对应颜色。[XSD] |
| 全局游戏/UI 数据 | `TheVersion`、`AssetReport`、`PipelineTestType`、`UIConfigList` 中的配置资产 | 版本值、资源报告项、管线测试用纹理/集合/对象，以及全局配置集合。[XSD] `PipelineTestType` 是工具/测试 schema，不应默认当作发行版必有运行时类型。[XSD][推断] |

特别复杂的 schema 示例：

- `GameObject` 本身声明约 300 种可选 Behavior 模块，并组合 Draw、AI、Body、ClientUpdate、音频、视野、碾压、建造和经济字段；首版加载器不应把它硬编码进通用四件套解析层。[XSD]
- `WeaponTemplate` 包含 Damage、DOT、Projectile、Suppression、OCL、Laser、Paralyze、InformationWarfare、AttributeModifier 等多态 Nugget，以及大量资源引用。[XSD]
- `W3D` 同时定义 Mesh、Container、Hierarchy、Animation、CollisionBox 等资源；`Audio` 同时定义 AudioFile、AudioEvent、MusicTrack、DialogEvent、AmbientStream、Multisound 等资源。[XSD]
- `Texture` 的 `File`、mipmap 和输出格式多为管线字段，而运行时 wrapper 是 `RNA::TextureC` 或按需 loader；通用加载器必须保留“schema 字段”和“运行时落盘字段可能不同”的区分。[XSD]

## 3. 四件套模型

### 3.1 实测基线

发行版 14 个 BIG 共 15,329 个条目；既有报告统计到 257 组四件套。[实测] 本次抽查进一步确认：

- `StaticStream.big` 有 `data/static.{bin,imp,manifest,relo}`，大小分别为 145,969,504、106,072、476,438、563,466 字节，并有大量 `.cdata`。[实测]
- `GlobalStream.big` 有 `global` 和 `locale` 两组；`locale` 四件套大小分别为 860、60、1,772、104 字节。[实测]
- `MapsTutorial.big` 的 6 张地图各有 `map.{bin,imp,manifest,relo}`。[实测]
- `Apt.big` 按 UI 名称重复出现四件套；部分 `.imp` 只有 4 字节，而 `.bin` 可达数 MB。[实测]

压缩是“每个成员独立”的外层传输属性，而不是某种扩展名的固定属性。[实测] `GlobalStream.big` 中 `global.bin/.manifest/.relo` 以 RefPack `10 FB` 开头，`global.imp` 未压缩；`locale` 四件套均未压缩。[实测] 因此处理顺序必须是“从 `Ra3::io` 读取成员 → 对该成员单独探测/解 RefPack → 再交给四件套解析器”。[实测][推断]

未压缩 `locale` 样本给出以下可重复观察：

- `.bin`、`.imp`、`.relo` 都以小端 `0xDC63FC0D` 开头；`.manifest` 在四字节 `00 01 06 00` 后出现同一个值。[实测]
- `.bin` 随后出现对齐整数、偏移和明文 `Arial`、`Lucida Sans Unicode`，符合实际对象数据与字符串池。[实测]
- `.relo` 在公共标识后主要是 4 字节对齐的小整数/偏移和 `0xFFFFFFFF` 哨兵。[实测]
- `.imp` 在公共标识后主要是 `4` 与 `0xFFFFFFFF` 的重复项；较大的未压缩 `global.imp` 在公共标识后出现计数样值和单调递增、4 字节对齐的偏移序列。[实测]
- `.manifest` 包含公共标识、多个计数/大小/偏移样值及高熵 16 字节值；精确布局见 §3.2。[实测]

`0xDC63FC0D` 是 `locale` 流的**流校验和**（stream checksum），见 §3.2。[全量][程序]

### 3.2 已证实的格式

全部为小端。发行版 257 组全部是 `version = 6`、`isBigEndian = 0`、`isLinked = 1`，共 118,232 条资源记录。[全量]

**`.manifest`** = 48 字节头 + `assetCount` × 48 字节记录 + 资源引用缓冲 + 被引用清单缓冲 + 资源名缓冲 + 源文件名缓冲。四个缓冲按此顺序紧邻，总长精确等于文件长度。[全量]

| 头偏移 | 类型 | 字段 | 说明 |
|---|---|---|---|
| 0x00 | u8 | isBigEndian | 发行版全为 0 |
| 0x01 | u8 | isLinked | 发行版全为 1 |
| 0x02 | u16 | version | 发行版全为 6 |
| 0x04 | u32 | streamChecksum | 与 `.bin/.relo/.imp` 的首 4 字节相同 [全量][程序] |
| 0x08 | u32 | allTypesHash | 语义未证实 |
| 0x0C | u32 | assetCount | |
| 0x10 | u32 | totalInstanceDataSize | 194 组等于 Σ instanceDataSize；63 组带 kind-2 引用的地图流更大，语义未知 [全量] |
| 0x14 | u32 | maxInstanceChunkSize | |
| 0x18 | u32 | maxRelocationChunkSize | |
| 0x1C | u32 | maxImportsChunkSize | |
| 0x20 | u32 | assetReferenceBufferSize | |
| 0x24 | u32 | referencedManifestBufferSize | |
| 0x28 | u32 | assetNameBufferSize | |
| 0x2C | u32 | sourceFileNameBufferSize | |

资源记录（原程序 `FullAssetList.cpp` 按 `count * 0x30` 分配，并从 +0x20/+0x24/+0x28 累加三个尺寸 [程序]）：

| 偏移 | 字段 | 说明 |
|---|---|---|
| 0x00 | typeId | 编译期常量，如 `FontSubstitution` = `0x057786F9`；同一值出现在原程序的类型处理器注册表和类型工厂 `switch` 里 [程序] |
| 0x04 | instanceId | 实例名的哈希，算法未知（见 §3.6） |
| 0x08 | typeHash | 类型布局哈希，原程序把它和 typeId 一起注册（如 `.rdata:00BC80E8` = `0xA63E49F9`）[程序] |
| 0x0C | instanceHash | 语义未证实 |
| 0x10 | assetReferenceOffset | 资源引用缓冲内的**字节**偏移 |
| 0x14 | assetReferenceCount | 每项 8 字节 `(typeId, instanceId)` |
| 0x18 | nameOffset | 资源名缓冲内偏移，NUL 结尾，形如 `FontSubstitution:Arial` |
| 0x1C | sourceFileNameOffset | 源文件名缓冲内偏移，形如 `DATA:globaldata/fontsubstitution.xml` |
| 0x20 | instanceDataSize | 在 `.bin` 中的块长 |
| 0x24 | relocationDataSize | 在 `.relo` 中的块长 |
| 0x28 | importsDataSize | 在 `.imp` 中的块长 |
| 0x2C | flags | 语义未知：全部 `GameObject`/`WeaponTemplate`/`ArmorTemplate` 实例为 1，其余为 0（同为可继承类型的 `LocomotorTemplate` 也是 0）[全量] |

**被引用清单缓冲**是 `[u8 kind][NUL 结尾的清单名]` 序列，只见过 kind 1 和 2。kind 1 是依赖流，例如 `static` 引用 `audio.manifest`、`global.manifest`；kind 2 只出现在 63 个地图流里，都指向 `worldbuilder.manifest`。原程序只对 kind 2 递归加载（`FullAssetList` `0x00495890`）。[全量][程序]

**`.bin/.relo/.imp`** 都是 4 字节流校验和，后面按记录顺序拼接各资源的块：第 i 个资源的实例数据位于 `.bin` 偏移 `4 + Σ instanceDataSize[0..i)`，`.relo`、`.imp` 同理。因此三者长度分别等于 `4 + Σ` 对应尺寸。校验和不符时原程序报 `... file is not in synch with manifest!`（`ResourceManagerFileStream.cpp`，`0x004479E0`）。[全量][程序]

**重定位块**：u32 数组，以 `0xFFFFFFFF` 结尾；每项是实例内一个 4 字节对齐字段的偏移，该字段存的是相对实例起点的偏移，装载时加上实例基址变成指针。例：`FontSubstitution:Arial` 的重定位为 `8, 0x10, 0x24, 0x38`，偏移 8 处的值 `0x14` 正指向实例内的 `"Arial"`。在 `global` 的 11,253 条资源上，所有重定位位置和目标都落在实例内。[实测]

**导入块**：同样是以 `0xFFFFFFFF` 结尾的实例内偏移数组。第 k 个导入位置在文件里存的值是 k（从 1 开始），对应该资源引用列表里的第 k 项，装载时换成被引用资源。`global` 里每条资源的导入数都等于引用数；地图流里有 237 条资源（如 `Road:*`）引用多于导入，多出的引用只作加载依赖。[实测]

**字符串与列表**在 `FontSubstitution` 样本的实例数据里分别编码为 `{u32 长度, 偏移}` 和 `{u32 数量, 偏移}`，偏移都在重定位表里；其他类型尚未逐一核对。[实测]

**`.cdata`**：原程序格式串为 `%s%08X.%08X.%08X.%08X.cdata`（`0x004A64CC`）[程序]；实际文件名是 `typeId.typeHash.instanceId.instanceHash`，放在与流同名目录的 `cdata/` 下，例如 `OnDemandTexture:IndividualLocalised_001` 对应 `data/locale/cdata/9312b9ac.f8488ede.01f402c3.91a9c787.cdata`。[实测]

### 3.3 Generals 参照能证明什么

Generals `ArchiveFile`/`ArchiveFileSystem` 把档案层限制为“按规范化路径索引、查询和打开字节流”；目录树会转小写，`openFile` 在多个档案中查找。[Generals] 这支持让 RA3 的 `Ra3::io` 负责 BIG/路径/读取，让 `Ra3::asset` 只负责四件套语义，而不是在 asset 模块重复档案实现。[Generals][推断]

Generals `W3DAssetManager` 把名称转小写作为缓存键，按需加载 W3D/纹理，维护 prototype/texture cache 和引用计数。[Generals] 这支持 RA3 的资源层提供稳定身份、缓存和按需访问接口，但它不是 RA3 四件套解析器，不能用来证明 manifest/relo/imp 的具体布局。[Generals]

### 3.4 原程序锚点

以下函数已读懂并记入 `docs/analysis/ra3_1.0/ledger_annotations.tsv`。[程序]

| 地址 | 源文件 | 作用 |
|---|---|---|
| `0x00495890` | `assetmanager\AssetList\FullAssetList.cpp` | 读清单：按 `count * 0x30` 分配记录、累加三个尺寸、对 kind-2 被引用清单递归 |
| `0x004479E0` | `assetmanager\Stream\ResourceManagerFileStream.cpp` | 换扩展名打开 `.imp/.relo/.bin`，比较首 4 字节与清单校验和 |
| `0x00417220` | `assetmanager\resourcemanager.cpp` | 注册资源类型处理器 `{虚表, typeId, typeHash}`，重复注册报 `Handler for resource type ... added twice`；约 160 个静态初始化函数调用它，构成原程序的资源类型目录 |
| `0x004A64CC` | `assetmanager\resourcemanager.cpp` | 生成 `.cdata` 文件名 |
| `0x00416E60` | `assetmanager\AssetList\PartialAssetList.cpp` | 报 `Patch streams not alllowed to be partial` |

另有两处尚未建成函数的线索：从 `0x004B1C80` 起是按 typeId 二分比较的类型工厂 `switch`（`cmp ecx, 0x057786F9; je …; push 0x14; call 分配`）；`RtsXmlPipeline\ReloDetokenizers.h:127` 报 `unknown polymorphic type`，说明部分重定位字段装的是多态类型令牌，装载时要换成虚表指针，这是第二个里程碑要读的代码。

### 3.5 全量校验

`work/probe/validate_all.py`（临时脚本，不入库；`Ra3::asset` 合并后由 `ra3tool streams` 取代）对 257 组逐组检查：清单尺寸方程、记录内偏移不越界、三个块长之和加 4 等于 `.bin/.relo/.imp` 长度、四件套首 4 字节校验和一致。结果 0 问题；唯一的偏差是 63 个地图流的 `totalInstanceDataSize` 大于记录之和。`.bin` 长度取自 RefPack 头，不必解压：`worldbuilder` 流的实例数据有 1.17 GB，32 位进程不能整块解压。[全量]

### 3.6 未决项

- **instanceId 的哈希算法**：已排除 SuperFastHash（含种子/大小写变体）、CRC32、FNV-1/FNV-1a、DJB2、Jenkins one-at-a-time、lookup2、lookup3、Murmur2。`.text` 里 58 处 FNV-1 都是 EASTL `hash_map` 的字符串哈希。存档 `xfer` 代码保存的是 `(typeId, instanceId)` 对而不是名字，所以那里也没有运行时哈希。清单自带完整名字表，首个里程碑不需要这个算法。
- `allTypesHash`、`instanceHash`、记录 `flags` 和地图流 `totalInstanceDataSize` 的语义。
- 多态类型令牌在重定位中的编码。

## 4. `Ra3::asset` 接口草案

### 4.1 模块职责和依赖

建议依赖方向为：

```text
Ra3::core  <-  Ra3::io  <-  Ra3::asset  <-  common/render/audio/game*
```

`Ra3::asset` 依赖 `Ra3::core` 的 `Result/Error`、安全整数/字节序工具和必要的字符串规范化；依赖 `Ra3::io` 的 `ArchiveSet`/`BigArchive` 读取成员及 `RefPack` 解压。[推断] 它不依赖 render、audio、common、gamelogic 或 gameclient；这些上层模块通过类型适配器消费资源。[推断]

首版必须保留原始字节并以“偏移”表示位置，不应把发行版中的 32 位指针布局直接暴露为宿主 C++ 指针。[推断] 这样可做严格边界校验，也不会把 Win32 ABI 假设扩散到其他模块。[推断]

### 4.2 只含声明的头文件轮廓

工程是 C++17，接口里不用 `std::span`，用 `const std::vector<T>&` 或指针加长度。实际实现以 `src/asset` 为准，这里只保留设计意图。

`include/ra3/asset/AssetId.h`：定义稳定、可比较的资源身份，即清单记录里的 `(typeId, instanceId)`，与资源引用缓冲的 8 字节项一致。[全量]

```cpp
namespace ra3::asset {
struct AssetId {
    std::uint32_t typeId;
    std::uint32_t instanceId;
};
}
```

`include/ra3/asset/StreamBundle.h`：表示同一 stem 的四个已解压只读字节区及来源路径；负责成员发现、缺件/重复件报告和逐成员 RefPack 探测。[实测][推断]

```cpp
namespace ra3::asset {
struct StreamMember;
struct StreamBundle;

Result<StreamBundle> readStreamBundle(
    const io::ArchiveSet& archives, std::string_view stem);
}
```

`include/ra3/asset/Manifest.h`：只解析目录，不构造类型对象；公开流标识、未知头字段、资源记录和其原始范围，使后续发现新字段时保持可追溯。[推断]

```cpp
namespace ra3::asset {
struct ManifestHeader;
struct AssetRecord;
class Manifest {
public:
    static Result<Manifest> parse(ByteView bytes);
    const std::vector<AssetRecord>& assets() const;
};
}
```

`include/ra3/asset/RelocationTable.h`：解析并校验 fixup 位置，生成“修复计划”；解析阶段不得就地修改档案字节。[推断]

```cpp
namespace ra3::asset {
struct Relocation;
class RelocationTable {
public:
    static Result<RelocationTable> parse(ByteView bytes);
    Result<void> validateAgainst(ByteView data) const;
};
}
```

`include/ra3/asset/ImportTable.h`：保存引用目标的原始编码、patch 位置、强/弱状态（只有在格式证实后才填语义枚举），并支持尚未解析的导入。[XSD][推断]

```cpp
namespace ra3::asset {
struct ImportRecord;
class ImportTable {
public:
    static Result<ImportTable> parse(ByteView bytes);
    Result<void> validateAgainst(ByteView data) const;
};
}
```

`include/ra3/asset/AssetStream.h`：拥有四件套解析结果和不可变数据块；提供按记录读取原始对象范围的视图，以及可选的、独立拥有的重定位工作映像。[推断]

```cpp
namespace ra3::asset {
class AssetStream {
public:
    static Result<AssetStream> load(StreamBundle bundle);
    const Manifest& manifest() const;
    std::optional<ByteView> findRaw(AssetId id) const;
};
}
```

`include/ra3/asset/AssetRegistry.h`：挂载多个流，执行“登记全部资源 → 内部重定位 → 解析导入”的分阶段装载，处理重复身份、优先级、循环依赖和未解析弱引用。[XSD][推断]

```cpp
namespace ra3::asset {
class AssetRegistry {
public:
    Result<void> mount(AssetStream stream);
    const AssetRecord* find(AssetId id) const;
    const std::vector<AssetStream>& streams() const;
};
}
```

`include/ra3/asset/TypeCatalog.h`：把已知原始 type value 映射到诊断名称和 schema 家族；它是可扩展元数据，不负责创建 render/audio/game 对象。[XSD][推断]

错误必须至少区分：成员缺失、RefPack 解码失败、签名/版本不支持、截断、整数溢出、越界记录、错误重定位、重复资源、未解析强导入和不一致的 stream id。[推断] 未知字段和未知资源类型应可保留并枚举，不应仅因“尚无类型适配器”而拒绝整个流。[推断]

## 5. 用真实数据逐步验证

进度：第 1–6、8 步已完成，结果见 §3.2；第 9 步完成了结构不变量部分，重定位和导入的全量解析留给第二个里程碑；第 10 步完成了清单和校验和两段调用链；第 7 步的类型部分已由原程序注册表证实（typeId/typeHash 是编译期常量），实例哈希算法仍未知。

1. **冻结清单。** 用 `ra3tool mount`/`big` 只读枚举 14 个档案，按规范化 stem 分组，验证 257 组都恰有四个成员；记录每个成员的存储大小、RefPack 状态和解压大小。[实测][推断]
2. **以 `locale` 建立最小样本。** 它四件套均未压缩且体积小；先确认四成员公共 `0xDC63FC0D` 的作用域，并把所有 32 位值、对齐位置和字符串范围导出成带偏移表。[实测]
3. **分离传输层。** 对每个成员独立调用现有 RefPack 探测/解码；对 `global`、`static`、教程地图和 Apt 各选一组，证明解压后的格式不依赖所在 BIG 或成员是否压缩。[实测][推断]
4. **反推 manifest 记录。** 枚举候选头长/记录宽度，要求所有记录的 `.bin` 范围不重叠或符合可解释的共享规则、全部在界内、计数准确到文件末尾；不能仅凭一个样本选布局。[推断]
5. **反推 relocation。** 对每个候选 patch offset 检查 4 字节对齐、边界和 patch 前值；在数据副本上分别尝试“绝对基址”“相对对象”“相对流”模型，选择能在多组流上使所有目标落入合法对象/字符串范围的模型。[推断]
6. **反推 import。** 从公开 XML 选择有明确 `AssetReference`、`WeakReference` 和循环引用的已知资源，定位其 manifest 记录与 `.bin` 区间，再关联 `.imp` 项；用 Apt 中 4 字节 `.imp` 作为“零导入”负样本。[XSD][实测][推断]
7. **确认身份算法。** 用 XML 中已知的类型名和 `id` 对照 manifest 候选值，测试常见大小写/命名空间拼接和哈希算法；只有至少跨 3 类资源、多个流全部吻合后才把原始字段正式命名为 type hash/instance hash。[XSD][推断]
8. **验证 `.cdata`。** 选择 `AudioFile`、Texture/Apt `DataBlob` 和 `static`/`locale` 的外置块，追踪其四段十六进制文件名与资源/导入记录的对应关系，确认键组成、分块和压缩语义。[XSD][实测][推断]
9. **跑全量不变量。** 对全部 257 组验证：无越界、计数一致、公共标识一致、重定位位置合法、所有强导入可解析；弱导入允许延后但必须报告最终状态。输出按流和资源类型的统计，不修改游戏目录。[推断]
10. **补程序证据。** 函数台账出现 assetmanager 归属后，从 manifest 打开、分配 `.bin`、应用 relo、解析 imp 的调用链逐项核对；在此之前不宣称精确字段布局已与 RA3 原程序一致。[实测][推断]

首个里程碑的验收标准不是“能把字节强转成某个 C++ struct”，而是：`locale`、`global`、一个地图和一个 Apt 流都能安全列出资源；所有读取、解压、目录、重定位和导入错误都有精确文件名与偏移；同一解析器随后可无越界地扫描全部 257 组。[推断]
