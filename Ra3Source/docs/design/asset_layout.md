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

全量结果（2026-10-16）：

| 指标 | 结果 |
|---|---:|
| 四件套流 | 257 / 257 |
| 发行资源类型完全通过 | **132 / 257** |
| 发行 manifest 记录通过 | **87,592 / 118,232** |
| 未通过记录 | 30,640 |

这里“类型完全通过”要求该类型每条发行记录（包括零长外部占位）都通过；“记录通过”
不等于已经理解其上层游戏语义，只表示本文列出的二进制布局、边界、relo 和 imp
不变量全部成立。

## 4. 主要失败类型

完整逐类型原因见 coverage TSV。按未通过记录数排序的主要缺口：

| 类型 | 通过 / 总数 | 原因 |
|---|---:|---|
| AudioEvent | 0 / 5,234 | `Audio` typeGroup 有源到 runtime 的专用重排；当前仍按源 XSD 得到 244 字节，而发行根常为 176/200 字节 |
| W3DMesh | 12,033 / 17,058 | `W3D` typeGroup 的顶点块和 FXShader 多态常量不是普通 list |
| GameObject | 8,511 / 12,280 | 大量模块/文本字段使用 tokenized runtime 表示；源 XSD 直接布局会把 token 误判为指针 |
| W3DContainer | 5,053 / 7,014 | RenderObject substitution/choice 需要多态 token 解码 |
| W3DHierarchy | 5,009 / 6,946 | Pivot 的 Poid relo 值不是普通实例相对指针，符合待解的 ReloDetokenizer 路径 |
| W3DAnimation | 4,099 / 5,924 | channel choice 在运行时变为 timecoded/delta 多态结构 |
| FXParticleSystemTemplate | 2,740 / 4,110 | Physics/Draw 等多个 choice 的 runtime discriminant 和对象表示未知 |
| ObjectCreationList | 898 / 1,787 | polymorphic CreateObject nugget 尚未解码 |

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

1. `xas:typeGroup="Audio" / "W3D" / "Tokenized"` 的完整编译规则不在公开 XSD
   本身；需继续从同目录 runtime 类型、发行字节及原程序
   `RtsXmlPipeline/ReloDetokenizers.h` 调用链三方交叉验证。
2. choice/substitution group 的槽究竟是 discriminant+inline union、token 指针还是
   对象数组，随 typeGroup 变化，不能采用一个未经验证的统一表示。
3. Poid 在普通 XML 对象图与 W3D Pivot 中表现不同；W3D 的 relo 项可能触发
   detokenizer，而不是普通“加实例基址”。
4. worldbuilder 流中的少量系统/UI 类型与另外两份平台流根尺寸不同，可能是
   runtime wrapper 或 schema 版本差异；在找到版本选择依据前保留为失败。
5. `typeHash` 很可能可用于选择精确 layout 版本，但其生成算法仍未证实。

