# 编译资源的 C++ 类型视图

## 1. 生成范围

`tools/analysis/gen_asset_types.py` 复用 `xsd_layout.py` 的 `SchemaModel` 和
`Layout`，再以
`docs/analysis/ra3_1.0/asset_layout_coverage.tsv` 中
`fully_validated=yes` 为唯一根类型清单。布局工具以后新增完全通过的类型时，生成器
会自动纳入，不维护手写类型表。当前生成结果是 132 个根类型、190 个
根/内联/列表元素结构和 20 个枚举，位于
`src/asset/include/ra3/asset/types/GeneratedTypes.h`，生成头为 240,327 字节。

生成文件派生自 EA GPL v3 Mod SDK 的 `AssetType*.xsd`、`Includes/*.xsd` 及其引用
schema；文件首行列出实际使用的 schema 文件，未把 XSD 本身复制进仓库。每个结构
使用固定宽度字段，并对结构尺寸和每个字段的 `offsetof` 做编译期断言。

再生命令（从 `Ra3Source` 目录运行）：

```powershell
python tools/analysis/gen_asset_types.py `
  --schemas "D:/Game/Ra3/RevRa3/work/references/modding/Red Alert 3/Schemas"
```

CI 或人工检查生成文件是否过期时追加 `--check`。

## 2. 编译编码与安全访问

`ra3/asset/TypedAsset.h` 定义保持 32 位落盘形状的轻量 wrapper：

| wrapper | 落盘字段 | 语义 |
|---|---|---|
| `RuntimeToken` | `u32 raw` | 根对象的 runtime type/token 槽 |
| `CompiledString` | `u32 length, u32 offset` | UTF-8/字节字符串，长度不含结尾 NUL |
| `CompiledList<T>` | `u32 count, u32 offset` | 连续定长元素 |
| `FileReference` | `u32 offset` | NUL 结尾字符串 |
| `DataBlob` | `u32 offset, u32 size` | 实例内字节范围 |
| `AssetReference` | `u32 index` | 1 起的 manifest reference 序号；0 是 null |

wrapper 只保存原始 `u32`，不保存宿主指针，因此同一结构既描述未重定位实例，也描述
原游戏随后原位修复的 32 位对象槽。`resolve`/`resolveList` 以
`AssetInstance` 为边界，检查算术范围、NUL、`.relo` 或 `.imp` 覆盖后才返回
`Result`。`asView<T>` 同时检查 manifest `typeId` 和最小实例尺寸，再用
`memcpy` 建立 trivially-copyable 值视图，避免直接对档案字节形成未对齐或生命周期
不成立的 C++ 对象。

每个根和内联结构另有 `TypeInfo`/`FieldInfo` 表，记录字段名、偏移、kind、元素类型
和元素尺寸。`ra3tool dump` 与 `types-check` 使用同一份反射递归访问嵌套结构和列表。

## 3. 枚举编码

普通 XSD enumeration 按声明顺序从 0 递增；`xas:forceValue` 会把当前项设为显式
数值，下一项从该值加一。`forceValue` 也可引用同一枚举中较早的项，此时形成数值
别名。负数按其 32 位二进制值生成。XSD list 仍是 `{count, offset}`，其枚举元素逐个
使用同一 32 位编码；它不是自动压成一个 bit mask。生成器保留重复数值，并由
`types-check` 对发行数据逐值验证。

## 4. 工具与发行数据验证

单个资源：

```powershell
ra3tool dump data/locale FontSubstitution:Arial
ra3tool dump data/global Language:EnglishLanguage
```

全量验证：

```powershell
ra3tool types-check
```

它只读 SkuDef 挂载的 BIG，对每个生成类型的每条发行实例递归检查字符串 NUL、列表
范围、枚举、bool、引用，并要求所有非空 offset/reference 槽分别被 `.relo`/`.imp`
覆盖；反向也要求表中每一项命中一个反射字段。零长依赖占位计入实例数，但没有可遍历
对象。

2026-10-17 的 Win32 Release 全量结果：

| 指标 | 结果 |
|---|---:|
| 生成类型 | 132 / 132 |
| 遍历 manifest 实例（含零长占位） | 44,016 |
| 反射字段访问 | 309,532 |
| 失败 | 0 |
| wall time | 17,464 ms |

布局文档中的 87,592 是所有 257 类中“通过的记录”总数，其中包含尚未整类通过的
类型；本工具只统计 132 个 `fully_validated=yes` 类型的全部记录，所以实例数是
44,016。

严格 bool 检查发现了原布局验证器只凭尺寸/relo/imp 无法发现的字段顺序缺口：
`OnDemandTextureImage`、`PackedTextureImage`、`RadarTerrainImage` 的 `Rotated`
以及 `W3DCollisionBox` 的 `JoypadPickingOnly` 在发行结构中位于内联坐标元素之后，
而不是通用 attribute-first 位置。生成器保留了这四个经全部发行实例验证的 runtime
顺序修正；`xsd_layout.py` 的布局算法未为此改动。
