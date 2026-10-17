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

## 3. 现状（截至 skirm87 合并）

- **台账** `docs/analysis/ra3_1.0/function_ledger.tsv`：48,991 行，其中 verified 66、
  reversed 1,378、unanalyzed 47,547（受保护函数已 reversed：entry-lost 104、inner-patched 14、
  entry-recoverable 3）。`securom` 分类：clean 47,777、entry-lost 653、
  inner-patched 432、entry-recoverable 121、patched 8。扣掉第三方后约 4 万个函数要重建。
  Ghidra 漏建、但已被认领的函数登记在 `function_additions.tsv`（目前 5 个）。
- **hook**：321 个目标（其中 274 个只认显式键）。原有 86 个全部 replace 时，Release
  遭遇战带 Easy AI 正常运行 2 分钟、无崩溃（injection.md §5.4 运行 34）；新加的
  219 个目标只有离线证据，尚未做合并实机 shadow/replace。
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
  - audio AudioEvent 批次：50 个语义重建入口（其中 49 个对应当前 Ghidra 台账函数，
    `0x00559C30` 待刷新导出），15 个显式 hook；两配置 oracle 各 3,200 例零差异，
    完整核对 x87 ST0，并成功检出语义突变
  - gamelogic Behavior6B 批次：57 个 clean 语义入口，覆盖 contain、special ability、
    dock/update 与 physics 簇，8 个显式 hook；独立复核发现的问题已修正，主分支
    Release 集成 oracle 671 场景零差异
  - render Render8e 批次：53 个 clean 语义入口、15 个显式 hook；强化 oracle 在修正
    Hermite 运算顺序、condition 选择优先级和变换累加顺序后，两配置各 3,480 场景零差异；
    既有 W3DView/Rain 两配置各 25,850 场景零差异
  - gamelogic ObjectLogic 批次：29 个 clean 入口、3 个显式 hook；主分支 Release
    1,665 次观测零差异，29/29 mutation 与别名 mutation 均检出并恢复
  - net LobbySession 批次：46 个 vtable 字段叶函数，无 hook；Release 原生差分通过
  - gameclient MusicScripting 批次：51 个入口、20 个类型名显式 hook；刷新后 range
    重载、构造写集和 x87 语义修正后，Release 3,403 场景零差异，3 类 mutation 均检出
  - ui UIControls 批次：53 个入口、10 个显式 hook；Release 2,784 场景零差异，两个
    mutation 均检出并恢复
  - gamelogic Scripts82 批次：36 个语义入口、10 个显式 hook；64 项库存中其余 28 项
    保持 inventory-only，Release 1,693 场景零差异
  - gameclient DisplayExtent 批次：20 个 vtable 叶函数，无 hook；别名写入、部分 EAX、
    无符号溢出谓词及代表性 mutation 均通过
  - gamelogic AIStates 批次：51 个 Dozer/Supply/Unit state 入口、16 个显式 hook；
    Release 3,264 场景零差异，16/16 mutation 检出
  - gamelogic WeaponLogic 批次（`0x0069A250–0x0069FD30` update 模块簇）：90 个 clean
    入口（30 对模块名/键解析 + 30 个 Lifetime/Physics/Shield/Slaved 等行为），11 个显式
    hook；代码在 `ra3/gamelogic/UpdateModules.h`（与 ObjectLogic 的 `WeaponLogic.h`
    区分）。集成态 Release 1,890 次比较零差异，`lifetimeResume` 语义 mutation 检出
  - sim TerrainLogic 批次（`0x0085C5E0–0x008684C0`，锚点 `TerrainLogic.cpp:3158`）：
    43 个入口（几何/裁剪、Bridge 格子与高度、Waypoint、水面高度、findPositionAround），
    20 个 hook，其中 17 个 explicitOnly，仅 `pointintriangle2d`、`lineinregion2d`、
    `aligntonormalmatrix` 三个纯函数跟随默认模式。首轮交付的 hook 配置和文档字节与代码
    不符，主控修正后合并（`c76dae9`）。集成态 Release 2,868 场景零差异，`kSideOffset`
    语义 mutation 检出
  - net 命令消息批次（`0x005D67E0–0x005DB270`，Grok）：38 个 clean 入口，均为命令消息
    vtable 的调试文本槽（`<frame=%d, player=%d, type=%d>` + 类型名）与字段传输槽，
    代码在 `ra3/net/CommandMessage.h`；无 hook（影子调用会重复驱动文本/流虚函数）。
    主控逐条核对 42 个 vtable 槽位声明并抽查反汇编；集成态 Release 165 场景零差异，
    `+0x18→+0x1C` 写偏移 mutation 检出。24 个全局/分配器/断言门面入口留作清单行
  - common ThingTemplate 批次（`0x0078B6E0–0x0078E810`，Grok）：12 个 clean 入口（掩码
    谓词、模块记录查找/移除、注册表取值、构造器安装助手），5 个 explicitOnly hook；代码在
    `ra3/common/ThingTemplate.h`。所有权为 vtable `00C2927C` 槽位或构造器 `00795F70`
    （写入该 vtable）/ `ThingTemplate.cpp` 锚点的直接调用边，主控逐条核对。126 个候选因
    无所有权边、生命周期、EASTL、x87 等被拒。`0x0078E7C0` 为 Ghidra 漏建函数，已登记
    `function_additions.tsv`；同时补登记了早先 stringbase2/audioevent 认领的 3 个漏建函数
  - asset relo detokenizer 批次（首个受保护批次，Gemini）：46 个 entry-lost 入口，原生体
    来自 run35（36 个在 `.text` 空隙、10 个在 `.securom`），21 个字段 walker、20 个 clone
    助手和 5 个原语；代码在 `ra3/asset/ReloDetokenizers.h`，设计见
    `relo_detokenizers.md`（主控补写）。主控核对了 46 条 site/slot/body 与
    `securom_sites.tsv` 一致、`.text` 原生体不与台账函数重叠；交付说明里的字符串地址和
    "回调登记表"不实，C++ 注释中 41 个 slot 号错误，均已按二进制更正。集成态 Release
    166 场景零差异；两处语义 mutation 检出。无 hook
  - gamelogic AIStateMachine7C 批次（`0x007C0AD0–0x007CE500`，Gemini 接手）：45 个 clean
    入口（AIStateMachine 基类方法与 AIMoveTo/Dock/Harvest/GuardRetaliate/Enter/Exit 等 state
    的 onEnter/onExit/xfer），8 个 explicitOnly hook；代码在 `ra3/gamelogic/AIStateMachine7C.h`。
    类名经真实 vtable 的 `slot[2]` 名字串核实无误，但交付中的 vtable 地址、`aistates.cpp`
    等源路径和一处尾调用为编造，C++ 函数名与 hook 键整体错位；主控按二进制重写注释/文档
    证据，函数改为类名+方法名，hook 键改为短键（`f7810d1`）。集成态 Release 364 场景零差异，
    `0x007CC920` 语义 mutation 检出
  - common 子系统批次（`0x007E0BA0–0x00805850`，Grok）：34 个 clean 入口，其中 23 个是
    Player/Team/Money/Energy/各 Tracker 等子系统 vtable 的 `slot[2]` 名字槽（`mov eax, imm32; ret`，
    名字串逐条核对），11 个是同表上的字段/转发/谓词方法；8 个 explicitOnly hook；代码在
    `ra3/common/CommonSubsystem.h`。主控脚本核对 34 条槽位全部成立，3 条按连续 vtable 段起点
    引用的槽位改为 `vtables_split.tsv` 的精确表（`649c861`）。27 个候选（析构/释放、无独立
    行为的 thunk、x87 返回等）记为延后行。集成态 Release oracle 通过，`+0xBA` 谓词 mutation 检出
  - gameclient Drawable 批次（`0x00520170–0x0052DEF0`，Grok）：30 个 clean 入口（sway、tint
    envelope、模块遍历、记录解析与标志位刷新等），4 个 explicitOnly hook；代码在
    `ra3/gameclient/DrawableAccess.h`。锚点为带 `Drawable.cpp` 断言的 `0x0051FCB0`/`0x0052C1C0`、
    `Drawable::xfer` `0x0053EE30`（副 vtable `0x00BE1048` slot 3）和主 vtable `0x00BE1058`
    （构造器 `0x0053CAF0` 写入）。主控逐条核对 12 个调用点、15 条调用边和 6 个 vtable 槽位
    全部成立；更正 1 处调用者归属和"构造函数"措辞（`15f83cd`）。37 个候选因无锚点边、
    析构、thunk、受保护等记为延后行。集成态 Release 30 函数零差异
  - gamelogic 模块标识批次（`0x00727950–0x0075C880`，Grok）：51 个 clean 入口，均为
    Contain/Update/AIUpdate 模块 vtable 的 `slot[2]` `getModuleName`（26 个）与 `slot[4]`
    `getModuleNameKey` 静态 NAMEKEY 缓存（25 个，生成器 `0x00986150`），4 个 explicitOnly hook；
    代码在 `ra3/gamelogic/ModuleIdentity72.h`。主控核对 51 个槽位、名字串及每个指针在 `.rdata`
    中唯一出现。全为宏样板，语义深度低；之后的批次限制样板占比不超过 30%。集成态 Release
    152 场景零差异，guard 位 mutation 检出
  - gameclient FXParticleSystem 模块批次（`0x0065E1E0–0x0066CE40`，Gemini）：50 个计数的 clean
    入口（Alpha/Color/Update/Wind/RenderObject/Lightning 模块的初始化、模板复制、每帧更新、
    emission volume 外向速度与矩阵/样条辅助），15 个 explicitOnly hook；代码在
    `ra3/gameclient/ParticleModules65.h`。交付注释没有逐函数所有权证据，文档中的
    `0x00BF4BD4` 源路径和多个 vtable 地址不实且缺 hook 表；主控脚本反查后补写证据
    （16 个模块 vtable 槽位带 `slot[2]` 类名，34 个可追到模块/模板 vtable 或
    `FxpsGPUStorageModule.cpp` 断言的直接调用边），3 个无锚点入口改为延后行（`ea106de`）。
    集成态 Release 1,272 次比较零差异
  - render W3DScriptedModelDraw 批次（`0x008E52A0–0x00911250`，Gemini 半成品由 Grok 接手）：
    36 个 clean 入口（ScriptedModel/Dynamics draw 的阴影、子对象可见性与淡出、动画层、附着粒子、
    标志位与子接口访问），无 hook；代码在 `ra3/render/W3DScriptedModelDraw.h`。接手方修正了草稿里
    2 处与原程序不符的实现，剔除 34 个无所有权边的入口。主控用 `work/probe/verify_ownership.py`
    核对 50 条声明全部成立：vtable 槽位带 `slot[2]` 类名（W3DScriptedModelDraw/W3DDynamicsDraw/
    W3DLaserDraw/W3DTankDraw），副表 `0x00C555F0` 由 `0x009108F0` 写在 `+0x14`，调用边都在
    指令边界上，`0x0090CE30` 带 `W3DScriptedModelDraw.cpp` 断言。集成态 Release 108 场景零差异，
    `+0x2FC` 写偏移 mutation 检出
  - gamelogic 对象模块批次（`0x006A16A0–0x006BC4A0`，Gemini 半成品由 Grok 接手）：16 个 clean
    入口，主要是 Upgrade/Behavior/SpecialPower/DozerAction state 的 `slot[3]` xfer，另有
    WeaponSetUpdate slot 1、EncloseBehavior 包含判定、VeterancyCrateCollide 通过判定和
    SlaughterHordeContain 标志；无 hook；代码在 `ra3/gamelogic/ObjectCore6A.h`。Gemini 草稿里的
    Object/WeaponSet 方法无所有权锚点，被整体替换；94 个候选（NameKey 缓存、析构、`0x0076B240`
    无类名 vtable 等）记为延后行。主控核对 16 个槽位与类名、`0x00743F50` 在 `0x00743F66` 写入
    WeaponSetUpdate vtable。产出偏少。集成态 Release 16 函数零差异，`this+0x20` mutation 检出
  - 受保护 cave 批次（Gemini 三次断连后由 Grok 接手）：15 个入口，12 个 inner-patched、3 个
    entry-recoverable，均为 gamelogic 对象/AI 小函数；无 hook；代码在
    `ra3/gamelogic/ProtectedCaveLogic.h`。站点槽位在 run35 里仍指向 SecuROM VM；oracle
    （`verify_protcave.py`）只在模拟器内存里把槽位改指 `securom_sites.tsv` 记录的原生 cave
    （被替换指令 + 跳回 continuation），从入口执行到返回。Gemini 草稿把 cave 站点和运行时目标
    写反，接手方已更正。主控核对 15 行站点/槽位/continuation 与站点表一致、15 条调用边、11 个
    调用者 vtable 槽位和 4 个带源路径的调用者。10 个无锚点入口剔除，13 个记为延后。集成态
    81 场景零差异，`!= → ==` mutation 检出
  - Lua 4.0 VM 受保护批次（`0x00403660–0x0040E2F0`，Gemini）：44 个 entry-lost 入口（内存、
    栈、parser、GC、tag method、table、code generator），原生体 32 个在 `.text`、12 个在
    `.securom`；无 hook；代码在 `ra3/core/LuaVmPorts.h`，设计见 `lua_vm.md`。函数体的调用被改写成
    `push 返回地址; push 目标; ret` / `push 返回地址; jmp 目标` 并夹垃圾字节，部分目标与字符串指针
    用 `ror`/`xor`/`add` 现算。主控核对 44 行 site/slot/body 与站点表和 `slot_changes.tsv` 一致、
    `.text` 原生体不与台账函数重叠、14 条字符串文本、按伪调用解析的 22 条调用边，以及内联算出的
    目标。交付中 28 个 `.securom` 立即数槽被说成"在 run35 里解析"，实际在所有快照中为 0、
    `slot_changes.tsv` 也没有，是按 Lua 4.0 语义推断的；另有 2 个字符串地址错位、2 处内联推导
    操作数为编造、1 条调用边不实。主控已改正文档/oracle/注释（`6e59ee9`）。**待办**：oracle 的
    `SECUROM_IMMEDIATES` 需用一次在 Lua VM 首次运行之后（会话约 254 s 以后）的实机转储确认。
    集成态 127 场景零差异，`MASK_B` mutation 检出
  - gamelogic 脚本具名对象查询批次（`0x00838D80–0x0083FF90`，Grok）：51 个 clean 入口，都是
    script action/condition 的实现：用 `[0x00DFC058]` 上的脚本引擎经 `0x00837420` 查具名对象，再读写
    字段、置标志或尾调用；无 hook（查找会复制 AsciiString）；代码在 `ra3/gamelogic/ScriptNamedQueries.h`。
    锚点 `0x0083E520` 带 `ScriptConditions::evaluateNamedUnitRankLevel` 断言。交付只用"共享同一个
    被调函数"论证归属，不算合规形式；主控补出 51 条正式边：全部是 action 分派 `0x0084E110`
    （vtable `0x00C35F30` slot 14）或 condition 分派 `0x008477A0`（由 vtable `0x00C48898` slot 14 的
    `0x0084C460` 调用）的直接被调函数（`fb644d1`）。集成态 178 场景零差异，变异 5/5 检出
  - skirmish AI 批次（`0x00873E00–0x00887C90`，Grok）：41 个 clean 入口（34 个 AI tactic/manager/
    buildable 的 xfer、字段与谓词，7 个 client behavior 的 xfer 与记录访问）；无 hook；代码在
    `ra3/gamelogic/SkirmishAI87.h` 和 `ra3/gameclient/SkirmishClient87.h`，gamelogic 新增对
    `Ra3::sim` 的依赖。首个由子 agent 自己跑 `verify_ownership.py` 交付的批次（50 条声明全部成立），
    主控确认声明覆盖全部 41 行。集成态 151 场景零差异，`+0x210` 写偏移 mutation 检出
- **未完成或卡住**：
  - 对局状态 CRC：种子 CRC `0x005A9390` 的唯一调用者 `0x004D7490` 是 entry-lost，按
    Generals 推断即每帧 `GameLogic::getCRC`。按新规则可用运行时证据重新处理。
  - 类型化资源访问（recon/typed）未合并。
  - 少数 hook 在遭遇战里未被调用（logicrandomvariable、几个 BIG 枚举入口、
    stringfindlast 等），只有离线证据。
- **当前并行**（子 agent 随主控会话结束而中断，恢复时先查各 worktree 有无新提交）：
  - `recon/gamelogic7c`、`recon/common7e`、`recon/drawable52`、`recon/gamelogic72`、
    `recon/particles65`、`recon/render91`、`recon/obj6a`、`recon/protcave`、`recon/protentry2`、
    `recon/gamelogic82`、`recon/skirm87`：已合并（`789bcb1`、`f3c87e3`、`c33c919`、`2c9fd80`、
    `b67d3d3`、`9df37c8`、`760e231`、`79a7caa`、`9c14506`、`6234602`、`00c9a7a`）。追加合并脚本在
    冲突块以 `)` 收尾时会报 WARN，需手工检查括号（skirm87 合并时拼坏过一次 CMake）。受保护批次的站点/字符串/伪调用核对脚本见 `work/probe/check_protcave.py`、
    `work/probe/check_protentry2.py`，多快照槽值用 `work/probe/slot_snapshots.py`。合并冲突若只是注册行追加，用
    `python work\probe\union_conflicts.py <文件...>`。所有权声明用
    `python work\probe\verify_ownership.py <claims.txt>` 逐条反查（格式见脚本头注释）。
  - `recon/common5a`：`0x005A0000–0x005BFFFF` common 簇，Grok，base `0d099eb`。
  - `recon/render56`：`0x00560000–0x0059FFFF` render 簇，Gemini，base `5e4a7c2`。
  - `recon/audio50`：`0x00500000–0x0050FFFF` SageAudio 设备层，base `d18d88e`；Gemini 断连后主控
    提交快照 `30643c3`（约 42 个草稿入口和 checker，无 oracle/注释/文档），改由 Grok 接手。
  - `recon/objbehav68`：Gemini 两次中断，主控提交快照 `97a0572`（约 70 个草稿入口和 checker，
    oracle 原型在该 worktree 的 `work/`），改由 Grok 接手。
  - `recon/render99`、`recon/thing78`、`recon/net5d2`、`recon/skirm8b`：Grok 新开，见补位队列。
  - `recon/skirmishai89`：`0x00890000–0x0089FFFF` skirmish AI 簇，Gemini，base `5d16a5b`。
  - 14:00 补足 10 路，base 均为 `86baa8d`：`recon/objbehav68`（`0x00680000–0x0068FFFF`
    对象 behavior/update 模块，Gemini）、`recon/protentry`（已合并；由
    `recon/protentry2` 接续剩余 entry-lost 原生体）、`recon/protcave`（全部站点为 cave 的
    inner-patched/entry-recoverable 函数，Gemini，无 hook）、`recon/drawable52`
    （`0x00520000–0x0052FFFF` Drawable 访问器，Grok，逐行所有权证明）、
    `recon/particles65`（`0x00650000–0x0067FFFF` 粒子模块，Gemini）、`recon/net5d`
    （已合并）。
  - 模型服务连接错误会中断子 agent。resume 续跑会带回长上下文，实测很快再次失败；
    做法是先查 worktree 未提交改动，再派一个新上下文的 agent 接手半成品（说明现有文件、
    缺什么、先提交再完善）。gamelogic7c、objbehav68、protcave 已按此接手。
  - 已合并批次的旧 worktree 已移除（分支保留）；`recon/behavior6b` 的 `c24fe82` 与
    main 的 `44616d1` 为同一补丁。
- **合并前静态验收**：`python Ra3Source\tools\analysis\accept_batch.py recon/<name>`。
  它只经 git 读取分支内容，检查共享文件改动、与 main 内容不同的同名新增文件、新 hook
  的键唯一性/explicitOnly 计数/expected 字节对二进制/stolen 整指令且可搬移/无回跳/
  每键有 record 调用、设计文档 hook 表的字节/VA/explicitOnly 与代码一致，以及注释的
  格式、owner、台账归属、保护类别和与 main/在途分支的重复认领。`RESULT FAIL` 先退回或
  由主控修正再合并。脚本不判断语义与所有权证据，这部分仍由主控抽查。
- **批次指标**：`docs/analysis/ra3_1.0/batch_metrics.tsv`，合并或退回时追加一行。
- **补位队列**（任一路完成即补，保持 10 路；数字为未认领 clean 函数数）：
  ~~`0x006A0000–0x006BFFFF`~~（已开 obj6a）→ ~~`0x007E0000–0x0080FFFF`~~（已开
  common7e）→ ~~`0x00720000–0x0075FFFF`~~（已开 gamelogic72，Grok）→ ~~`0x00560000`~~（已开
  render56，Gemini）→ ~~`0x005A0000`~~（已开 common5a，Grok）→ ~~`0x00500000`~~（已开 audio50，Gemini）→
  ~~`0x00870000`~~（已开 skirm87，Grok，base `179b87b`，派单模板 `work/prompts/skirm87.md`）→
  ~~`0x00990000–0x009AFFFF`~~（已开 render99，SageRender，Grok，base `38a0f6c`）→
  ~~`0x004D0000`~~（已开 logic4d，GameLogic 核心 clean 部分，Gemini，base `a721bfd`）→ ~~`0x00780000`~~（已开 thing78，Grok，base `4ead699`）→
  ~~`0x005D0000`~~（已开 net5d2，ReliableTransport 等，Grok）→ ~~`0x008B0000–0x008CFFFF`~~（已开
  skirm8b，skirmish AI，Grok）→ `0x00690000` 对象模块 497（锚点 10）→ `0x00790000` AIStates 591（锚点 10）→
  `0x00740000–0x0077FFFF` 对象 update/weapon 783（锚点 31）→ protcave 的 13 个延后入口与剩余
  cave 站点（Grok 已证明能做）→ ~~`0x00820000–0x0083FFFF`~~（已开 gamelogic82，Grok）。
  受保护批次合并后，按剩余 body/cave 数量再开下一路。
- **验收教训**：子 agent 的交付说明可能与提交不符（protentry 的证据地址、gamelogic7c 的
  vtable 地址与源路径均为编造的）。vtable 所有权声明一律用脚本反查：在 `.rdata` 中找函数
  指针 → 经 `vtables_split.tsv` 定位所在表与槽号 → 读该表 `slot[2]` 的类名字符串（本二进制
  无 MSVC RTTI）。
  受保护批次除 `accept_batch.py` 外，主控还要用脚本把注释中的 site/slot/body 与
  `securom_sites.tsv` 对齐，并抽查所引字符串、表引用确实存在。合并前用脚本从目标表直接核对
  键名、explicitOnly 计数与二进制 expected16，并核对 record 键与注释一致；旧 base 的
  分支可能新建与主分支同名的头文件（WeaponLogic 合并时即需改名为 `UpdateModules.h`）；
  批次自带的 `--mutated` 若只是事后翻转输出，主控需另做一次实现层语义 mutation。每支独立 worktree，完成后主控逐支
  复验合并；构建/oracle 可并行，主分支合并仍串行，避免注册表和 CMake 冲突污染。
- **后续模型分流**（用户 12:10 决定）：复杂 ABI、受保护边界与重型对象簇继续优先
  Gemini 4 Argon Medium；简单叶函数、访问器与清晰类簇在 Gemini 4 Argon Medium 和
  Grok 4.7 High Fast 间 1:1 分配。当前已有两个 Gemini 补位，因此接下来两个简单补位
  先用 Grok，再交替。主控统一记录首轮构建/oracle 通过率、误计数或猜测、返工次数、
  完成耗时和最终有效重建数；没有账单遥测时不推断精确 token 成本。Grok 的前两批只分配
  clean 低风险簇，并提高证据门槛：逐项核对 `reversed` ↔ C++ ↔ shipped 行为，主控额外
  抽查反汇编、ABI、hook 前缀与写集；归属模糊、身份指纹冒充语义或重复认领立即退回。
- **模型分流调整**（用户 15:25 决定）：改为约 7 路 Grok、3 路 Gemini，Gemini 只做最难的
  （受保护代码、GameLogic 核心、重 x87/ABI 簇）；在跑的 Gemini 批次不中断，空出的位优先补
  Grok 直到 7:3。依据见 `batch_metrics.tsv`：截至 15:20，Grok 5 批 165 个函数、交付与代码
  一致、0 次断连，但倾向挑样板（派单限定样板 ≤30%）；Gemini 3 批 141 个函数、代码过
  oracle，但 3 批证据均有编造、约 10 次断连，交付一律由主控脚本重推证据。
  两个 Grok 低风险批次均经主控反汇编抽查后无需实质返工；后续简单叶簇可按普通强度
  使用 Grok，复杂 ABI、构造器和状态机仍优先 Gemini。
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
- **git**：主控不主动 push；提交差距以当前 `git status` 为准。

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
3. **大批量并行移植**：🚧 持续进行。保持 10 路，复杂批次用
   `gemini-4-argon-medium`，简单 clean 批次在 Gemini 与 `grok-4.7-high-fast` 间
   1:1 分流。每路负责一个成块子系统，每批
   50–200 个函数，每批带 oracle 脚本。优先成块的 gamelogic/gameclient 簇，例如
   `0x0065xxxx–0x0066xxxx` 粒子/特效（大量调用 client RandomVariable），用 Generals 源码
   命名，不要挑零散的匿名字段访问器。当前九批共新增 341 个台账 reversed；按风险分级
   验收，并在集成态集中跑构建、CTest 与新批 oracle。
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
