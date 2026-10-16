# BigHandler 档案文件系统

本文记录 `ra3_1.0.game` 的只读静态恢复结果。所有地址属于英文 1.0
映像（基址 `0x00400000`）；只分析函数台账中 `.text / clean` 的代码。

## 1. 对象模型与所有权

`0x004D2490` 分配 0x20 字节的具体 BigHandler，把基类构造函数
`0x009F5F40` 的虚表 `0x00C69460` 替换成派生虚表 `0x00BDAF98`，再把对象指针写到
全局槽 `0x00DEB69C`。该对象拥有单向档案链表：

```
0x00DEB69C -> GameBigHandler
                  +0x10 head -> GameBigArchive -> next -> ...
                  +0x14 tail
                  +0x18 archiveCount
                  +0x1C allocator
```

`AddArchiveFromFile` (`0x009F5AF0`) 为每个档案分配 0x68 字节节点并追加到尾部，所以
遍历头到尾就是 SkuDef 的 `add-big` 顺序。每个节点拥有文件对象、读入内存的完整目录
头和可选的排序哈希表。`Open` (`0x009F5F80`) 命中后分配 0x40 字节
`GameBigFile`；`Close` (`0x009F5DB0`) 释放其可选 RefPack inflater、临时缓冲和文件
对象。

派生虚表 `0x00BDAF98` 的相关槽为：析构 `+0x00`、open `+0x0C`、close `+0x10`、
read `+0x14`、seek `+0x1C`、stored-size `+0x20`、absolute-offset `+0x28`。
派生类在 `+0x38/+0x3C/+0x40` 覆盖枚举入口；基类对应实现是
`0x009F65E0/0x009F6670/0x009F5EB0`。

## 2. 内存布局

精确声明在 `ra3/io/GameBigLayout.h`，并对大小和关键偏移作了 `static_assert`。

| 类型 | 大小 | 已确认字段 |
|---|---:|---|
| `GameBigHandler` | 0x20 | vptr `+0x00`，head/tail/count `+0x10/+0x14/+0x18`，allocator `+0x1C` |
| `GameBigArchive` | 0x68 | next/file/header `+0x00/+0x04/+0x08`，内嵌 index `+0x0C`，archiveName `+0x18`，两个 64 位范围值 `+0x20/+0x28`，format `+0x30`，同步对象 `+0x38` |
| `GameBigIndex` | 0x0C | header `+0x00`，entries `+0x04`，searchTable `+0x08` |
| `GameBigSearchEntry` | 0x08 | byte-swapped DJB2 hash `+0x00`，目录记录指针 `+0x04` |
| `GameBigFile` | 0x40 | archive `+0x00`，inflater/temp `+0x0C/+0x10`，dataOffset `+0x18`，logicalSize `+0x20`，compressedSize（明文为 0）`+0x28`，source/logical cursor `+0x30/+0x38` |

`GameBigArchive +0x38` 的 0x30 字节只确认由 `0x00A21790` 初始化；具体同步原语名称仍未知。
`+0x20/+0x28` 的语义由 open/read 的加法约束恢复为档案范围起点和流基址，尚未用运行
时状态核对。

## 3. 函数图

| VA | 提议名称 | 作用 | 本轮 hook |
|---|---|---|---|
| `0x009F5A80` | `GameBigArchive::GameBigArchive` | 初始化 0x68 字节档案节点 | 否，构造有副作用 |
| `0x009F5AF0` | `BigHandler::AddArchiveFromFile` | 打开档案、读目录、建节点并追加 | 否，文件/分配 |
| `0x009F5DB0` | `BigHandler::Close` | 释放 open 返回的文件对象 | 否，释放 |
| `0x009F5E00` | `BigHandler::Seek` | 更新物理/逻辑游标，必要时重置 inflater | 否，有状态 |
| `0x009F5E80` | `BigHandler::GetStoredSize` | 返回文件对象 `+0x20` 的 64 位值 | 后续可选 |
| `0x009F5E90` | `BigHandler::GetAbsoluteDataOffset` | 档案范围起点加文件数据偏移 | 后续可选 |
| `0x009F5F40` | `BigHandler::BigHandler` | 初始化 0x20 字节管理器 | 否，构造 |
| `0x009F5F80` | `BigHandler::Open` | 路径解析、跨档案查找、创建文件对象 | 否，文件/分配 |
| `0x009F6280` | `BigHandler::InflateRange` | 流式 RefPack 读取辅助 | 否，有状态 |
| `0x009F63B0` | `BigHandler::Read` | 原始或 RefPack 文件读取 | 否，文件/状态 |
| `0x009F65E0` | `BigHandler::FindFirst` | 分配枚举状态并取第一项 | 否，分配 |
| `0x009F6670` | `BigHandler::FindNext` | 跨档案推进枚举游标 | 否，有状态 |
| `0x009F6810` | `Big::ReadBigEndian32` | 读取 1..4 字节整数 | 纯函数，未纳入本轮 |
| `0x009F6880` | `Big::ReadBigEndian64` | 读取变宽大端整数 | 纯函数，未纳入本轮 |
| `0x009F6910` | `Big::DetectFormat` | BIG/C0FB/Viv4 判别 | 纯函数，未纳入本轮 |
| `0x009F6990` | `Big::HashPath` | `tolower` DJB2，种子 0x1505，结果字节翻转 | 由查找实现内联复刻 |
| `0x009F6A20` | `Big::Index::Index` | 初始化 0x0C 字节目录视图 | 否，构造 |
| `0x009F6A50` | `Big::HeaderSize` | 读取格式相关头长度 | 纯函数，未纳入本轮 |
| `0x009F6AA0` | `Big::EntryCount` | 读取格式相关条目数 | 纯函数，未纳入本轮 |
| `0x009F6B00` | `Big::TrailerSize` | 识别“字母 + 三位数字”的 8 字节尾部 | 纯函数，未纳入本轮 |
| `0x009F6B90` | `Big::FindHashedEntry` | 排序哈希表二分和碰撞邻域检查 | `bigfindhash` |
| `0x009F6D00` | `Big::FindEntry` | 按路径或序号查找并填写 offset/size/cursor | `bigfindentry` |
| `0x009F7000` | `Viv::FindByHash` | 在固定 16 字节记录中按 hash 线性查找 | 否，发行 BIG 不走 |
| `0x009F7130` | `Big::BuildSearchTable` | 分配、填充并排序 `{hash, record}` | 否，分配/qsort |
| `0x009F71D0` | `Viv::FindEntry` | Viv4 路径 hash 查找适配 | 否，发行 BIG 不走 |

完整逐函数标注见 `docs/analysis/ra3_1.0/bigfs_annotations.tsv`。

## 4. open-by-path 调用流

1. `Open` 去掉区分大小写的 `"big:"` 前缀，再去掉一个开头的 `/` 或 `\`。
2. `archive|path` 形式先按档案名称限定节点；没有 `|` 时遍历全部节点。
3. 对普通 BIG 节点调用 `Big::FindEntry`。有 `searchTable` 时转到
   `Big::FindHashedEntry`；否则从目录记录线性扫描。路径比较调用
   `0x009F3650`，只忽略 ASCII 字母大小写，`/` 与 `\` 不相等。
4. 从链表头开始的第一个命中获胜。
5. 读取成员前 16 字节并用 `0x009F74F0` 探测 RefPack；随后分配 0x40 字节文件对象。

离线 `BigArchive::find` 现与第 3 步一致，不再把两种斜杠视为相同。较高层
`ArchiveSet::find` 仍把 API 输入的 `/` 规范成 `\`，对应游戏进入 BigHandler 前的
路径规范化职责，并保持 asset 层的跨平台路径接口。

## 5. hook 与验证

两个新目标默认关闭；仅在配置中显式写键时安装。shadow 总是先执行原函数，再在相同
只读结构上运行重建实现。`bigfindentry` 把所有 in/out 参数的初值复制到局部变量，
比较返回指针和每个非空 out 参数，向调用者保留原函数结果。

离线 oracle 从只读 `Data\Misc.big` 取真实目录头，放入 Unicorn heap，构建与
`0x009F7130` 相同的排序表，并执行原始机器码：

| 函数 | 用例 | 结果 |
|---|---:|---:|
| `0x009F6B90` | 441 原路径 + 441 大小写翻转 + 1 缺失 | 883 / 883 |
| `0x009F6D00` | 882 路径 + 441 序号 + 1 缺失 | 1324 / 1324 |
| 合计 | 2207 | 0 mismatch |

CRT `tolower` IAT 绑定为发行数据所需的 ASCII 折叠。oracle 没有映射或执行任何
SecuROM section。`bigcheck Data\Misc.big` 对同一批 2207 个 C++ 用例作独立检查。

## 6. 待实机确认

- 全局槽和布局来自静态写入/访问证据；注入时是否存在其他具体 BigHandler 实例未知。
- `+0x38` 同步对象的锁粒度未知。新实现只读，但 shadow 会把热查找成本增加一倍。
- 哈希碰撞邻域代码保留了原函数的精确比较次序；发行 `Misc.big` 未出现需要该异常
  分支才能区分的碰撞。
- `Viv4`/`C0FB` 格式路径已按 clean 代码记录，但发行版 14 个挂载档案都是 BIG4；
  本轮 live shadow 的有效覆盖应以 BIG4 为准。
- live shadow 应在窗口出现后尽早注入，并走到主菜单和一次遭遇战加载，以覆盖初始
  资源流、地图和对局资源查找；长时间运行与并发加载仍需单独观察。

## 7. 生命周期与第二批端口

`GameBigHandler +0x1C` 是带虚表的 rw::core allocator。BigHandler clean 路径确认：
`+0x08` 为带标签分配（`GameBigFile` 0x40、`GameBigArchive` 0x68、枚举游标 0x110、
搜索表 `count*8`），`+0x0C` 为 `free(pointer, 0)`；`+0x04` 只用于 RefPack 临时缓冲
的另一种分配入口。具体接口声明在 `GameBigLayout.h`，未知槽不命名。

`GameBigArchive +0x04` 是 rw::core 文件对象。`Read` 只确认其虚表 `+0x14` 为 read、
`+0x1C` 为绝对 seek；完整文件接口的其余槽和打开/关闭所有权仍未知。档案内
`+0x38` 的 0x30 字节对象由 `0x00A21790` 初始化：前 0x18 字节兼容
`CRITICAL_SECTION`/mutex handle，`+0x20` 是递归计数，`+0x24` 的字节选择
critical-section 路径，`0x00A216A0/0x00A21760` 分别 acquire/release。析构路径和
`+0x28` 的精确意义未恢复，因此代码仍把它保留为 opaque 0x30 字节。

第二批公开了并离线差分验证以下无副作用端口：

| VA | Ra3Source | Unicorn vs C++ | hook |
|---|---|---:|---|
| `0x009F6810` | `readGameBigEndian32` | 1000/1000 | `bigreadbe32` |
| `0x009F6880` | `readGameBigEndian64` | 1000/1000 | `bigreadbe64` |
| `0x009F6910` | `detectGameBigFormat` | 1000/1000 | `bigformat` |
| `0x009F6990` | `hashGameBigPath(path, seed)` | 1000/1000 | `bighashpath` |
| `0x009F6A50` | `gameBigHeaderSize` | 1000/1000 | `bigheadersize` |
| `0x009F6AA0` | `gameBigEntryCount` | 1000/1000 | `bigentrycount`（显式） |
| `0x009F6B00` | `gameBigTrailerSize` | 1000/1000 | `bigtrailersize`（显式） |
| `0x009F5E80` | `gameBigStoredSize` | 1000/1000 | `bigstoredsize`（显式） |
| `0x009F5E90` | `gameBigAbsoluteDataOffset` | 1000/1000 | `bigabsoluteoffset`（显式） |

`applyGameBigSeekState` 精确实现 `0x009F5E00` 的字段转换（null-inflater
Unicorn vs C++ 1000/1000），但不释放 inflater/temp。
非零压缩流 seek 只改逻辑游标；零位置或 stored 路径把逻辑游标清零、物理游标设为
`dataOffset + position`，并要求调用者销毁 inflater、释放临时缓冲。由于 allocator
ABI 尚未经过原机调用桩验证，本轮不挂 Seek。

`Read` 先获取档案锁、通过文件对象 seek/read，再走 stored 直接读取或 0x70 字节
`RefpackInflate`。后者字段状态机与 `DecodedStream` 的 16/128 KiB 滑窗语义一致，
但对象 ABI 不兼容，不能把 STL/Pimpl 对象放进游戏的 0x70 字节内存。shadow 不能进行
第二次文件 I/O，replace 又依赖未验证的文件/allocator ABI，因此 Read、InflateRange、
Open、Close、AddArchiveFromFile 和 BuildSearchTable 本轮均不 hook。FindFirst 会分配
0x110 字节游标；FindNext 还调用外部模式匹配器并写调用方结果对象，也留待游标与结果
布局完整恢复后处理。

## 8. 第三轮 ABI oracle 与安全边界

`verify_big_lifecycle_abi.py` 在 Unicorn 中执行原始 clean 函数，并把虚表入口绑定到
Python 桩。验证结果：

- allocator `+0x04` 是
  `thiscall(size, tag, 0, alignment, 0)`；Read 的临时缓冲调用为
  `(0x8000, "rw::core::BigHandler::temp Z buf", 0, 4, 0)`。
- allocator `+0x08` 是 `thiscall(size, tag, flags)`；FindFirst 使用
  `(0x110, "BigHandler::FindStruct", 0)`，Open 的反汇编同样确认
  `(0x40, "BigFileHandler", 0)`。
- allocator `+0x0C` 是 `thiscall(pointer, 0)`。Close 依次调用 inflater 的 deleting
  destructor `(1)`、释放临时缓冲、释放 0x40 文件对象。
- archive file `+0x1C` 是 thiscall、6 个栈 dword、callee cleanup；`+0x14` 是
  thiscall、5 个栈 dword、callee cleanup。参数的高层语义仍有 opaque 参数，端口不能
  在证据不足时重新排序或省略。
- acquire `0x00A216A0` 为 `thiscall(sync, timeoutPointer)`、`ret 4`；release
  `0x00A21760` 为 sync 在 ECX 的 fastcall/plain `ret`。两者都是 `.text/clean`，
  critical-section 路径已在 oracle 中执行。

Open 的赋值序列纠正了第二轮字段命名：`+0x20` 是调用方看到的逻辑长度（压缩成员为
解压长度），`+0x28` 是压缩源长度并兼作 compressed 标志（明文成员为 0）。
`prepareGameBigFile` 已移植不分配的规范化、跨档案查找和 0x40 状态构造；
`seekGameBigFile`、`closeGameBigFile` 与 Read bookkeeping 也已移植。

本轮只挂 `bigseek` 和 `bigclose`，均为 `explicitOnly`。Seek shadow 在原调用前复制
完整 0x40 状态，调用后比较返回值和全部字段；replace 通过已验证的 inflater
destructor/free ABI。Close shadow 只能原样转发（原调用后对象已经释放），replace
走已验证释放顺序。Open、Read、InflateRange 和枚举没有达到任务要求的真实压缩档案
逐字节/完整状态 differential，因此没有挂钩；特别是不能把 `DecodedStream` 的
Pimpl 当成游戏 0x70-byte inflater。
