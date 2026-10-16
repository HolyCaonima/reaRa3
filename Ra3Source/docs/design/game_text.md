# GameText 运行时字符串表

本文记录英文 1.0 `ra3_1.0.game` 的静态恢复和离线差分结果。地址以
`0x00400000` 为映像基址；只移植和 hook 台账中 `.text / clean` 的函数。

## 1. 对象与全局

`TheGameText` 全局槽是 `0x00DEEA54`。对象虚表为 `0x00BE0FD0`，构造代码
`0x0053B4A0` 初始化到 `+0x68`，所以已确认管理器大小为 0x6C：

| 偏移 | 字段 |
|---:|---|
| `+0x00` | vptr `0x00BE0FD0` |
| `+0x38` | initialized |
| `+0x48` | primary `GameStringTable*` |
| `+0x4C` | map `GameStringTable*` |
| `+0x50` | override `GameStringTable*` |
| `+0x54..+0x60` | prefix-query result storage |
| `+0x64` | fetch mode |
| `+0x68` | reload pending |

前三个表按 `override -> primary -> map` 的顺序查找。每个表是 0x0C 字节：
`{u32 count, GameTextInfo* values, GameTextLookupEntry* lookup}`。排序数组元素为
`{GameAsciiString* label, GameTextInfo* info}`，大小 8。`GameTextInfo` 大小 0x0C，
依次为单指针的 label、UTF-16 text、extra 字符串对象。精确声明及偏移断言在
`ra3/text/GameTextLayout.h`。

## 2. 查询语义

`0x00539080` 在一个表的 `lookup` 数组上做 lower-bound。比较器
`0x00535A10` 调用 MSVCRT `_stricmp`；命中后返回 `&entry.info->text`，未命中返回
空指针。标签因此对 ASCII 大小写不敏感，排序也是不分大小写排序。发行
`gamestrings.csf` 的 10,401 个标签均为 ASCII，且没有忽略大小写后的重复项。
离线 `CsfStrings` 已使用同一 ASCII 不分大小写语义，并在解析时拒绝这种重复。

管理器查询 `0x0053B500` 依次调用上述表查询。外层虚函数 fetch
`0x00521C50` 位于虚表 `+0x40`：普通模式通过虚表 `+0x4C` 调管理器查询并填写
`exists`；未命中时返回全局字符串对象 `0x00DEAD00`。`fetchMode == 1` 时使用
`0x00DEEC70` 返回标签本身。两个全局对象如何初始化及缺失文本的最终内容尚未恢复，
所以 hook 只覆盖返回指针/空指针的内层只读查询，不制造缺失字符串。

## 3. 加载与保护边界

`Data\GameStrings.csf` 的路径 getter 是 `0x004C9E40`。初始化
`0x005478C0` 分配三个 0x0C 字节表对象并进入 CSF 读取路径。主体
`0x0052FCE0` 有 `securom == inner-patched`，本轮在这里停止，没有反编译、移植或
hook 该加载器。clean 的销毁路径 `0x00541E30` 证实三个表的所有权。

因此当前已恢复的是加载完成后的稳定内存模型和只读查询，不声称恢复了 CSF 到对象的
完整构造流程。

## 4. 与 StringHashTable 资源的关系

资产系统另有 `StringHashTable`（typeId `0x11E5CF64`）。XSD 把它声明为
`ResourceManager::DataToStringTable` runtime wrapper，内容是
`StringHashBin` 加 `{Hash, Text}` 列表；发行流中有 4 个实例，布局验证为 4/4。
它服务字符串到整数 hash 的编译资产，不是 UI 本地化接口。

UI、菜单和 tooltip 调用 `0x00DEEA54` 的虚表 `+0x40`，最终进入本页的 CSF
标签到 UTF-16 字符串查询。两条路径的数据类型、全局入口和返回值均不同。

## 5. 函数图与 hook

| VA | 名称 | 作用 | 状态 |
|---|---|---|---|
| `0x00521C50` | `GameText::Fetch` | fetch mode、exists、缺失回退包装 | clean，未 hook |
| `0x00535A10` | `GameTextLowerBound` | `_stricmp` equal-range/lower-bound | clean，未 hook |
| `0x00539080` | `GameStringTable::Find` | 单表只读查询 | `gametexttable` |
| `0x0053B500` | `GameTextManager::Find` | 三表优先级查询 | `gametextmanager` |
| `0x005478C0` | `GameTextManager::Init` | 分配表并触发加载 | clean，有副作用，不 hook |
| `0x0052FCE0` | CSF loader | 解析并建立表 | inner-patched，停止 |

两个 hook 都是 thiscall（hook 用 `__fastcall(this, edx, label)`），默认关闭，
`explicitOnly=true`。shadow 先调用原函数，再在同一只读结构上运行重建实现，只比较
返回指针；不修改对象、不分配游戏对象，也不进入缺失字符串回退。

## 6. 离线验证

`textcheck` 通过 Ra3::io 从只读安装读取并解压真实 `gamestrings.csf`，构建上述
布局，并对每个原标签、大小写翻转标签和两个 miss 验证 C++ 与离线 `CsfStrings`。
`tools/analysis/ports/verify_game_text_lookup.py` 把同一标签集和布局放进 Unicorn，
绑定 `_stricmp`，执行原始 `0x00539080`/`0x0053B500`。模拟器只映射磁盘
`.text/.rdata/.data`，不映射或执行 SecuROM 区段。最终计数见
`csf_annotations.tsv`。

## 7. 未决项与实机覆盖

- 管理器基类 `+0x04..+0x37` 的非文本字段未命名。
- `0x00DEAD00` 的缺失字符串内容和初始化点未恢复。
- `_stricmp` 的非 ASCII locale 行为没有发行标签证据；重建实现明确按 ASCII 折叠。
- loader 因 inner-patched 边界未分析，map/override 表分别由哪些上层文件填充仍待实机
  状态或其他 clean 调用链确认。
- live shadow 应从窗口出现后尽早注入，经过主菜单、遭遇战设置和对局内 tooltip；
  两个目标都应有 calls 增长且 mismatches 保持 0。
