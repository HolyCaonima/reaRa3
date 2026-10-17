# 0x0087–0x0088 遭遇战 AI 与客户端行为

## 范围

本批保留 `0x00870000–0x0088FFFF` 里 41 个 clean、原先 `unanalyzed` 的函数。34 个在 `ra3::gamelogic`（`SkirmishAI87`），7 个在 `ra3::gameclient`（`SkirmishClient87`）。每个函数要么是 `vtables_split.tsv` 里某张表的槽，要么是这种槽的直接调用。类名来自该表 slot 2：`mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。

`00C52768` 的 slot 3 是 `008822E0`，但 slot 2 是 `00411440`（`mov ecx, [0x00DE9B1C]`），不是类名 getter。这个函数只按地址重建，不另起类名。

同一份函数体会装进多张表。计数按函数入口一次：

- `0087D3E0` 是 `AIReactiveDefenseTactic` `00C532FC` slot 8，也在 `AIExactExpansionTactic` `00C533A4` slot 8 和 `AISimpleExpansionTactic` `00C533DC` slot 8，另外还有六张战术表。
- `00881820` 同时是 `AIStaticDefenseTactic`、`AIFlankAttackTactic` `00C53460`、`AISimpleAttackTactic` `00C534E8` 的 slot 5。
- `0087D860` 同时是 `AIBuildableStructure` 和 `AIRebuildableStructure` 的 slot 8。
- `0087D870` 同时是 `AIBuildableUnit`、`AIUpgrade`、`AIUpgradeScience` 的 slot 17。
- `00873EB0` 同时是 `ModelConditionAudioLoopClientBehavior` 和 `ClientUpdateModule` `00BF7B18` 的 slot 3。
- `00874160` 同时是多种客户端行为的记录指针槽。

`00885BB0` 不是虚表槽。`AISimpleExpansionTactic` `00C533DC` slot 10（`00886640`）在 `00886648` 调用它。

## 函数

| VA | 归属 | C++ |
| --- | --- | --- |
| `00873E00` | AnimationSoundClientBehavior `00C4FE40` slot 3 | `xferAnimationSoundClientBehavior` |
| `00873EB0` | ModelConditionAudioLoopClientBehavior `00C500B8` slot 3 | `xferModelConditionClientBehavior` |
| `00874160` | `00C4FE40` slot 13 | `clientBehaviorRecordOrNull` |
| `00874170` | RandomSoundSelectorClientBehavior `00C50318` slot 3 | `xferRandomSoundSelector` |
| `00874250` | BeaconClientUpdate `00C4FF48` slot 3 | `xferBeaconClientUpdate` |
| `00874470` | LargeGroupAudioClientUpdate `00C503E0` slot 3 | `xferLargeGroupAudio` |
| `00874870` | `00C500B8` slot 10 和 slot 11 | `audioLoopRecordOrNull` |
| `0087C400` | AIBank `00C52E20` slot 3 | `xferAIBank` |
| `0087C4D0` | AIBuildHeuristicStore `00C52338` slot 3 | `xferBuildHeuristicStore` |
| `0087C730` | AIEconomyManager `00C53114` slot 3 | `xferEconomyManager` |
| `0087C830` | AIRedAlertManager `00C51768` slot 3 | `xferRedAlertManager` |
| `0087CB80` | AISpecialPowerUpdatableAttackObject `00C517F0` slot 5 | `attackObjectPresent` |
| `0087D2A0` | AISimpleDefenseTactic `00C53334` slot 7 | `clearSimpleDefenseAlertByte` |
| `0087D2B0` | `00C53334` slot 8 | `applySimpleDefenseTacticMode` |
| `0087D3E0` | AIReactiveDefenseTactic `00C532FC` slot 8 | `rollAITacticActivation` |
| `0087D6E0` | AIStrategyManager `00C52724` slot 3 | `xferStrategyManager` |
| `0087D760` | AIPrerequisiteBuilder `00C51BC4` slot 3 | `xferPrerequisiteBuilder` |
| `0087D7D0` | AIUnitBuilder `00C51C08` slot 3 | `xferUnitBuilder` |
| `0087D860` | AIBuildableStructure `00C527C8` slot 8 | `copyBuildableStructureScore` |
| `0087D870` | AIBuildableUnit `00C52CD8` slot 17 | `ensureBuildablePhaseFour` |
| `0087D890` | AIBuildableStructureFactory `00C52DBC` slot 3 | `xferBuildableStructureFactory` |
| `0087D940` | AIEmergencyManager `00C5193C` slot 3 | `xferEmergencyManager` |
| `0087DC90` | SkirmishAISubSystemEnabler `00C515BC` slot 3 | `xferSubsystemEnabler` |
| `0087EDD0` | `00C527C8` slot 19 | `copyBuildableStructureAnchor` |
| `0087F450` | `00C51768` slot 8 | `aiRedAlertFrameReached` |
| `0087FD80` | `00C517F0` slot 3 | `xferSpecialPowerAttackObject` |
| `00881820` | `00C5336C` slot 5 | `tacticArgumentPresent` |
| `008822E0` | `00C52768` slot 3 | `xferSharedBuildRecord` |
| `00885700` | `00C51768` slot 6 | `armAIRedAlertFrame` |
| `00885BB0` | `00886640` 在 `00886648` 调用 | `findSimpleExpansionRecord` |
| `008865E0` | AIStaticDefenseTactic `00C5336C` slot 8 | `applyStaticDefenseTacticRecord` |
| `00886640` | AISimpleExpansionTactic `00C533DC` slot 10 | `cacheSimpleExpansionRecord` |
| `00886660` | AISimpleSiegeTactic `00C53524` slot 8 | `applySimpleSiegeTacticFlag` |
| `00886680` | AIHuntTactic `00C53560` slot 8 | `applyHuntTacticFlags` |
| `00887870` | `00C52CD8` slot 3 | `xferBuildableUnit` |
| `00887BE0` | AIUpgrade `00C52D48` slot 3 | `xferAIUpgrade` |
| `00887C90` | AIUpgradeOpeningMove `00C53280` slot 3 | `xferUpgradeOpeningMove` |
| `00882E20` | AIRebuildableStructure `00C53048` slot 19 | `copyRebuildableStructurePosition` |
| `00882E60` | `00C53048` slot 24 | `rebuildableStructurePositionPointer` |
| `00882E70` | `00C53048` slot 27 | `rebuildableStructureRadiusOrZero` |
| `00882E90` | `00C53048` slot 28 | `rebuildableStructureRadiusPointer` |

样板（slot 2 类名 getter、`call 00986150` 的 NAMEKEY 缓存、`mov eax, imm; ret` 常量）为 0/41。若把单字段写入和空指针判断也算薄函数，有 8 个（`0087D2B0`、`00886660`、`00881820`、`0087CB80`、`0087D860`、`00874160`、`00874870`、`0087D2A0`），约 20%，仍低于 30%。

## 行为

战术激活函数的栈参数才是记录，`this` 不用。`008865E0` 把 `+0x210` 写成 1，再把 9 个 dword 从 `+0x21C` 起写成 `0x400` 和 8 个 0，EAX 留下 `0x400`。`00886680` 写 `+0x214 = 1`、`+0x160 = 0`、`+0x294 |= 0x10`、字节 `+0x2B2 = 1`。`0087D2B0` 写 `+0x210 = 5`。`00886660` 对 `+0x21C` 做 `|= 0x02000000`。`0087D2A0` 清 `[this+0x2C]+0x10A`。`00881820` 用 `setne al` 判断栈参数是否非 0。

`0087D3E0` 推 `AITactic.cpp`（`00C511D0`）后调用 `005A94C0` `logicRandomInteger(3, 5)`，把结果存到记录 `+0x214`。逻辑随机状态在 `00CB7420`。

`00886640` 用键 2 调用 `00885BB0`。树头在 `this+0x58`，哨兵是头 `+0x14`，起点在头 `+0x18`。命中时返回节点 `+0x10`，并写到 `this+0xA0`。后继是 `009C9980`（EASTL 红黑树，不计数）：右孩子非空则取最左，否则沿父节点走到不再是父的右孩子。

`0087D870` 在 `[this+0x14] == 4` 时直接 `ret 4`（EAX 保持进入时的 0）。否则把栈参数改成 4，尾跳到虚表 `+0x28`。

`0087F450` 比较 `TheGameLogic`（`[00DEBA64]+0x58`）和 `[this+0x14]`，无符号 `frame < due` 得 0，否则 1。`00885700` 把帧加上 `[this+0x10]` 写回 `[this+0x14]`，EAX 是逻辑对象指针。

`00882E20` 在记录或位置指针为空时用 `xorps` 清三个 float，否则 `fld/fstp` 拷贝。`0087EDD0` 从 `[this+0x44]` 拷贝三个 float。`00882E70` 在半径指针非空时 `fld` 该 float，否则 `fldz`。

xfer 都是 `thiscall`，栈参数是 xfer 对象，`ret 4`。版本槽是虚表 `+0x18`，字段槽是 `+0x20/+0x58/+0x64/+0x6C/+0x70/+0x84`，具名槽是 `+0x88`（count、field、name）。`0054A030/070/090` 把 ParticleSystemID、ProductionQueueType、ObjectID 转成 `+0x88`。`008D82F0` 在本范围外，客户端快照把它内联成两次丢弃结果的版本调用。版本号低于文件支持版本时跳过新增字段：紧急管理器和启发式仓库是 2，攻击对象是 3，动画声音是 2。`0087C400` 在 xfer 标志 bit 1 置位时把 `+0xFC` 清 0。`00874470` 和 `00874250` 在 bit 3 为 0 时才写尾部字段。`008822E0` 对 `+0x29/+0x2A/+0x2B` 做重叠的 dword 异或，并把 `+0x14` 经栈临时量再写回。

## 未计入

- slot 2 类名 getter，以及调用 `00986150` 的 NAMEKEY 缓存（例如 `00873DB0`、`00874840`、`00877060`、`008772D0`）。这是样板。
- 析构和释放：`0088A0A0`、`0088A330`、`0088A500` 写虚表并调用 `004166B0`。`00887D30` 沿链表调用虚表 `+4` 和 `009C9980`。
- `0087CBE0` 是 `jmp [vt+0x38]`。`0087FFF0` 的账本长度 15，函数体在 `ret` 之前就结束。
- `00882190` 把三个 float 清零，并且同时属于粒子模块。
- `00882400` 是可建造结构 slot 26，但调用 `00DFC190` 虚表 `+0x18`，返回值在 ST0。桩只能写 EAX。
- `00873F80` 推 `RandomSoundSelectorClientBehavior.cpp`，读 `00DEEA50` 虚表 `+0x74` 并调用客户端浮点随机 `005A95A0`。调用方 `00874120` 还要连带计入。
- `00874400` 没有虚表命中。它用 ESI 传入对象，调用客户端整数随机 `005A94F0`，并用 `fild/fistp` 做舍入。
- `00881510` 长 417 字节，读多个全局并调用 `00B7AF40`。
- `0087CAE0` 是多张特殊能力表的 slot 2，但函数体推的是 `AISpecialPowerHeuristic.cpp` 并调用虚表 `+4` 和 `005A9550`，不是类名 getter。不给这些表编类名。
- `00882D00`、`0088E7D0` 分别推 `AIPersonalityManager.cpp` 和 `AiPowerPlayerShroudClear.cpp`，但自身不是虚表槽，也没有一条可证明的调用边。

## Hook

无。

## Oracle

`tools/skirm87check` 与 `tools/analysis/ports/verify_skirm87.py` 对 41 个入口共 151 组输入比较 blob、EAX 和 `00882E70` 的 ST0。

恢复正确偏移后：

```
skirm87 oracle: 41 entries, 151 cases, 0 mismatches
```

把 `applyStaticDefenseTacticRecord` 的 `+0x210` 改成 `+0x20C` 后，oracle 在第一条用例报错：

```
0x008865E0: blob mismatch at +0x20C: game=0xA5 cpp=0x01
```

改回 `+0x210` 并重新编译后，再次是 `0 mismatches`。
