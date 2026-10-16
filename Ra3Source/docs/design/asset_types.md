# RA3 资源类型与名称哈希

## 1. 范围

本文记录发行版编译资源中的类型目录、原程序的类型处理器注册，以及实例名到
`instanceId` 的算法。游戏目录只读，程序没有运行；程序结论均来自 ledger 标为
`clean` 的 `.text` 函数。[程序]

完整逐类型结果在
`docs/analysis/ra3_1.0/asset_types.tsv`。生成器是
`tools/analysis/build_asset_type_table.py`，程序侧提取器是
`tools/ghidra/ExportAssetTypes.java`。[程序][全量]

## 2. 类型目录

14 个 SkuDef 档案的 257 组流共有 118,232 条资源和 257 个 `typeId`。程序对
`0x00417220` 有 164 次可恢复的直接调用，得到 164 个不同的处理器注册。
两侧并集再加类型工厂独有项后，表中共有 266 行。[程序][全量]

表中 `streams` 是出现该类型的流数量，不是流名列表。尺寸是 manifest
`instanceDataSize` 的十进制最小值和最大值。[全量]

数据和程序共有 157 个类型。它们的 `typeHash` 全部相同，没有发现冲突。
数据中另有 100 个没有处理器注册的类型；其中大量是 UI 命令或多态子类型，
不能据此推断它们都需要独立资源处理器。完整集合就是
`asset_types.tsv` 中 `asset_count` 非空且 `registrar_address` 为空的行。[程序][全量][推断]

程序注册但发行数据未出现的 7 个类型如下。程序里没有它们的类型名字符串；名称
是用 §3.2 的 typeId 算法对 modding SDK `Schemas/*.xsd` 中全部标识符求哈希反查
得到的（表中 `typeName_source=schema`）。[程序][全量]

| typeId | 类型名（XSD 反查） | typeHash | handler vtable | registrar |
|---|---|---|---|---|
| `0x0CAD7864` | `DLContent` | `0x629BD41C` | `0x00BF3400` | `0x00BA8F20` |
| `0x0F3DE1C6` | `AssetReportTable` | `0x6A9E6E8B` | `0x00BD930C` | `0x00BA5560` |
| `0x17E53184` | `CrowdResponse` | `0x07BCA38C` | `0x00BDFC28` | `0x00BA6150` |
| `0x89A0BDA4` | `GameplayConditionData` | `0xBE9DF599` | `0x00BF3474` | `0x00BA9070` |
| `0xD63C6B4C` | `MapNameToAchievementEventMapping` | `0x404D9E5D` | `0x00BF3514` | `0x00BA9170` |
| `0xF38E1878` | `AIPersonalityToAchievementEventMapping` | `0x8EC2496A` | `0x00C35DD8` | `0x00BA9F60` |
| `0xFC82DC06` | `TheVersion` | `0xA5DF846F` | `0x00BE6EB0` | `0x00BA7F30` |

163 个初始化器使用静态 16 字节描述符。`0x00540B50` 是唯一的动态描述符调用：
它写入 `typeId=0x86682E78`（`FXList`）、`.rdata:0x00BC7D34` 中的
`typeHash=0x6C5977BB` 和虚表 `0x00BDFD04` 后调用 `0x00417220`。[程序]

持久化补建的 `0x004B1C80` 类型工厂叶函数覆盖 4 个 case：

| typeId | 分配大小 |
|---|---:|
| `0x05DB454B` (`SkirmishStartCash`，XSD 反查) | 16 |
| `0x057786F9` (`FontSubstitution`) | 20 |
| `0x05CE3E75` (`PathMusicGameDynamicState`) | 20 |
| `0x05F72D93` (`SupplyCenterProductionExitUpdateModuleData`，XSD 反查) | 16 |

这里只报告该叶函数实际覆盖的 case，不能把 4 个误称为完整类型工厂数量。[程序]

## 3. instanceId 名称哈希

### 3.1 程序调用

`0x00417DB0` 接收 `(bytes, length, seed, finalize)`，逐字节调用 `tolower` 后做
32 位混合。资源调用点把 `length` 同时作为长度和 seed，并把 `finalize` 设为
1。例如 `0x004A6C40` 对
`"InfiltrationCanEnterObjectFilter"` 调用
`hash(name, 0x20, 0x20, 1)`；`0x0047BE90` 对运行时字符串先求长度，再以
`hash(name, length, length, 1)` 调用。[程序]

算法按无符号 32 位回绕，输入先按字节转小写：

```text
h = length
每 4 字节：
    h += b0 | b1<<8
    h ^= (((b2 | b3<<8) ^ (h<<5)) << 11)
    h += h>>11
余 1 字节：h += b0; h ^= h<<10; h += h>>1
余 2 字节：h += b0|b1<<8; h ^= h<<11; h += h>>17
余 3 字节：h += b0|b1<<8; h ^= (((b2<<2)^h)<<16); h += h>>11
h ^= h<<3; h += h>>5; h ^= h<<2; h += h>>15; h ^= h<<10
```

这是 SuperFastHash 家族的混合，但 RA3 的关键调用约定是：每个输入字节先
`tolower`，初值是字节长度，并执行末尾 avalanche。仅写“SuperFastHash”不足以
复现结果。[程序]

Python 参考实现是 `tools/analysis/asset_name_hash.py`。它对 257 个流的全部
118,232 条 `(instanceName, instanceId)` 验证，结果 118,232 相等、0 不等。
例如 `Arial -> 0x23BAA623`、`LucidaSans -> 0xD548386B`、
`RedAlert -> 0x1E51591F`。发行数据中的实例名全部是 ASCII；非 ASCII 的
`tolower` 区域设置行为没有数据证据，参考实现因此明确拒绝非 ASCII。[全量][程序]

大小写折叠是算法的一部分，所以 ASCII 大小写不同的名字会得到同一 ID。
manifest 中没有同一类型下这种碰撞；一般 32 位哈希仍可能碰撞，不能只靠
`instanceId` 证明原始拼写唯一。[程序][全量][推断]

### 3.2 typeId

`typeId` 是同一混合**不做大小写折叠**、以字节长度为初值并执行 avalanche 的结果，
对 257 个发行类型名全部成立（例如 `FontSubstitution -> 0x057786F9`、
`FXList -> 0x86682E78`）。实例名算法会先转小写，所以直接套用它对类型名 0 命中。
程序里计算类型名哈希的调用点尚未定位；类型 ID 在注册描述符里是编译期常量。[全量]

### 3.3 C++ 实现与全量校验

`ra3::asset::hashName / instanceIdFromName / typeIdFromName`
（`src/asset/include/ra3/asset/NameHash.h`）按 `0x00417DB0` 反编译语义实现：
MSVC `char` 有符号，每个字节以有符号值传给 `tolower` 并以符号扩展参与混合，所以
≥0x80 的字节与参考 SuperFastHash 不同；大小写折叠按默认 "C" locale（只改 A–Z）。
`ra3tool streams` 用我们自己的清单解析器对全部 118,232 条资源同时校验
`instanceId == instanceIdFromName(实例名)` 与 `typeId == typeIdFromName(类型名)`，
结果 0 不等，与 Python 参考实现互相独立。[全量][程序]

## 4. 尚未确定

`typeHash` 不是类型名经两种变体得到的值，也不是 CRC32/FNV-1/FNV-1a；
推测是类型结构定义（XSD 布局）的版本哈希，未证实。[全量][推断]

`instanceHash` 也不是实例名、`Type:Name`、类型名或流名经上述算法得到的值：
对 118,232 条记录四种形式都为 0 匹配。它的内容哈希或版本语义仍未知。[全量]
