# 0x008B–0x008C 遭遇战 AI

## 范围

本批保留 `0x008B0000–0x008CFFFF` 里 42 个 clean、原先 `unanalyzed` 的函数，全部在 `ra3::gamelogic`（`SkirmishAI8B`）。40 个是 `vtables_split.tsv` 里某张表的槽。`008C9950` 和 `008C9AA0` 没有虚表，分别是 `008CD760` 在 `008CD791`、`008CD9D0` 在 `008CD9E6` 的 `E9` 尾跳。类名来自该表 slot 2：`mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。`verify_ownership.py` 对 `work/skirm8b_claims.txt` 的结果是 `fails=0`。

范围外的 `0x00890000–0x0089FFFF` 不在本批。邻批是已合并的 skirm87（`0x00870000–0x0088FFFF`）。

## 函数

| VA | 归属 | C++ |
| --- | --- | --- |
| `008B7860` | AIRecruitmentManagerDataCache `00C51EB4` slot 3 | `xferAIRecruitmentManagerDataCache` |
| `008B85F0` | AIBuildableUnit `00C52CD8` slot 16 | `buildableUnitPhaseIsNotOne` |
| `008B8600` | `00C52CD8` slot 7 | `buildableUnitPhaseIsOne` |
| `008B8610` | `00C52CD8` slot 19 | `copyBuildableUnitAnchor` |
| `008B8FE0` | AIBuildableStructure `00C527C8` slot 9 | `pushAIBuildableStructureRecord` |
| `008BDC40` | AiBuildEfficiencyCalculator `00C51A7C` slot 3 | `xferAiBuildEfficiencyCalculator` |
| `008BFF40` | AIOpeningMover `00C51AFC` slot 3 | `xferAIOpeningMover` |
| `008C0100` | SkirmishAISpecialPowerStore `00C52510` slot 3 | `xferSkirmishAISpecialPowerStore` |
| `008C1340` | AISimpleDefenseTactic `00C53334` slot 10 | `markAISimpleDefenseContact` |
| `008C1370` | AIEngineerTactic `00C53414` slot 10 | `cacheAIEngineerTacticRecord` |
| `008C3040` | AINeedsBuilder `00C51EC4` slot 3 | `xferAINeedsBuilder` |
| `008C48E0` | AIUnitUpgrader `00C51C44` slot 3 | `xferAIUnitUpgrader` |
| `008C4B10` | AIUpgradeScience `00C52FD0` slot 3 | `xferAIUpgradeScience` |
| `008C4D40` | `00C52CD8` slot 1 | `bindAIBuildableUnitTarget` |
| `008C5140` | `00C527C8` slot 1 | `bindAIBuildableStructureTarget` |
| `008C58F0` | AIRebuildableStructure `00C53048` slot 3 | `xferAIRebuildableStructure` |
| `008C8DA0` | `00C53334` slot 11 | `tryAISimpleDefenseActivation` |
| `008C8DE0` | AIStaticDefenseTactic `00C5336C` slot 10 | `cacheAIStaticDefensePosition` |
| `008C96F0` | AIFlankAttackTactic `00C53460` slot 10 | `updateAIFlankAttackTactic` |
| `008C9950` | `008CD760` 在 `008CD791` 尾跳 | `continueAISimpleAttack` |
| `008C9A70` | AIScriptedTactic `00C535DC` slot 10 | `gateAIScriptedTactic` |
| `008C9AA0` | `008CD9D0` 在 `008CD9E6` 尾跳 | `chooseAIScriptedTacticMode` |
| `008CCA40` | AIReactiveDefenseTactic `00C532FC` slot 5 | `aiReactiveDefenseWindowOpen` |
| `008CCAA0` | `00C532FC` slot 10 | `updateAIReactiveDefenseTactic` |
| `008CCB40` | `00C532FC` slot 7 | `clearAIReactiveDefenseAlert` |
| `008CCB90` | `00C532FC` slot 4 | `createAIReactiveDefenseTactic` |
| `008CCC10` | `00C53334` slot 4 | `createAISimpleDefenseTactic` |
| `008CCEB0` | `00C5336C` slot 4 | `createAIStaticDefenseTactic` |
| `008CCF60` | AIExactExpansionTactic `00C533A4` slot 4 | `createAIExactExpansionTactic` |
| `008CD020` | AISimpleExpansionTactic `00C533DC` slot 4 | `createAISimpleExpansionTactic` |
| `008CD110` | `00C53414` slot 4 | `createAIEngineerTactic` |
| `008CD3F0` | `00C53460` slot 4 | `createAIFlankAttackTactic` |
| `008CD4B0` | AIGarrisonTactic `00C5349C` slot 4 | `createAIGarrisonTactic` |
| `008CD6E0` | AISimpleAttackTactic `00C534E8` slot 4 | `createAISimpleAttackTactic` |
| `008CD760` | AISimpleSiegeTactic `00C53524` slot 11 | `updateAISimpleSiegeTactic` |
| `008CD7B0` | `00C53524` slot 4 | `createAISimpleSiegeTactic` |
| `008CD840` | AIHuntTactic `00C53560` slot 5 | `aiHuntWindowOpen` |
| `008CD880` | `00C53560` slot 7 | `clearAIHuntAlert` |
| `008CD8D0` | `00C53560` slot 4 | `createAIHuntTactic` |
| `008CD950` | AINullTactic `00C535A4` slot 4 | `createAINullTactic` |
| `008CD9D0` | `00C535DC` slot 11 | `updateAIScriptedTactic` |
| `008CD9F0` | `00C535DC` slot 4 | `createAIScriptedTactic` |

样板（slot 2 类名 getter、NAMEKEY 缓存、`mov eax, imm; ret` 常量、只转发给基类的 xfer）为 0/42。13 个 slot 4 工厂形状相近，但分配尺寸和构造目标各不相同，并且都做空指针检查和字段传递，不计入上述四类。

## 行为

`008B85F0` 比较 `[this+0x18]` 是否不等于 1，`008B8600` 比较是否等于 1。`008B8610` 把 `+0x40/+0x44/+0x48` 三个 float 拷到栈参数并 `ret 4`，EAX 是目的指针。

`008B8FE0` 在第二个栈参数的低字节为 0 时清 `[this+0x14]`。玩家取自参数对象 `+0x90`。`[this+0x14]==0` 时向量在玩家 `+0x618`，否则 `+0x640`。`end < cap` 为无符号比较；有空位时把 end 加 4，end 非 0 才写入 `[this+0x2C]`。否则调用 `008ADD50`。

slot 4 工厂用 `00416820` 分配，再把 ai（`+0x50`）、玩家索引（`+0x54`）和 `this+0x38` 交给各自的构造函数。尺寸：Reactive `0xBC`/`008CC9B0`，SimpleDefense `0xA0`/`008CCBC0`，Static `0xB0`/`008CCC40`，Exact `0xBC`/`008CCEE0`，SimpleExpansion `0xB4`/`008CCF90`，Engineer `0xA8`/`008CD050`，Garrison `0xA8`/`008CD440`，SimpleAttack `0xB0`/`008CD4E0`，SimpleSiege `0xA0`/`008CD710`，Hunt `0xA4`/`008CD7E0`，Null `0xA0`/`008CD900`。分配失败返回 0。

`008CD3F0` 分配 `0xA4`。`sete cl` 只改低字节，第四个构造参数是战术表指针的高 24 位加上“表项是否等于 2”。表在 `[[ai+0x10]+0x3C]`，下标是 `player * 17`，偏移 `+4`。构造函数是 `008CD140`。

`008CD9F0` 分配 `0xA0`，用 `004CBFF0` 对栈上的临时量传入 `00BD15C0`，再以五个参数调用 `008B4900`，然后 `004CA930`，最后把对象首 dword 写成虚表 `00C535DC`。

战术更新从 `[this+0x2C]` 取 owner，再从 owner `+0x90` 取玩家。`008C1340` 在 `[this+0x28]` 非空时经 `0089A740` 和 `008B6260` 记下位置，并把 owner `+0x10A` 写成 1。`008C1370` 把 `00885BB0(this+0x58, 1)` 的结果存到 `+0xA0`，再调用 `008B5370`。`008C9A70` 在 `008818E0(this, 0)` 或 `[this+0x28]` 为 0 时调用 `008C16A0(this, 0, 0)`。

`008C8DA0` 要求 `[this+0x28]` 和字节 `+0x14` 都非 0。`008A8B20` 的低字节为 0 时再看目标对象的低字节（`0089A640`）；两者都为 0 则返回，否则 `008C16A0(this, 1, 0)`。`008C8DE0` 在 live 或 `[this+0x28]` 为 0 时走 `008C16A0(this, 0, 0)`，否则从目标拷 12 字节到 `+0xA0` 并调用 `008B6260`。

`008C96F0` 同样在 live 或目标为空时失败。否则 `007E5660` 写三个 float，从目标再拷三个 float，用玩家索引和表项 `+4` 拼出 `sete al` 保留高 24 位的标志，调用 `008B4560`，再 `008B5370`。

`008CCAA0` 先算表项 `+0x30`，调用 `008908C0(player+0x454, slot, 0)`，并且无论后面是否失败都从 `[[player+0x7DC]+0]+0x60` 拷一个 float 到 `+0xB4`。live 或 found 为 0 时 `008C16A0(this, 0, 0)`。成功时写 `[found+4]` 到 `+0xA4`，`00890400` 的结果交给 `008B6260`，再经 `00625090(player+0x7FC, 00DFC6D4)` 把该单元写成 1。

`008CCB40` 对同一地图单元先后写入 0 和 `frame+1`（键 `00DFC6D4`、`00DFC67C`），并把 owner `+0x10A` 清 0。`008CD880` 在字节 `+0xA0` 非 0 时直接返回；否则用键 `00DFC6D8`、`00DFC680` 做同样的两步写入。帧来自 `[00DEBA64]+0x58`。

`008CCA40` 和 `008CD840` 都是 `ret 4`，栈参数不用。`cmp / jg` 是有符号比较。失败路径 `xor al, al`，成功路径 `mov al, 1`，高 24 位保留。Reactive 成功时还会把地图单元写成 1，返回值是该指针掩掉低字节再或 1。Hunt 成功不写内存。键分别是 `00DFC6D4`/`00DFC67C` 和 `00DFC6D8`/`00DFC680`，都从玩家对象读出。

`008CD760` 在字节 `+0x14` 为 0 时返回。`008818E0(this, 1)` 或 `008818E0(this, 0)` 为 0 时调用 `008C16A0(this, 0, 0)` 后返回。`008A8A90(this, 0)` 的低字节为 0 时直接返回，不调用 `008C16A0`。否则尾跳 `008C9950`。

`008C9950` 在 `[this+0x28]` 非空时 `0089A6D0` 再 `008C1950(this, 0, pos)`。然后 `008818E0(this, 0)`、`007E6850`、`0088C0E0(this, 1, resolved)`。

`008C9AA0` 读表项 `+8`。mode 1 调用 `008C16A0(this, 1, 0)` 并返回 0。mode 0 看 `008B5050` 的低字节，非 0 则返回该字节，否则同样调用并返回 0。mode 2 看 `008A88C0` 的低字节，为 0 时返回该值的高 24 位，否则调用并返回 0。其他 mode 原样返回。

`008CD9D0` 在 `[this+0x28]` 非空且 `0089A640` 的低字节为 0 时返回该字节。否则尾跳 `008C9AA0`。

`008C4D40` 把 `[this+0x3C]` 写成 1。玩家是 `[[this+4]+0x90]`，为 0 则返回。`008878C0(player+0x394, [this+0xC], [this+0x18])` 的结果写入 `+0x20`，非 0 时再 `008BC5B0(found+0xC, &this)`。`008C5140` 先用全局 `00DF9D28` 上的目录和 `[this+0x10]` 调用两次 `0078D880`。目录非空时从 `[found+0x1A8]` 走到 `[found+0x1AC]`，节点步长 `0x14`，`[[node+4]]==0xA112A404` 时把该指针写入 `[this+0x68]`，否则写 0。账本长度停在 `pop esi`，真正的收尾是后面的 `pop ecx; ret`，命中项经 `008C51DD` 的 `mov eax, edx; jmp 008C5194` 回到共用的存储。然后走同一个绑定尾部。

xfer 都是 `thiscall`，栈参数是 xfer，`ret 4`。版本槽 `+0x18`，字段槽 `+0x20/+0x6C/+0x70/+0x84`，具名槽 `+0x88`。`008C0100` 的版本对是 1,2，次版本小于 2 时直接返回，否则 `008BC0C0(xfer, this+0x10)` 和 `008BC310(xfer, this+0x2C)`。`008BDC40` 版本 1,1，然后 `008BBCE0`、`008A5020` 和 `+0x6C`。`008C3040` 对 `+0x2C` 和 `+0x38` 调用 `0082D820`，对 `+8` 走 `+0x20`，再对 `+0x1C` 调用 `0073A470`。

`008B7860` 在 xfer 标志 bit 1 置位时用 `007A7BA0` 和 `0087DF00` 清两段并写 0 到 `+0x10` 和 `+0x30`，然后 `008A51A0`、`008AD720` 和 `+0x6C` 作用于 `+0x44`。`008C4B10` 调用 `00887BE0`，`+0x84` 作用于 `+0x4C`，`+0x70` 作用于 `+0x54` 和 `+0x58` 的栈副本再写回；标志 bit 1 置位时把第一份结果交给 `008C4990`。`008C48E0` 对 `+0x30` 走 `+0x20`，对 `+0x40` 走 `+0x70`，然后两次 `0073A470`（`+8` 和 `+0x1C`），中间各有一次版本调用。

`008BFF40` 先做版本 1,2，再写 `+4`、`+0x18`、`+0x1C`。第二次版本调用的栈槽初值是 1,1，但 `cmp byte [esp+9], 2 / jb` 比较的是第一对的次版本。小于 2 时 `+0x6C` 作用于 `+0x38`，否则 `0087F630(this-4, xfer)`。`0073A470` 作用于 `+0x24`。`008C58F0` 调用 `008C5060`，用 `+0x84` 处理“`+0x70` 是否非空”的字节；低字节为 0 则返回。标识是 `0xFFFFFFFF`，或 `[[this+0x70]+4]`。`+0x88` 的名字立即数是 `00C51504`，count 是 4。标志 bit 1 置位时 `008884E0([this+0x6C], ident)` 的结果写回 `+0x70`。

## 未计入

- slot 2 类名 getter：`008B0910`（AIBuildStateManager）、`008B85E0`（AIBuildableUnit）、`008B8E00`（AIUpgrade）、`008B9040`（AIBuildableStructureFactory）、`008BCAC0`（AIBank）、`008C4980`（AIUpgradeScience）、`008C5890`（AIRebuildableStructure）、`008C6D20`（AIBuildManager）、`008C70D0`（AIEconomyManager）。都是 `mov eax, imm32; ret`。
- 返回常量：`008B8DF0` 是 `mov eax, 4; ret`（AIUpgrade slot 5），`008C4970` 是 `mov eax, 5; ret`，`008C5880` 是 `mov eax, 7; ret`。
- slot 0 析构/释放，写虚表并调用 `004166B0`：`008BA1A0`、`008BDF70`、`008C00E0`、`008C2AD0`、`008C4C80`、`008C51F0`、`008C58A0`、`008C62B0`、`008CAA00`、`008CBF70`。`008CA4B0` 也在释放路径上调用 `004166B0`。
- `008C70E0` 是 `sub ecx, 4; jmp 008CC2F0`，没有自身行为。
- `008C9900`（AISimpleAttackTactic slot 10）在 `008C993E` 跳进 `008C1390`。后者不在本批，不能把这条尾跳算作独立行为。
- `008C8E30`（AIExactExpansionTactic slot 10）读 `00DEBA64` 和 `00DEBB00`，并调用 `00A21BE0`、`0050B5F0`、`00A21C90`。字符串表和全局对象没有和本批其余函数相同的可替换桩。
- 调用过多或带 SSE 的求值器：`008C8920`（1149 字节，AIReactiveDefenseTactic slot 11）、`008C6590`、`008CDE40`、`008B62F0`、`008B66A0`、`008C3CF7`（650 字节，slot 2 不是类名 getter）。`008CCCB0`、`008C9190`、`008C9430`、`008CD260`、`008C97A0`、`008C8EE0`、`008CD550` 是更长的战术 slot 11。
- `0x008B0000–0x008CFFFF` 以外的地址，以及非 clean / 已分析的入口。

## Hook

无。

## Oracle

`tools/skirm8bcheck` 与 `tools/analysis/ports/verify_skirm8b.py` 对 42 个入口共 141 组输入比较 blob 和 EAX。范围外的直接调用在两边用同一份桩：分配、构造、谓词和 xfer 虚槽的写入都记进 blob 内的调用日志。

```
skirm8b oracle: 42 entries, 141 cases, 0 mismatches
```

把 `buildableUnitPhaseIsNotOne` 的 `phase != 1` 改成 `phase == 1` 后，第一条 phase 0 用例失败：

```
0x008B85F0: return mismatch (u32): game=0x00000001 cpp=0x00000000
```

改回 `!=` 并重新编译后，再次是 `0 mismatches`。
