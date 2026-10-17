# 交接文档（2026-10-17）

新 agent 开工顺序：先读仓库根目录 `AGENT.md`（用户规则），再读本文，然后按需查
`docs/RECONSTRUCTION.md`（总控、状态板）和 `docs/design/injection.md`（hook 目标表与
全部实机运行记录）。历史对话记录在
`C:\Users\yuletian\.cursor\projects\D-Game-Ra3-RevRa3\agent-transcripts\`，只在需要
查细节时检索，不要通读。

## 1. 目标与用户的最新决定

- 把 `ra3_1.0.game`（32 位 PE，带 SecuROM）逐步重建成可维护的 C++（`Ra3Source`）。
  主路线是路线 A：窗口出现后把 `ra3hook.dll` 注入运行中的原版游戏，逐个函数用我们的
  实现替换（off / shadow / replace），游戏始终可进遭遇战。
- 用户只给宏观要求，全程由 agent 自主完成，不要频繁提问。
- **速度优先**（2026-10-17）：实机游戏验证能省就省。单批函数靠离线 oracle 证明正确；
  累计**几百到几千个**新函数后，才跑一次合并的实机 shadow + replace。
- **子 agent**：用户 11:18 决定后续新批次用 `gemini-4-argon-medium`（速度/成本优先，
  主控继续负责严格复验）；保持 10 个并行，各自在 git worktree 分支（`recon/<module>`）
  里干活，由主控合并。11:18 前已经启动的 10 路是 `inherit`，不为换模型中断在途工作。
- **受保护代码纳入范围**（2026-10-17，用户选定的边界）：
  - 范围是**游戏自己的**受保护函数（台账 `securom` 为 `inner-patched` 432、
    `entry-lost` 653、`entry-recoverable` 121、`patched` 8）。
  - 证据以**运行中进程**为准：注入后只读地读取运行时已解密/已重定位的原生 x86 代码，
    加上动态追踪的输入输出，据此重建。
  - 被虚拟化（内存里没有原生 x86 真身）的函数只做**行为等价重建**（参照 Generals 源码、
    clean 的周边代码与调用点、动态输入输出），**不逆向 SecuROM 自身的 VM/DRM**。
  - 不做静态脱壳或解密文件，不生成、不保存可执行的脱壳映像，不修改游戏目录。运行时转储
    只放在 `work/`（已 gitignore），只用于分析。
  - SecuROM 自己的代码（`Stext/Sitext/Srdata/Sdata/Sidata/.securom`，约 7,400 个函数）
    仍不在重建范围内。
  - hook 受保护函数前也必须先通过 shadow 对照。若破坏可运行性或触发完整性检查，就停下、
    记录，**不绕过**。

## 2. 硬约束（来自用户，原样遵守）

- `D:\Game\Ra3\Game` 只读：不改、不移动、不写入任何文件。
- data 不拷贝：通过配置文件直接引用 `D:\Game\Ra3\Game` 里的 data。
- 只在游戏启动后、首个窗口出现时注入。
- 少测试，多推进：每次合并要求零警告构建、已有测试不坏、一次真实数据运行（离线即可）。
  只给关键行为写新测试。
- 已获用户同意使用 EA 以 GPL v3 公开的 Generals 源码与 RA3 modding 材料
  （`work/references/`）作参照。写原创实现，注释注明参照文件。

## 3. 现状（截至 AITNGuard 合并）

- **台账** `docs/analysis/ra3_1.0/function_ledger.tsv`：48,986 行，其中 verified 66、
  reversed 317、unanalyzed 48,603。`securom` 分类：clean 47,772、entry-lost 653、
  inner-patched 432、entry-recoverable 121、patched 8。扣掉第三方后约 4 万个函数要重建。
- **hook**：149 个目标（其中 105 个只认显式键）。原有 86 个全部 replace 时，Release
  遭遇战带 Easy AI 正常运行 2 分钟、无崩溃（injection.md §5.4 运行 34）；新加的
  Player/Net/W3DView/FXPS/StringBase2/AITNGuard 共 63 个目标只有离线证据，尚未做合并实机
  shadow/replace。
- **已完成模块**：
  - core（配置/INI/SkuDef）
  - io（BIG 档案、RefPack、BigHandler 四层：查找/打开/读取/seek/枚举）
  - text（CSF、GameText 查询）
  - asset（清单/四件套/重定位/导入全量校验，XSD 推导布局 160/257 类型）
  - common（字节串函数、引擎字符串类 StringBase、CRC）
  - math（SSE/x87 原语）
  - sim（随机数模块全部 clean 函数：三条流、整数/实数区间、RandomVariable、种子 CRC）
  - Player/ScoreKeeper 批次：35 个接口，其中 34 个新台账 reversed，14 个显式 hook；
    Debug/Release oracle 各 13,000 例零差异
  - net transport 批次：28 个 clean 函数，9 个 hook；两配置 oracle 各 14,000 例零差异
  - render 批次：W3DView/Rain 37 个接口，其中 19 个逐入口 reversed，11 个 hook；
    两配置 oracle 各 25,850 例零差异
  - gameclient FXPS 批次：45 个 clean 函数，15 个显式 hook；两配置 oracle 各
    25,100 例零差异；`fxpsgpuspeed` 用 naked + 十字节缓冲保持完整 x87 ST0
  - common StringBase2 批次：50 个重建入口（其中 48 个对应 Ghidra 台账函数），6 个
    显式 hook；两配置 oracle 各 4,000 例零差异，含 112 个嵌入 NUL 与非零容量尾部
    对抗场景；受保护六字节站点只按已观察的指针清零效果建模
  - gamelogic AITNGuard 批次：50 个 clean body 仅作类族库存，其中 8 个叶入口完成
    C++ 重建与显式 hook；两配置原生行为 oracle 各 4,000 例零差异，语义突变成功检出
- **未完成或卡住**：
  - 对局状态 CRC：种子 CRC `0x005A9390` 的唯一调用者 `0x004D7490` 是 entry-lost，按
    Generals 推断即每帧 `GameLogic::getCRC`。按新规则可用运行时证据重新处理。
  - 类型化资源访问（recon/typed）未合并。
  - 少数 hook 在遭遇战里未被调用（logicrandomvariable、几个 BIG 枚举入口、
    stringfindlast 等），只有离线证据。
- **当前十路并行**（用户 11:14 明确允许扩到 10 路）：`recon/terrainlogic`、
  `recon/aistates`、`recon/audioevent`、`recon/objectlogic`、`recon/uicontrols`、
  `recon/weaponlogic`、`recon/behavior6b`、`recon/scripts82`、`recon/render8e`、
  `recon/gameclient92`。每支独立 worktree，完成后主控逐支复验合并；构建/oracle 可并行，
  主分支合并仍串行，避免注册表和 CMake 冲突污染。
- **后续模型分流**（用户 12:10 决定）：复杂 ABI、受保护边界与重型对象簇继续优先
  Gemini 4 Argon Medium；简单叶函数、访问器与清晰类簇在 Gemini 4 Argon Medium 和
  Grok 4.7 High Fast 间 1:1 分配。当前已有两个 Gemini 补位，因此接下来两个简单补位
  先用 Grok，再交替。主控统一记录首轮构建/oracle 通过率、误计数或猜测、返工次数、
  完成耗时和最终有效重建数；没有账单遥测时不推断精确 token 成本。Grok 的前两批只分配
  clean 低风险簇，并提高证据门槛：逐项核对 `reversed` ↔ C++ ↔ shipped 行为，主控额外
  抽查反汇编、ABI、hook 前缀与写集；归属模糊、身份指纹冒充语义或重复认领立即退回。
  连续两批无实质返工后，再降到与 Gemini 相同的审查强度。
- **验收按风险分级**（用户 12:13、12:15 决定，避免重复验证）：
  - 低风险直线访问器/常量写入：2–4 个有区别的场景，覆盖正常值、边界值、ABI、实际写集
    和 sentinel 即可；不做随机数量竞赛。
  - 中风险循环、范围、别名和状态谓词：6–12 个最小分支矩阵，覆盖空、上下界、自别名/
    部分别名及每条控制流；只有发现差异才扩样。
  - 高风险 x87/SSE、构造器、复杂状态转换、全局或 stub 依赖：先做 12–24 个针对性场景，
    按风险比较完整 ST0、必要控制字、布局 sentinel 与调用轨迹；无需统一跑满大样本。
  - 每批只要求一个非平凡语义 mutation（共享实现明显不同才增加）；无需逐函数 mutation。
    完整语义 oracle 默认只跑 Release。Debug 只要求编译；仅浮点优化、UB/配置敏感代码才
    增加 Debug oracle 或第二控制字，单纯 `fld m32`/字段读取不重复测多个控制字。
  - 分支开发期只构建受影响目标；交付时构建 Debug/Release 改动目标并跑 Release oracle。
    只有修改共享框架、CMake、hook 或测试注册时才跑相关 CTest，不机械跑两配置全套。
    主控默认静态审查后直接合并，在集成态做一次全量 Release 构建、Release CTest、
    Debug 增量编译和新批 Release oracle；合并前独立动态复验只留给高风险批次。
  - 既有模块 oracle 仅在共享实现或依赖被修改时回归，不因单纯注册新模块而全跑。
  - 身份指纹只算 inventory，不算语义重建；实机 shadow/replace 继续按多批累计执行，
    不为每个小批次单独启动游戏。
- **git**：本地 `main` 领先 `origin/main` 20 个提交，未 push（用户没要求 push）。

## 4. 建议的下一步（按优先级）

1. **一次运行时采集，之后全部离线做。** ✅ 已完成（运行 35，`work/runtime_dump/run35`）。
   `ra3hook.dll` 加了只读转储模式（`dump=DIR`，见 §6）：全程 hook 全关，只扫描 `.text`
   里槽在 `.securom` 的 `jmp [slot]` 站点，轮询槽值变化，并在 0/45/120/240 秒各转储一次
   `.text/.rdata/.data/.securom` 原文与槽表。结论：**游戏不就地改 `.text`**（四次快照与磁盘
   逐字节相同），而是惰性地把槽从 `.securom` 的 VM 桩解析成真实目标——首次执行某站点时
   SecuROM 把真目标写进槽。`.securom` 节自身在运行时含：解析后的槽表（`0x018EDxxx` 段）与
   被重定位/解密出来的函数体（`0x0190xxxx` 段）。
   - `tools/analysis/securom_sites.py` 把一次或多次转储合并成
     `docs/analysis/ra3_1.0/securom_sites.tsv`：每个站点的槽、磁盘目标、运行时目标、目标所在
     节、分类（`body` 整个函数体 / `cave` 只跑被替换指令再跳回站点后 / `unresolved` 本次没执行
     或仍虚拟化）、cave 的被替换指令与跳回地址。运行 35 里 656 个槽被解析，其中 282 个
     entry-site thunk 恢复出真实函数体（266 entry-lost + entry-recoverable 等）。
   - **原生 vs 虚拟化**：416 个解析出的 `body` 目标（261 在 `.text`、155 在解密出的
     `0x0190xxxx` 段）全是可解码的原生 x86，无一是 VM 分派（`push imm; call 0x1132580`）。
     即：本次采集中被执行到的受保护函数都恢复成了原生体，真正“虚拟化/未执行”的就是那些
     `unresolved` 槽。扩大采集覆盖面（多跑些玩法）会把更多 `unresolved` 变成 `body`/`cave`。
2. **扩展离线 oracle**：✅ 已完成。`tools/analysis/emulate.py` 的 `Emulator(capture=快照目录)`
   会把该快照的 `.securom` 原文叠加映射到 `0x01882000`——它同时带着解析后的槽表和解密后的
   函数体，于是 `jmp [slot]` 能正确跳转、`0x0190xxxx` 的体能执行、cave 读的 `.securom` 常量
   也拿到运行时值。受保护函数就能和 clean 函数一样做 Unicorn 差分。`entry_targets(sites_tsv)`
   返回 `thunk VA → 真实体 VA`：移植 entry-lost 函数时要 oracle 真实体，而不是那 6 字节 thunk。
   `.text/.rdata/.data` 仍取磁盘（运行时 `.text` 与磁盘逐字节相同；`.data` 保持装载期值）；
   SecuROM 自己的节（`Stext` 等）永不映射。
3. **大批量并行移植**：🚧 持续进行。保持 10 个 `gemini-4-argon-medium` 子 agent，
   每个负责一个成块子系统，每批
   50–200 个函数，每批带 oracle 脚本。优先成块的 gamelogic/gameclient 簇，例如
   `0x0065xxxx–0x0066xxxx` 粒子/特效（大量调用 client RandomVariable），用 Generals 源码
   命名，不要挑零散的匿名字段访问器。已合并四批共 126 个新 reversed；主控每次合并均跑
   零警告 Debug/Release、两套 ctest、该批 oracle 和所有已合并模块的 Release oracle。
4. **攒到几百个函数后**，按 §6 跑一次合并的实机 shadow，再跑一次 replace。
5. 结构性整理（可选）：`src/hook/dll/Targets.cpp` 已 3,479 行，宜按模块拆分；
   `work/probe` 里的实机脚本不在 git 中，宜挑必要的移进 `tools/`。

## 5. 构建、测试、离线验证

- Win32 C++17，VS2019 自带 CMake 3.19（要加到 PATH：
  `C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`）。
- 在 `Ra3Source/` 下：`cmake --build build/win32 --config Debug|Release`；测试用
  `cd build/win32; ctest -C Debug`（3 项）。实机用 Release；重建 Release 前先关游戏
  （DLL 被锁）。
- **离线 oracle**：`tools/analysis/emulate.py` 用 Unicorn 只映射磁盘映像的
  `.text/.rdata/.data` 执行单个原函数（不映射 SecuROM 节）。
  - 接口：`alloc/read/write/call(addr, args, ecx=)`、`call_float80(...)` 取完整 80 位
    ST0，桩用 `bind_import/bind_function/bind_stub`，`reset_data()` 复原 `.data` 和堆。
  - **受保护函数**：`Emulator(capture="work/runtime_dump/run35/snapshot_3_240s")` 叠加该快照
    的 `.securom`（解析后的槽 + 解密后的体），受保护函数即可照常 `call`。用
    `entry_targets("docs/analysis/ra3_1.0/securom_sites.tsv")[thunk_va]` 取真实体地址再
    `call`（entry-lost thunk 本身也能 call，会经解析后的槽跳进体）。详见 `securom_sites.py`
    文件头与 §4。
  - 各模块的 oracle 在 `tools/analysis/ports/verify_*.py`，与 C++ 端
    `tools/portcheck`（行协议）或 `tools/stringcheck`、`tools/bigcheck`、
    `tools/textcheck` 对比。
- **Unicorn 已知偏差**：两个 NaN 操作数时选哪个 NaN 与硬件不同（payload 不比，留给实机
  或本机对照）；`fld m32` 不会把 signaling NaN 变成 quiet NaN（真硬件会，oracle 用
  portcheck 的 `x87roundtrip` 本机往返取硬件结果）。
- **portcheck 协议坑**：按空白切分，空 token 会让后续参数错位（空缓冲用 `.`，空指针
  用 `-`）；Debug 版异常会弹 `abort()` 模态框，导致进程挂住、互相等待。若两进程 CPU
  时间几乎不涨，就是卡死了。
- 辅助脚本：
  - `tools/analysis/disasm.py <VA> --hook-only` 给出 expected16、stolen 长度、返回形式。
  - `work/probe/disasm_fn.py <VA> <十进制长度> all` 反汇编。
  - `containing.py` 查所属函数，`read_dwords.py` 读映像数据。
  - `callers_eax.py [--only KIND]` 查调用者是否用 EAX；部分寄存器写入会误报。

## 6. hook 目标与实机验证流程

- **目标表** `src/hook/dll/Targets.cpp`：
  - 条目格式 `{key, rva, expected16, stolen, replacement, state, explicitOnly, rel32Offsets}`，
    RVA = VA − 0x400000。
  - 配置 `ra3hook.ini` 每行 `key=mode`，`mode=` 设所有非显式目标的默认值。
- **ABI 规则**：
  - thiscall 包装写成 `__fastcall(this, void* edx, ...)`。
  - 返回结构体时 EAX 必须是输出指针（运行 19 因此崩过）。
  - x87 返回值必须走 naked 入口加十字节缓冲（`fstp/fld tbyte`），不能经过 C++ `float`。
  - x86 MSVC 的 `float` 返回会经 ST0，从而把 signaling NaN 变成 quiet NaN；要传精确位，
    用位或单成员结构体。
  - 内联汇编里 `high`、`low`、`single`（以及 `length`、`size`、`type` 等 MASM 运算符）
    不能当变量名。
- **stolen 前缀**：必须位置无关，或在 `rel32Offsets` 里列出要重定位的 `E8/E9 rel32`；
  函数内不能有跳回 stolen 范围的分支。shadow 只读写原函数会读写的内存，原函数不写的
  字段不比较。
- **实机一次运行**（攒够函数才跑）：
  1. `.\work\probe\next_run.ps1 -Mode shadow|replace -Config Release -Targets (键 | % {"$_=模式"})`
  2. 后台运行 `.\work\probe\inject_watch.ps1 -Bin D:\Game\Ra3\RevRa3\Ra3Source\build\win32\bin\Release -TimeoutSeconds 180`
  3. 启动游戏：`Start-Process D:\Game\Ra3\Game\RA3.exe -ArgumentList '-win' -WorkingDirectory D:\Game\Ra3\Game`
  4. 等到出现 `ra3inject exit=0`，再等约 45 秒。启动时的 "Failed to connect to
     servers" 对话框是正常的。
  5. `work\probe\click.ps1 -X -Y` 依次点 OK (507,352) → Versus (300,267) →
     Skirmish (296,310) → BEGIN (675,548)，坐标按 1024 宽截图换算。
     截图用 `shot.ps1 -Out <绝对路径>`。
  6. 约 120 秒后检查：进程在且有响应；`Get-WinEvent` 应用日志中没有 ra3_1.0 的
     Id=1000 事件；截图里对局在跑。
  7. 复制 `%LOCALAPPDATA%\Ra3Source\logs\ra3hook.log` 为
     `work\probe\ra3hook_runN.log`，看每个键最后一条 `summary` 的
     calls/matches/mismatches。
  8. 关游戏：`Get-Process | ? ProcessName -match 'ra3_1.0|RA3' | Stop-Process -Force`。
- 显式键的完整清单见 injection.md §4.1 表格。全量 replace 用 `mode=replace` 加全部显式键。

## 7. 台账流程

1. 每批写分支注释 TSV（列 `address status owner module notes`，地址为小写 8 位 hex）。
2. 折叠进标注文件：
   `python work\probe\fold_annotations.py Ra3Source\docs\analysis\ra3_1.0\ledger_annotations.tsv <批次.tsv>`。
   遇到 clash（已有别的 owner）时手工接管，旧注释保留为 `(earlier: ...)`。
3. 实机通过后标记：
   `python work\probe\mark_live_verified.py <annotations> <shadow.log> <replace.log> '运行 N/M' key=addr ...`。
4. 重建台账：
   `python Ra3Source\tools\analysis\build_function_ledger.py --inputs work\analysis\ghidra --out Ra3Source\docs\analysis\ra3_1.0\function_ledger.tsv`。

状态约定：只有离线证据记为 reversed，通过实机 shadow/replace 后记为 verified。Ghidra 漏建的
函数（例如 `0x005A9390`）没有台账行，需要刷新 Ghidra 导出后才能标注。

## 8. 环境小坑

- PowerShell 控制台用 GBK 显示 UTF-8 会乱码，这不代表文件坏了，要核对就用 Read/Grep 工具。
  管道里接 `Select-Object -First N` 会截断上游输出，并返回 -1。
- `rea` 目录是逆向 MCP 服务（含 Frida、调试器、Rizin 工具），用前先查工具 schema。
