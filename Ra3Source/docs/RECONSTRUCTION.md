# RA3 重建总控文档

这份文档是整个重建工作的操作中枢：目标、路线、模块分工、函数台账协议、并行规则和授权边界。所有 agent（人或 AI）开工前先读这里。

## 1. 目标与路线

把 `ra3_1.0.game`（32 位 PE，带 SecuROM，明文代码在 `.text`）逐步重建成可维护的现代 C++ 源码（Ra3Source）。有两条路线：

- **路线 A — 进程内逐步替换**（参考 Generals 社区的 Thyme/GeneralsGameCode 做法）：从外部启动原版 `ra3_1.0.game`，注入 DLL，一个函数一个函数地用我们的实现替换原函数。每一步都能当场对照原版验证，游戏始终可运行。前提是原版能启动（**用户已确认能进对局**）且能在不触碰 SecuROM 的前提下注入和改写——**已在真实进程上确认**：窗口出现后注入、`.text` 内联补丁未被拒绝，RefPack 解码、名称哈希、路径比较三个函数同时整体替换后，游戏正常加载并能进入遭遇战对局（docs/design/injection.md §5.4）。
- **路线 B — 独立重写**（参考 OpenSAGE）：不运行原程序，只读游戏数据，自己写引擎。可移植、干净，但要到渲染打通才能看到画面，且没有原版可逐函数对照。

**当前策略**：路线 A 为主路线。继续推进两条路线都需要的**数据层与引擎无关层**（资源、文本、格式解析、平台抽象），每复刻一个 `securom == clean` 的函数，就先用 shadow 模式在真实进程里逐次对照，再切到 replace 模式。路线 B 保留为长期方向，数据层代码两边共用。不做 SecuROM 脱壳、解密或绕过。

## 2. 规模与现状

- `.text` 共 30,496 个函数、218 万条指令，明文。另有虚表恢复补建的约 11,022 个虚函数（只通过虚表调用、Ghidra 原先漏建），函数台账现共 48,982 行。扣掉第三方（EASTL/OpenSSL/运行库）后，真正要重建的约 **4 万个函数**。
- SecuROM 只改了零散单条指令：1,212 个函数受影响（入口丢失 653、入口可恢复 119、中间补丁 432、未分类 8）。详见 `docs/analysis/ra3_1.0_overview.md`。
- `Stext`/`Sdata`/`.securom` 里的约 7,400 个函数是 SecuROM 自身代码，**不在重建范围内**。
- 已完成并验证：`core`（配置/INI/SkuDef）、`io`（BigArchive/ArchiveSet/RefPack/ReadOnlyFile）。构建零警告，单测 25、数据测试 4、CTest 2 全通过。

## 3. 模块分工

Ra3Source 按子系统分模块，每个模块一个静态库 `ra3_<name>`（别名 `Ra3::<name>`），对外只暴露 `include/` 下的头文件。模块之间只能通过头文件接口依赖，不许直接用别的模块的 .cpp。

| Ra3Source 模块 | 原始 RA3 子树（恢复路径） | Generals 参考路径 | 状态 |
|---|---|---|---|
| `core` | Libraries\Source\{ini,string,debug,memorypool} | Code\GameEngine\Source\Common | 已完成 |
| `io` | lib-packages\RtsBase\…\file、rw::core filesystem | Code\GameEngine\Source\Common\System\ArchiveFile* | 已完成 |
| `text` | CSF 字符串表 | Code\GameEngine\Source\GameClient\GameText.cpp | 已完成（实测 10,401 条） |
| `asset` | Libraries\Source\assetmanager、RtsXmlPipeline | Code\GameEngine\Source\Common\System\AssetManager 等 | 里程碑 1 完成（清单、四件套校验、按资源切块、类型/实例名哈希） |
| `common` | GameEngine\common | Code\GameEngine\Source\Common | 规划 |
| `gamelogic` | GameEngine\gamelogic（127+ 函数已锚定） | Code\GameEngine\Source\GameLogic | 规划 |
| `gameclient` | GameEngine\gameclient | Code\GameEngine\Source\GameClient | 规划 |
| `net` | GameEngine\gamenetwork | Code\GameEngine\Source\GameNetwork | 规划 |
| `render` | GameEngineDevice\W3DDevice、SageRender | Code\GameEngineDevice\W3DDevice | 规划 |
| `audio` | GameEngineDevice\sageaudiodevice、rw::audio | Code\GameEngineDevice\…Audio | 规划 |
| `ui` | SageInGameUI、Apt（见导出表） | Code\GameEngine\Source\GameClient\GUI | 规划 |
| `platform` | Win32/DX9 入口、窗口、输入 | Code\GameEngineDevice\Win32Device | 规划 |

依赖顺序（自底向上）：`core → io → {text, asset} → common → {gamelogic, gameclient, net} → {render, audio, ui} → platform`。

## 4. 函数台账协议

`docs/analysis/ra3_1.0/function_ledger.tsv` 是所有函数的唯一事实来源，48,985 行，由 `tools/analysis/build_function_ledger.py` 生成。列：

`address  name  section  size  source_file  securom  module  owner  status  notes`

- `securom`：`clean` / `inner-patched` / `entry-recoverable` / `entry-lost` / `patched`。
- `module`：按 `source_file` 猜测；`securom` 表示 SecuROM 自身代码（跳过），`unknown` 表示尚未归属。
- `status`：`unanalyzed` / `claimed` / `in-progress` / `reversed` / `verified`。
- `owner`：认领者标识（agent 名或分支名）。

**人工状态不写在台账里**，而是写在旁边的 `docs/analysis/ra3_1.0/ledger_annotations.tsv`（列 `address status owner module notes`，每个地址一行）。生成脚本把其中非空字段覆盖到台账对应行，所以重新生成不会丢失认领和结论；标注了却找不到函数的地址会在生成时列出。

`docs/analysis/ra3_1.0/module_estimates.tsv` 在台账之上记录可复现的模块估计、证据方法和直接调用入边；从仓库根目录运行 `python Ra3Source/tools/analysis/attribute_modules.py --inputs D:/Game/Ra3/RevRa3/work/analysis/ghidra --exe D:/Game/Ra3/Game/Data/ra3_1.0.game` 可重新生成它及配套报告。

**认领规则**：开工前在标注文件里给要做的函数加一行，`owner` 填自己、`status` 置 `claimed`；完成并验证后置 `reversed` 或 `verified`，在 `notes` 里写结论和对应的 Ra3Source 符号，然后重新生成台账。**不要改别人 owner 非空的行。** 并行分支各自只追加自己的行，合并时冲突最小。

## 5. 并行工作规则

- 每个写代码的 agent 在自己的 git worktree 分支里干活（分支名 `recon/<module>`），完成后由主控合并，避免互相覆盖。
- 只改自己负责的模块目录和台账里自己认领的行；跨模块需求通过在对方模块头文件里加接口来表达，不直接改对方实现。
- 少测试，多推进（AGENT.md）。每次提交前：构建零警告，已有测试不能坏，用一次实际数据运行（如 `ra3tool` 命令）确认新功能可用。新测试只给关键的格式和容易回归的行为写，不追求覆盖率，不做耗时的全量回归。
- 游戏目录 `D:\Game\Ra3\Game` 只读，任何情况下不写入、不移动。

## 6. 构建与验证

在 `Ra3Source/` 下（VS2019 自带 CMake 3.19，注意等号写法）：

```
cmake --preset=win32
cmake --build build/win32 --config Debug
cd build/win32 && ctest -C Debug            # 3.19 不支持 --test-dir
```

CMake 在 `C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`，需要时加进 PATH。`ra3tool paths|sku|mount|big|find|unpack-check|streams|manifest` 可对实际数据做端到端检查。Ghidra 分析用 `tools/ghidra/run_headless.ps1`（需 `GHIDRA_INSTALL_DIR`、`JAVA_HOME`；默认只读）。

## 7. 参考资料与授权边界

EA 已以 GPL v3（附加条款）公开两份材料，克隆在 `work/references/`（已 gitignore）：

- `work/references/generals` — C&C Generals / Zero Hour 完整引擎源码。RA3 引擎由它演变而来，目录结构基本一致，可作**结构与格式参照**。RA3 改动很大，不能直接照搬。
- `work/references/modding/Red Alert 3` — RA3 的源 XML、XSD 数据结构定义、着色器、地图库。游戏里的 `.manifest/.bin` 资源流就是从这些 XML 按 XSD 编译来的。

**授权要求**：用户已同意使用这些 GPL 材料。凡是参照 GPL 源码的结构或格式写代码，要写原创实现，并在注释里注明参照来源文件；凡实际拷用了 GPL 代码或数据，Ra3Source 整体必须按兼容 GPL v3 的方式发布。正式的 LICENSE/NOTICE 待首次纳入 GPL 派生内容时补齐。

## 8. 状态板

- [x] core、io 完成并验证
- [x] 函数台账（48,985 行）、总体报告；人工状态移到 ledger_annotations.tsv，源路径识别 148 → 191 个文件
- [x] 引入 GPL 参考
- [x] 资源流格式：清单/重定位/导入/cdata 布局在全部 257 组上校验通过，并与原程序 FullAssetList、ResourceManagerFileStream 对上（docs/design/asset_system.md §3）
- [x] 虚表恢复：补建约 11,022 个虚函数，导出 2,977 个虚表清单（docs/analysis/ra3_1.0/vtables.tsv）
- [x] text（CSF）模块 — 已合并进 main，实测 gamestrings.csf 解析出 10,401 条字符串，单测 26、数据测试 5、CTest 2 全通过
- [x] asset 模块里程碑 1（四件套读取与资源目录）— 已合并；`ra3tool streams` 2 秒校验 257 组 / 118,232 条资源，0 问题；RefPack 全/前缀解码共用一个实现，14 个档案 5,203 个压缩条目 0 失败；单测 32、数据测试 8 全通过
- [x] 模块映射文档（docs/design/module_map.md）— 统计基于修正前的台账，待刷新
- [x] asset 里程碑 2（recon/asset2，已合并）：流式 RefPack `io::DecodedStream`（滑窗按原程序 0x009F7620：flag 0x40 为 16 KiB，否则 128 KiB）、`BigArchive` 区间读、严格重定位/导入表与只读 `AssetInstance`；`ra3tool streams --deep` 全量 118,232 条资源 0 问题（重定位 6,849,878、导入 259,832；目标可等于实例大小；导入值按 1 起的引用序号，不一定连续）。合并时主控发现一次性/前缀解码改走流式预取后会越过源缓冲读 64 KiB（hook 传入无界长度，游戏内可能崩溃），已恢复按需读取的内存解码器并加保护页单测
- [x] 注入可行性研究 → 路线 A 成立。hook 库、注入器、RefPack hook 已合并（主控重写跳转与补丁事务）；真实游戏 shadow 38/38 一致，replace 38/38 成功并正常进主菜单（docs/design/injection.md §5.4）。未覆盖：进入对局
- [x] 路线 A 通用化：`src/hook/dll/Targets.cpp` 目标表，按目标开关 off/shadow/replace，全部或全不安装。第二个目标 namehash（0x00417DB0）实机 shadow 5579/5579、replace 5554/5554，两目标同时替换正常进主菜单
- [x] 第三个 hook 目标 pathcompare（0x009F3650，`ra3::io::comparePathNames`，复刻非 stricmp 的排序）：实机 shadow 227,359/227,359，三目标同时 replace 正常进主菜单
- [x] 进入对局的实机覆盖：三目标 shadow 在遭遇战中 refpack 58/58、namehash 117,975/117,975、pathcompare 451,773/451,773；三目标 replace 同样进入对局并正常运行（injection.md §5.4 运行 8、9）
- [x] 第四个 hook 目标 refpackprobe（0x009F74F0，`ra3::io::probeRefPackSize`，flag 0x01 时返回压缩大小的怪癖照搬）：离线预言机 4,024/4,024；遭遇战中 shadow 556/556，四目标同时 replace 进入对局正常（injection.md §5.4 运行 10、11）
- [x] 资源类型目录与名称哈希 — 已合并（recon/types）。`asset_types.tsv` 266 个类型全部有名（9 个只在程序里出现的用 XSD 标识符反查）；instanceId = 转小写的 SuperFastHash 变体（0x00417DB0），typeId = 不转小写的同一算法；C++ `ra3/asset/NameHash.h`，`ra3tool streams` 全量 118,232 条两项 0 不等。typeHash 语义未知（docs/design/asset_types.md）
- [x] Ghidra 函数导出刷新（随 hookcat 合并完成）
- [x] 离线差分预言机 `tools/analysis/emulate.py`：用 unicorn 从磁盘映像执行单个 clean 叶函数（只映射 .text/.rdata/.data，SecuROM 节不映射），可把 IAT 槽绑到 Python 桩；namehash 随机 3000/3000 与参考实现一致，pathcompare 与单测一致
- [x] hook 移植第一批（recon/ports1，已合并）：新 `common` 模块 17 个函数（限长/普通字符串复制·追加·比较·长度·小写复制·反向查找、字节扩宽、定长字节比较、两种长度前缀串比较、小写 FNV-1a、UTF-8 解码/编码/前移），预言机差分主控复跑 17×约 2,007 例 0 不一致；合并时主控收紧了限长复制/追加的 shadow 访存范围。实机：13 个在遭遇战中被调用，shadow 全部一致、23 目标同时 replace 对局正常（injection.md §5.4 运行 12、13）；stringfindlast、bytewiden、utf8decode、utf8encode 在该流程中未被调用，仍只有离线证据
- [ ] 类型化资源访问：由 XSD 模型生成全部已验证类型的布局兼容 C++ 结构、枚举和反射表，`ra3tool dump`/`types-check` 全量逐字段校验 — 子 agent，分支 recon/typed。第一轮 132 类型 44,016 实例 0 失败；第二轮跟进 layout2 的新字段种类（token、可空复合指针、多态指针/列表）后再合并
- [x] BigHandler 档案文件系统（recon/bigfs，已合并）：全局实例槽 `0x00DEB69C`（`0x004D2490` 分配 0x20 字节派生对象），档案链表、每档案索引与按哈希排序的查找表布局（`ra3/io/GameBigLayout.h`，docs/design/big_filesystem.md）；只读查找 `0x009F6B90`/`0x009F6D00` 移植为在游戏结构上运行的 C++，Unicorn 在真实 Misc.big 目录上 2,207 例 0 不一致（主控复跑 `tools/analysis/ports/verify_big_lookup.py` 与 `bigcheck` 一致）。离线 `BigArchive::find` 改为区分斜杠方向（与游戏一致），`ArchiveSet` 负责把 '/' 规范成 '\'。实机 shadow：bigfindentry 2,632,470/2,632,470、bigfindhash 4,627/4,627；replace 后对局正常。这两个目标只认显式键
- [x] 模块归属工具 `tools/analysis/attribute_modules.py` → `module_estimates.tsv`：台账/虚表/64 KiB 邻近三级归属，留出验证虚表 4/4、邻近 35/38（92%）；仍未归属 26,050。模块映射已按新台账刷新。asset 的邻近估计（961，留出 1/2）偏粗，慎用
- [x] 虚表 → 类（recon/vclass，已合并）：没有 RTTI；164 个资源处理器是 5 槽接口（析构 / create / 原位复制 / destroy / 缺失原型解析），分属 3 个模板族（多态 119、单例设置 20、4 字节值 9）加少量特化；`classes.tsv` 3,126 行、805 条候选继承边（启发式）。注意 `vtables.tsv` 是“连续代码指针段”，相邻虚表会被合并（docs/design/class_map.md）
- [x] 精确虚表边界（recon/vsplit，已合并，主控复跑输出逐字节一致）：clean `.text` 的 vptr 写入、立即数引用与数据指针把 2,977 个连续段中的 955 个切开，得 7,442 个候选虚表；164/164 资源处理器恰为 5 槽，BigHandler 基/派生边界吻合；`classes.tsv` 7,442 行、3,721 条候选继承边（启发式）；无 RTTI COL。1,092 段无独立起点证据，41 个 direct-slot 边界可能是表中槽访问（docs/design/class_map.md）
- [x] BigHandler 第二层（recon/bigfs2，已合并）：9 个辅助函数（BE 32/64 读取、格式判别、路径 hash、目录头/条目数/尾标识、stored size、绝对数据偏移）移植并挂 hook，Unicorn 各 1,000/1,000，主控复跑一致；实机 32 目标 shadow 全部一致（bigformat 1,077 万次等），32 目标 replace 对局正常（injection.md §5.4 运行 15、16）。分配器 vtable（+0x04 临时 / +0x08 带标签 / +0x0C 释放）、文件对象 read `+0x14`/seek `+0x1C`、同步对象 acquire `0x00A216A0`/release `0x00A21760` 已识别；Seek 字段转移 1,000/1,000 但未挂
- [x] BigHandler 第三层（recon/bigfs3，已合并）：分配器（+0x04 临时 `thiscall(size, tag, 0, align, 0)`、+0x08 带标签、+0x0C 释放）、档案文件对象（read `+0x14` 5 dword、seek `+0x1C` 6 dword，被调方清栈）与同步对象 ABI 经 Unicorn 桩核实；更正 `GameBigFile +0x20` 为逻辑长度、`+0x28` 为压缩源长度（明文为 0）。Seek（整 0x40 快照比较）与 Close 移植并挂 hook：实机 shadow bigseek 3,682/3,682，replace 下 Seek 3,574 次、Close 341 次由 Ra3Source 完成，对局正常（injection.md §5.4 运行 17、18）
- [ ] BigHandler 第四层：Open/Read/InflateRange 与枚举。需逐成员 + 压缩档案随机序列差分、0x70 inflater 状态等价、0x110 枚举结果布局 — 子 agent，分支 recon/bigfs4，work/ghidra_bigfs
- [x] 资源布局第三轮（recon/layout3，已合并，主控全量复跑输出逐字节一致）：AudioEvent 46 → 5,234/5,234（156 字节根、三组 12 字节音频引用列表、紧凑 range）、W3DAnimation 4,099 → 5,924/5,924（36/44 字节 Timecoded/Delta 通道，clean 构造函数锚定偏移）、FXParticleSystemTemplate 2,740 → 4,110/4,110（多态模块指针、Event 指针数组、EmitterSound 间接 import）；总覆盖 160/257 类型、101,583/118,232 实例，原 157 类型无回归；生成视图 160 类型 283 结构 45 枚举，`types-check` 74,193 实例 19,915,581 字段 0 失败。W3DMesh（VertexData/FXShader 常量）、GameObject（Tokenized 模块对象图）、ObjectCreationList（248 字节 nugget）仍排除
- [x] 模拟确定性第一步：随机数（recon/simrng，已合并，docs/design/simulation_determinism.md）。RA3 仍是 6×32 位加进位发生器，client 0x00CB73F0、audio 0x00CB7408、logic 0x00CB7420、logic 基础种子 0x00DF60CC；与 Generals 的差异：logic 整数区间在 `low >= high` 时直接返回 low 且不推进状态。`src/sim` 移植步进 0x005A9310、统一播种 0x005A93B0、三个整数区间 0x005A94C0/0x005A94F0/0x005A9520，主控复跑 oracle 400,000 + 10,000 + 3×10,006 例 0 不一致；5 个 explicitOnly 快照式 hook。实机 AI 遭遇战 shadow：randomstep 256,113/256,113、logicrandomint 23,888/23,888、client 248/248、audio 278/278；replace 后对局正常（injection.md §5.4 运行 22、23）；randominit 在注入后未被调用。返回 x87 ST0 的三个实数区间 helper（0x005A9550/0x005A95A0/0x005A95F0）暂未挂
- [ ] 对局状态 CRC：没有找到 Generals `XferCRC::addCRC` 的 clean 对应物（0x004D4EA0 的 `CRCBuffer::PREALLOCATE_SIZE` 只是脱同步报告缓冲区容量，0x0092A0D0 只哈希 C 字符串），未做推测性实现
- [x] 运行时字符串表（recon/csf，已合并）：`TheGameText` 0x00DEEA54，管理器 0x6C 字节，override → primary → map 三张按 `_stricmp` 排序的表（docs/design/game_text.md）；只读查找 0x00539080/0x0053B500 移植，主控补充近似未命中与三表叠加后 Unicorn 原函数与 C++ 各 208,782 例 0 不一致；实机 shadow 707/707、673/673，replace 后界面文字正常。CSF 加载器 0x0052FCE0 为 inner-patched，未越界分析
- [x] hook 移植第二批（recon/ports2，已合并）：`src/math` 17 个 SSE/x87 原语与 CRC-32 C 字符串（docs/design/math_library.md）。oracle 在 CW 007F/027F/037F 下 Debug+Release 全部 0 不一致；返回 x87 ST0 的 4 个目标用 naked 入口保留 80 位结果。实机：运行 19 暴露 4 个包装缺少 EAX 结构体返回值（normalize2 调用者崩溃），修复后运行 20 shadow 53 目标 0 不一致、运行 21 replace 带 AI 对局正常；7 个目标 live verified，游戏 CW 0x007F，其余 10 个在遭遇战中未被调用
- [x] hook 候选目录（recon/hookcat，已合并，主控复跑计数一致）：40,333 个 clean 函数中 Tier 1 6,187、Tier 2 1,197（docs/design/hook_candidates.md、hook_candidates.tsv）；已上线的三个目标都在候选内。注意 Ghidra 推断的调用约定与裸 `ret` 常冲突，以 `return_forms` 为准。Ghidra 函数导出已刷新（48,986 行，补入 0x004B1C80；旧导出备份在 work/analysis/ghidra/pre_hookcat）
- [x] 资源二进制布局（recon/xsdlayout，已合并，主控全量复跑结果一致）：由 XSD 推导实例布局（根 4 字节 token；基类 → 属性 → 元素；字符串/列表 `{长度/数量, 相对指针}`；引用为 1 起的导入序号），完全通过 132/257 类型、87,592/118,232 条实例（docs/design/asset_layout.md）
- [x] 资源布局第二轮（recon/layout2，已合并）：tagged 标量、枚举 flags、可选复合指针、choice/多态指针数组、Poid/Tokenized 子规则；W3DContainer 与 W3DHierarchy 全量通过；总覆盖 157/257 类型、93,200/118,232 实例；程序锚定 0x00673A50 粒子模块与 0x009B0540 W3D 通道 typeId 工厂，剩余缺口见 asset_layout.md
