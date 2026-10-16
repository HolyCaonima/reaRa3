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
- [ ] asset 里程碑 2：流式 RefPack、重定位/导入表、全量深度校验 — 子 agent，分支 recon/asset2
- [x] 注入可行性研究 → 路线 A 成立。hook 库、注入器、RefPack hook 已合并（主控重写跳转与补丁事务）；真实游戏 shadow 38/38 一致，replace 38/38 成功并正常进主菜单（docs/design/injection.md §5.4）。未覆盖：进入对局
- [x] 路线 A 通用化：`src/hook/dll/Targets.cpp` 目标表，按目标开关 off/shadow/replace，全部或全不安装。第二个目标 namehash（0x00417DB0）实机 shadow 5579/5579、replace 5554/5554，两目标同时替换正常进主菜单
- [x] 第三个 hook 目标 pathcompare（0x009F3650，`ra3::io::comparePathNames`，复刻非 stricmp 的排序）：实机 shadow 227,359/227,359，三目标同时 replace 正常进主菜单
- [x] 进入对局的实机覆盖：三目标 shadow 在遭遇战中 refpack 58/58、namehash 117,975/117,975、pathcompare 451,773/451,773；三目标 replace 同样进入对局并正常运行（injection.md §5.4 运行 8、9）
- [ ] 下一批 hook 目标：RefPack 大小探测 0x009F74F0（等 asset2 合并后按原语义移植）、hook 候选目录里的纯叶函数
- [x] 资源类型目录与名称哈希 — 已合并（recon/types）。`asset_types.tsv` 266 个类型全部有名（9 个只在程序里出现的用 XSD 标识符反查）；instanceId = 转小写的 SuperFastHash 变体（0x00417DB0），typeId = 不转小写的同一算法；C++ `ra3/asset/NameHash.h`，`ra3tool streams` 全量 118,232 条两项 0 不等。typeHash 语义未知（docs/design/asset_types.md）
- [ ] Ghidra 函数导出刷新（types agent 用 -Save 补建了 0x004B1C80 等，台账尚未收录）
- [x] 模块归属工具 `tools/analysis/attribute_modules.py` → `module_estimates.tsv`：台账/虚表/64 KiB 邻近三级归属，留出验证虚表 4/4、邻近 35/38（92%）；仍未归属 26,050。模块映射已按新台账刷新。asset 的邻近估计（961，留出 1/2）偏粗，慎用
- [x] 虚表 → 类（recon/vclass，已合并）：没有 RTTI；164 个资源处理器是 5 槽接口（析构 / create / 原位复制 / destroy / 缺失原型解析），分属 3 个模板族（多态 119、单例设置 20、4 字节值 9）加少量特化；`classes.tsv` 3,126 行、805 条候选继承边（启发式）。注意 `vtables.tsv` 是“连续代码指针段”，相邻虚表会被合并（docs/design/class_map.md）
- [ ] 精确虚表边界：用构造/析构写入点与直接虚调用重新切分 vtables.tsv 的连续段
- [ ] hook 候选目录：全部 clean 函数按叶/纯度/序言可搬/调用约定分级，并刷新 Ghidra 函数导出 — 子 agent，分支 recon/hookcat，独占 work/ghidra
- [x] 资源二进制布局（recon/xsdlayout，已合并，主控全量复跑结果一致）：由 XSD 推导实例布局（根 4 字节 token；基类 → 属性 → 元素；字符串/列表 `{长度/数量, 相对指针}`；引用为 1 起的导入序号），完全通过 132/257 类型、87,592/118,232 条实例（docs/design/asset_layout.md）
- [ ] 资源布局第二轮：Audio/W3D/Tokenized typeGroup 与多态 choice（AudioEvent 0/5,234、W3DMesh、GameObject、FXParticleSystemTemplate 等），结合原程序处理器与 ReloDetokenizers
