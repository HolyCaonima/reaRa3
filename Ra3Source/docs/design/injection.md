# 路线 A 注入与逐函数替换

## 1. 范围

本设计用于把已经重建并验证的函数逐个接入正在运行的 32 位
`ra3_1.0.game`。注入不是启动器，不修改磁盘上的游戏文件，也不处理保护代码。
登记的游戏目标在 `src/hook/dll/Targets.cpp` 的目标表里（见 §4.1）。

`Ra3::hook` 在目标入口写入五字节 `jmp rel32`，直接跳到替换函数；32 位下 rel32
按 2^32 回绕，任意两地址之间都可达。可执行 trampoline 先执行完整的 stolen bytes，
再跳回原函数。trampoline 从共享的 64 KiB 可执行块里按 16 字节切槽：每次
`VirtualAlloc` 都要占 64 KiB 地址空间，32 位进程装不下成千上万个独立分配。

安装和卸载都执行以下事务：

1. 先在堆上准备好全部数据（trampoline、原字节、补丁字节、Detour 对象）。
2. 创建 Toolhelp32 线程快照，挂起当前进程的其他线程。
3. 读取每个线程的 EIP；EIP 落在待改写范围时恢复所有线程并重试，持续出现则失败。
4. 在所有线程保持挂起时比较内存中的 expected bytes。
5. 临时把代码页设为可写，写入补丁，刷新指令缓存并恢复原页保护。
6. 恢复所有线程。

第 2–6 步不得使用堆（线程句柄放在定长数组里，错误信息在恢复线程后才拼）：被挂起的
游戏线程可能正持有堆锁，此时再分配内存会让安装线程永久卡死。

expected bytes 必须覆盖全部 stolen bytes。每个目标固定校验入口前 16 字节，只偷取
开头若干条完整的指令（§4.1）。唯一允许的位置相关指令是目标表 `rel32Offsets` 列出的
`E8/E9 rel32` call/jmp：安装时确认这些偏移确为 `E8/E9` 且整条指令在 stolen 范围内，
trampoline 中的副本按原目的地址重算位移（`Hook_RebasesStolenRel32Call` 覆盖）；其他
相对寻址仍拒绝。任何字节不同都会拒绝安装。卸载也要求入口
仍然等于本实例写入的补丁，避免覆盖其他工具的改动。

## 2. SecuROM 边界

本方案不脱壳、不解密、不绕过检查，也不分析或改写 `Stext`、`Sdata`、
`.securom`。它不改入口点，不参与保护启动过程：

- 先由原版启动器正常启动游戏，等主窗口出现后才用 `CreateRemoteThread` 和
  `LoadLibraryW` 附加 DLL。
- 候选地址必须来自 `docs/analysis/ra3_1.0/function_ledger.tsv`，同时满足
  `section == .text` 和 `securom == clean`。其余状态一律不能作为 hook 目标。
- 每个可发布目标还要把地址、足够长的原字节和 stolen 指令边界写进受评审的
  hook 注册项，不能只从运行时配置任意指定替换函数。
- 内存字节校验是版本门槛，不把“地址相同”当作“版本相同”。

若游戏自身或保护系统拒绝带 hook 运行，立即停止实验，保存普通启动与注入后的
日志并记录现象；不得隐藏 DLL、禁用检查或尝试规避。

## 3. DLL 与配置

`ra3hook.dll` 的 `DllMain` 只关闭线程通知并启动 worker。读取配置、打开日志、
检查 PE 信息和安装 hook 都在 worker 上执行。配置路径优先取目标进程启动时已有的
`RA3_HOOK_CONFIG` 环境变量，否则读取 DLL 同目录的 `ra3hook.ini`。后者最适合首次
实验，因为不需要把任何文件放进游戏目录。

配置示例：

```
mode=shadow
namehash=replace
summary_interval_ms=1000
```

`mode` 是所有目标的默认模式（`off|shadow|replace`），`<目标键>=模式` 单独覆盖
某个目标。地址和期望字节只来自编译进 DLL 的目标表，配置不能指定游戏内地址。
所有启用的目标要么全部安装成功，要么一个都不留（已装的会被卸载）。可选
`log=绝对路径`；默认写入 `%LOCALAPPDATA%\Ra3Source\logs\ra3hook.log`。日志记录
主模块基址、PE 时间戳、镜像大小、每个目标的 `installed <键> mode=<模式>`，以及每个
目标一行的原子计数 `summary <键> calls/matches/mismatches`。每个目标的前五次
shadow 不一致另写明参数和两边结果。`target=绝对地址` 只用于无游戏集成测试，
只作用于 `refpack`，其他目标自动关闭。

注入器 `ra3inject` 接受 `--pid` 或 `--name`。名称匹配多个进程时拒绝猜测；
`--wait-window` 最多等 60 秒可见顶层窗口。它先确认目标是 32 位进程，再在目标中
运行一个小型 loader stub。stub 调用 `LoadLibraryW`，并把返回模块地址和目标进程
的 `GetLastError` 写回注入器。

## 4. shadow 到 replace

### 4.1 目标表

| 键 | 函数 | ABI | stolen | Ra3Source 实现 | 状态 |
|---|---|---|---:|---|---|
| `refpack` | `0x009F72E0` 一次性 RefPack 解码 | stdcall | 5 | `ra3::io::decompressRefPackPrefix` | live verified |
| `namehash` | `0x00417DB0` 名称哈希 | cdecl | 8 | `ra3::asset::hashName` | live verified |
| `pathcompare` | `0x009F3650` 档案路径比较 | cdecl | 5 | `ra3::io::comparePathNames` | live verified |
| `refpackprobe` | `0x009F74F0` RefPack 大小探测 | stdcall | 6 | `ra3::io::probeRefPackSize` | live verified |
| `stringcopylimit` | `0x009F3470` 限长复制 | cdecl | 6 | `ra3::common::copyStringLimited` | live verified |
| `stringappendlimit` | `0x009F34B0` 限长追加 | cdecl | 5 | `ra3::common::appendStringLimited` | live verified |
| `stringcomparelimit` | `0x009F3500` 限长比较 | cdecl | 5 | `ra3::common::compareStringsLimited` | live verified |
| `stringcomparecaselimit` | `0x009F3560` ASCII 不分大小写限长比较 | cdecl | 5 | `ra3::common::compareStringsCaseInsensitiveLimited` | live verified |
| `stringcopy` | `0x009F35E0` 字符串复制 | cdecl | 5 | `ra3::common::copyString` | live verified |
| `stringcompare` | `0x009F3610` 字符串比较 | cdecl | 6 | `ra3::common::compareStrings` | live verified |
| `stringlength` | `0x009F36C0` 字符串长度 | cdecl | 6 | `ra3::common::stringLength` | live verified |
| `stringlowercopy` | `0x009F36E0` ASCII 小写复制 | cdecl | 5 | `ra3::common::copyStringLowercase` | live verified |
| `stringfindlast` | `0x009F3720` 反向查找字节 | cdecl | 7 | `ra3::common::findLastCharacter` | offline-verified；运行 12/13 未被调用 |
| `bytewiden` | `0x009F3F10` 字节扩宽至 16 位 | cdecl | 5 | `ra3::common::widenBytes` | offline-verified；运行 12/13 未被调用 |
| `bytecompare` | `0x004C9EA0` 定长字节比较 | stdcall | 7 | `ra3::common::compareBytes` | live verified |
| `prefixedccompare` | `0x004CB3A0` 长度前缀串与 C 串比较 | thiscall | 5 | `ra3::common::compareLengthPrefixedToCString` | live verified |
| `prefixedcompare` | `0x004CB400` 两个长度前缀串比较 | thiscall | 6 | `ra3::common::compareLengthPrefixed` | live verified |
| `fnvhash` | `0x00A28290` ASCII 小写 FNV-1a | cdecl | 7 | `ra3::common::hashLowercaseFnv1a` | live verified |
| `utf8decode` | `0x00A282D0` 宽松 UTF-8 解码 | cdecl | 6 | `ra3::common::decodeUtf8` | offline-verified；运行 12/13 未被调用 |
| `utf8encode` | `0x00A28350` UTF-8 编码 | cdecl | 9 | `ra3::common::encodeUtf8` | offline-verified；运行 12/13 未被调用 |
| `utf8next` | `0x00A284B0` UTF-8 解码并前移 | cdecl | 6 | `ra3::common::decodeUtf8AndAdvance` | live verified |
| `floatmax` | `0x00504620` 浮点最大值 | cdecl | 6 | `ra3::math::maximum` | offline-verified, live pending |
| `floatclamp` | `0x00504650` 浮点夹取 | cdecl | 6 | `ra3::math::clamp` | offline-verified, live pending |
| `vectorequal3` | `0x00504680` `Vector3` 精确相等 | cdecl | 8 | `ra3::math::exactlyEqual` | live verified |
| `vectorrangeequal2` | `0x005046C0` `Vector2` 区间精确相等 | cdecl | 5 | `ra3::math::exactlyEqualVector2Range` | live verified |
| `squaredistance3` | `0x005049E0` `Vector3` 距离平方 | cdecl | 8 | `ra3::math::squaredDistance` | offline-verified（80-bit ST0）, live pending；NaN payload 待实机 |
| `floatmin` | `0x00522A30` 浮点最小值 | cdecl | 6 | `ra3::math::minimum` | offline-verified, live pending |
| `approximatelength3` | `0x00523240` `Vector3` 近似长度 | cdecl | 7 | `ra3::math::approximateLength` | offline-verified（80-bit ST0）, live pending；NaN payload 待实机 |
| `colorargb` | `0x005235B0` ARGB 转 `Vector4` | cdecl | 12 | `ra3::math::colorFromArgb` | offline-verified, live pending |
| `contains2` | `0x00528BC0` 二维包含测试 | cdecl | 8 | `ra3::math::containsInclusive` | offline-verified, live pending |
| `separate2` | `0x00528C00` 二维点对分离 | cdecl | 8 | `ra3::math::separate` | offline-verified, live pending |
| `clampoffset68` | `0x0055A9B0` 对象 `+0x68` 浮点夹取 | thiscall | 5 | `ra3::math::clampFloatAtOffset68` | live verified |
| `distance3` | `0x00564150` `Vector3` 距离 | cdecl | 8 | `ra3::math::distance` | live verified（80-bit ST0，游戏 CW 0x007F）；NaN payload 待实机 |
| `normalize2` | `0x0056F370` `Vector2` 归一化 | cdecl | 9 | `ra3::math::normalize` | live verified（运行 19 暴露 EAX 返回值缺失，已修复）；NaN payload 待实机 |
| `squaredistanceoffset38` | `0x006ABCB0` 到对象 `+0x38` 向量的距离平方 | thiscall | 7 | `ra3::math::squaredDistanceFromOffset38` | offline-verified（80-bit ST0）, live pending；NaN payload 待实机 |
| `crc32cstring` | `0x0092A0D0` 零结尾字节串 CRC-32 | cdecl | 6 | `ra3::common::crc32CString` | offline-verified, live pending |
| `scalevector3` | `0x009814E0` 对象向量缩放 | thiscall | 7 | `ra3::math::scaled` | live verified |
| `transformdirection3` | `0x00994A10` `Matrix3x4` 方向变换 | thiscall | 9 | `ra3::math::transformDirection` | live verified；多 NaN payload 待实机 |
| `bigfindhash` | `0x009F6B90` 单档案哈希表查找 | thiscall | 5 | `ra3::io::findGameBigHashedEntry` | live verified（shadow；replace 时唯一调用者已被替换）; 仅显式键启用 |
| `bigfindentry` | `0x009F6D00` 单档案路径/序号查找 | thiscall | 7 | `ra3::io::findGameBigEntry` | live verified; 仅显式键启用 |
| `bigreadbe32` | `0x009F6810` 变宽大端 32 位读取 | cdecl | 7 | `ra3::io::readGameBigEndian32` | live verified |
| `bigreadbe64` | `0x009F6880` 变宽大端 64 位读取 | cdecl | 5 | `ra3::io::readGameBigEndian64` | live verified |
| `bigformat` | `0x009F6910` BIG/C0FB/Viv4 格式判别 | cdecl | 7 | `ra3::io::detectGameBigFormat` | live verified |
| `bighashpath` | `0x009F6990` BIG 路径 hash | cdecl | 7 | `ra3::io::hashGameBigPath` | live verified |
| `bigheadersize` | `0x009F6A50` 目录头长度 | cdecl | 5 | `ra3::io::gameBigHeaderSize` | live verified |
| `bigentrycount` | `0x009F6AA0` 目录条目数 | thiscall | 6 | `ra3::io::gameBigEntryCount` | live verified；仅显式键启用 |
| `bigtrailersize` | `0x009F6B00` 目录尾标识 | thiscall | 5 | `ra3::io::gameBigTrailerSize` | live verified；仅显式键启用 |
| `bigstoredsize` | `0x009F5E80` 文件 stored size | stdcall | 7 | `ra3::io::gameBigStoredSize` | live verified；仅显式键启用 |
| `bigabsoluteoffset` | `0x009F5E90` 文件绝对数据偏移 | stdcall | 6 | `ra3::io::gameBigAbsoluteDataOffset` | live verified；仅显式键启用 |
| `gametexttable` | `0x00539080` 单个 GameText 排序表查询 | thiscall | 5 | `ra3::text::findGameText(GameStringTable*)` | live verified；仅显式键启用 |
| `gametextmanager` | `0x0053B500` GameText 三表优先级查询 | thiscall | 6 | `ra3::text::findGameText(GameTextManager*)` | live verified；仅显式键启用 |
| `bigseek` | `0x009F5E00` BIG 文件 seek | thiscall（6 stack dword） | 6 | `ra3::io::seekGameBigFile` | live verified；仅显式键启用 |
| `bigclose` | `0x009F5DB0` BIG 文件 close | thiscall | 5 | `ra3::io::closeGameBigFile` | live replace-only（运行 18，341 次，对局正常）；仅显式键启用；shadow 仅转发 |
| `randomstep` | `0x005A9310` 六 dword RNG 核心步进 | thiscall | 5 | `ra3::sim::nextRandom` | live verified；仅显式键启用 |
| `randominit` | `0x005A93B0` 三随机流统一播种 | cdecl | 10 | `ra3::sim::seedRandom` | offline-verified, live pending；仅显式键启用 |
| `logicrandomint` | `0x005A94C0` 逻辑流闭区间整数 | cdecl | 5 | `ra3::sim::logicRandomInteger` | live verified；仅显式键启用 |
| `clientrandomint` | `0x005A94F0` 客户端流闭区间整数 | cdecl | 5 | `ra3::sim::clientRandomInteger` | live verified；仅显式键启用 |
| `audiorandomint` | `0x005A9520` 音频流闭区间整数 | cdecl | 5 | `ra3::sim::clientRandomInteger` | live verified；仅显式键启用 |
| `logicrandomreal` | `0x005A9550` 逻辑流实数区间（x87 ST0） | cdecl（naked） | 7 | `ra3::sim::randomRealExtended` | live verified；仅显式键启用 |
| `clientrandomreal` | `0x005A95A0` 客户端流实数区间（x87 ST0） | cdecl（naked） | 7 | `ra3::sim::randomRealExtended` | live verified；仅显式键启用 |
| `audiorandomreal` | `0x005A95F0` 音频流实数区间（x87 ST0） | cdecl（naked） | 7 | `ra3::sim::randomRealExtended` | live verified；仅显式键启用 |
| `rotatingcrc` | `0x0092A0A0` Generals `CRC::computeCRC` 字节循环 | cdecl（3） | 6 | `ra3::common::addRotatingCrc` | live verified；仅显式键启用 |
| `logicseedcrc` | `0x005A9390` `GetGameLogicRandomSeedCRC` | cdecl | 9 | `ra3::sim::randomStateCrc` | live verified；仅显式键启用 |
| `randomvariableset` | `0x005A9640` RandomVariable 设置 (low, high, type) | thiscall（3） | 6 | `ra3::sim::setRandomVariable` | live verified；仅显式键启用 |
| `randomvariablecopyrange` | `0x005A9660` RandomVariable 复制（先 low/high） | thiscall（1，EAX=this） | 6 | `ra3::sim::copyRandomVariable` | live verified；仅显式键启用 |
| `randomvariablecopytype` | `0x005A9680` RandomVariable 复制（先 type） | thiscall（1，EAX=this） | 6 | `ra3::sim::copyRandomVariable` | live verified；仅显式键启用 |
| `clientrandomvariable` | `0x005A96A0` `GameClientRandomVariable::getValue`（x87 ST0） | thiscall（naked） | 5 | `ra3::sim::clientRandomVariableExtended` | live verified；仅显式键启用 |
| `logicrandomvariable` | `0x005A9730` `GameLogicRandomVariable::getValue`（x87 ST0） | thiscall（naked） | 6 | `ra3::sim::logicRandomVariableExtended` | offline-verified，遭遇战中未被调用；仅显式键启用 |
| `bigfindclose` | `0x009F5EB0` BIG 枚举状态释放 | thiscall | 6 | allocator-backed release | offline-verified, live pending；仅显式键启用 |
| `bigopen` | `0x009F5F80` BIG 文件 open | thiscall（3 stack dword） | 6 | `ra3::io::prepareGameBigFile` + tagged allocation | live verified；仅显式键启用 |
| `bigread` | `0x009F63B0` BIG 文件 read | thiscall（5 stack dword） | 5 | `ra3::io::readGameBigFile` | live verified；仅显式键启用 |
| `bigfindfirst` | `0x009F65E0` BIG 枚举开始 | thiscall（2 stack dword） | 5 | `ra3::io::initializeGameBigFindState` + `findNextGameBig` | offline-verified, live pending；仅显式键启用 |
| `bigfindnext` | `0x009F6670` BIG 枚举推进 | thiscall（2 stack dword） | 6 | `ra3::io::findNextGameBig` | offline-verified, live pending；仅显式键启用 |
| `asciistringcopy` | `0x004CA8F0` `StringBase<char>` 拷贝构造 | thiscall（1 stack dword，EAX=this） | 9（rel32 @4） | `ra3::common::copyGameString` | live verified；仅显式键启用 |
| `asciistringassign` | `0x004CA9B0` `StringBase<char>` 赋值 | thiscall（1） | 5 | `ra3::common::assignGameString` | live verified；仅显式键启用 |
| `asciistringensure` | `0x004CA710` `StringBase<char>::ensureUniqueBufferOfSize` | thiscall（4） | 5 | `ra3::common::ensureUniqueGameString` | live verified；仅显式键启用 |
| `asciistringlower` | `0x004CAA30` `StringBase<char>` 转小写 | thiscall | 5 | `ra3::common::mapGameString` + CRT `tolower` | live verified；仅显式键启用 |
| `asciistringupper` | `0x004CAA80` `StringBase<char>` 转大写 | thiscall | 5 | `ra3::common::mapGameString` + CRT `toupper` | live verified；仅显式键启用 |
| `asciistringconcat` | `0x004CB6E0` `StringBase<char>` 追加 (text, length) | thiscall（2） | 7 | `ra3::common::concatGameString` | live verified（空串目标交回原函数）；仅显式键启用 |
| `unicodestringcopy` | `0x004CAD90` `StringBase<wchar_t>` 拷贝构造 | thiscall（1，EAX=this） | 9（rel32 @4） | `ra3::common::copyGameString` | live verified；仅显式键启用 |
| `unicodestringrelease` | `0x004CAB60` `StringBase<wchar_t>` 释放 | thiscall | 9（rel32 @4） | `ra3::common::releaseGameString` | live verified；仅显式键启用 |
| `unicodestringensure` | `0x004CABA0` `StringBase<wchar_t>::ensureUniqueBufferOfSize` | thiscall（4） | 5 | `ra3::common::ensureUniqueGameString` | live verified；仅显式键启用 |
| `unicodestringlower` | `0x004CAF40` `StringBase<wchar_t>` 转小写 | thiscall | 5 | `ra3::common::mapGameString` + CRT `towlower` | live verified；仅显式键启用 |
| `unicodestringremovelast` | `0x004CAF90` `StringBase<wchar_t>` 删除末字符 | thiscall | 5 | `ra3::common::removeLastGameCharacter` | live verified；仅显式键启用 |
| `unicodestringset` | `0x004CB9B0` `StringBase<wchar_t>` 设置 (text, length) | thiscall（2） | 6 | `ra3::common::setGameString` | live verified；仅显式键启用 |
| `unicodestringconcat` | `0x004CBA00` `StringBase<wchar_t>` 追加 (text, length) | thiscall（2） | 7 | `ra3::common::concatGameString` | live verified；仅显式键启用 |
| `netrowconnected` | `0x005D6A70` 连接矩阵行状态检查 | thiscall（1） | 5 | `ra3::net::rowHasConnectedPeer` | offline-verified, live pending；仅显式键启用 |
| `netconnectionvalue` | `0x005D6C00` 连接矩阵值读取 | thiscall（2） | 5 | `ra3::net::connectionValue` | offline-verified, live pending；仅显式键启用 |
| `netpacketheadersize` | `0x005D72A0` 可变长包头长度 | cdecl（1） | 5 | `ra3::net::packetHeaderSize` | offline-verified, live pending；仅显式键启用 |
| `netactivepending` | `0x005D7500` 活跃 peer 待处理量求和 | thiscall | 8 | `ra3::net::activePeerPendingTotal` | offline-verified, live pending；仅显式键启用 |
| `netcopyrecord` | `0x005D75E0` 传输记录复制 | thiscall（2，EAX=this） | 6 | `ra3::net::copyTransportRecord` | offline-verified, live pending；仅显式键启用 |
| `netconnectioncode` | `0x005D7640` 四字节连接代码 | cdecl（3） | 5 | `ra3::net::makeConnectionCode` | offline-verified, live pending |
| `netopaquevalue` | `0x005D77D0` 四字节状态设置 | thiscall（1，EAX=value） | 6 | `ra3::net::setOpaqueValue` | offline-verified, live pending；仅显式键启用 |

这些目标均由 shipped 文件逐个复核 expected16；stolen 前缀位置无关，且函数内
后向边均不落入各自前 `stolen` 字节。限长复制/追加的 shadow 只读写原函数会触及的字节
（源串按 `count` 截断求长，目标缓冲可能短于 `count`）。

ports2 的 17 个目标同样逐个用 `disasm.py --hook-only` 复核 expected16、完整指令边界、
位置无关前缀和函数内后向边。写指针的 shadow 先在临时对象上计算结果，原函数仍是唯一
写游戏内存的一方；`separate2` 另处理两个输出指针相同的别名情形。

四个直接返回 x87 计算结果的目标不能经过 C++ `float` 临时量：
`squaredistance3`、`distance3`、`approximatelength3` 和
`squaredistanceoffset38` 的 naked hook 用两个十字节临时区保存并比较完整 `ST0`。
shadow 最终重新装入游戏值，replace 重新装入重建值，因此调用者收到的尾数和扩展指数
均不被单精度舍入。其余 x87 浮点目标返回的是从内存装入的精确单精度值。每个 x87
目标还在入口采集调用者 CW 与 MXCSR；汇总行的
`cw=0x.... mxcsr=0x........ cw_changes=N` 给出首次值和后续 CW 变化次数，采集过程
不修改浮点环境。
三个 RNG 实数区间目标同理：naked 入口在栈上留十字节区，C++ `runRandomReal` 在
replace 时把 `randomRealExtended` 的 `ST0` 以 `fstp tbyte` 写入，shadow 时写入原函数
结果并与状态快照上的重建结果逐字节比较（同时比较六 dword 状态），入口再 `fld tbyte`
返回。
bigfs4 的五个目标同样只认显式键。建议首次 live shadow 配置：

```
bigopen=shadow
bigread=shadow
bigfindfirst=shadow
bigfindnext=shadow
bigfindclose=shadow
```

Open shadow 不分配，只在栈上构造预期 0x40 状态；Read shadow 的额外字节验证使用
独立只读 handle，限制为 8 MiB，未验证调用记录 `skipped`；FindFirst/FindNext 在
0x110 状态副本上运行。FindClose shadow 在原调用前预测非空状态的返回值，调用后
不再访问已释放对象。replace
使用已验证 allocator/file/sync ABI 和布局兼容的 0x70 inflater。live 应尽早注入，
进入主菜单和遭遇战，以覆盖压缩与明文成员、加载期 seek/read 以及资源枚举。
运行 24–28（§5.4）的结果是 open/read live verified；三个枚举目标在注入后没有被调用。

`Ra3Source/tools/analysis/disasm.py <VA> --hook-only` 从只读的可执行文件直接给出前 16 字节、
stolen 长度（≥5 字节的最短整指令前缀）及其是否位置无关、返回形式（`ret` = cdecl，
`ret N` = stdcall/thiscall）。

读取游戏自有布局的 `bigfindhash`、`bigfindentry` 默认关闭，不继承全局 `mode`；
必须在配置中显式写出对应键。这样仅启用既有 `refpack` 的集成测试日志不会出现新目标。
同样，`bigentrycount`、`bigtrailersize`、`bigstoredsize`、`bigabsoluteoffset` 读取游戏
对象，也只认显式键；其余五个只读调用方缓冲的纯函数继承全局模式。第二批 9 个目标的
expected16 均从发行文件读取，stolen 前缀位置无关且没有后向边落入前缀；每个目标经
Unicorn 与 `bigcheck --stdio` 对 1000 例比较，均为 0 mismatch。
`gametexttable`、`gametextmanager` 同样读取游戏自有布局，也只认显式键。两者用真实
`gamestrings.csf` 构造的 10,401 项表做 69,594 次查询（每个标签、其大小写互换、截短、
加 `_`、末字节加一，另有地图专用标签与空串），分别对单表、只有主表的管理器和
override/primary/map 三表叠加的管理器执行：Unicorn 中的原函数与 `textcheck` 中的
C++ 各 208,782 例，均与同一模型 0 不一致；
shadow 只比较返回指针，不进入或创建缺失字符串回退。

五个 RNG 目标都修改全局或调用方提供的随机状态，因此均为 `explicitOnly`。shadow
在调用原函数前快照完整的 6×32-bit 状态，原函数返回后只对快照应用 Ra3Source
转移，再比较返回值和完整后状态，不回写游戏内存；`randominit` 比较三个状态块及
逻辑基种子。整数 helper 的原函数可能再次经过已安装的 `randomstep` hook，这只产生
嵌套观测，不会额外推进状态。

引擎字符串类的 13 个目标（`docs/design/game_string.md`）修改共享的引用计数块，均为
`explicitOnly`。shadow 先把输入块复制到私有副本（锁与分配都用私有 ops），在副本上
运行移植，再调用原函数，然后比较字符串最终指向的是原块还是新块、结果块的头与字符、
以及其余输入块的引用计数；不写游戏内存。调用方传入的复制源若不是两个 `{text, length}`
vtable 之一则 shadow 记为 skipped。replace 绑定游戏自己的类锁（getter
`0x004CA440`/`0x004CA490`，vtable 槽 0 进入，IAT `LeaveCriticalSection` 离开）、分配
`0x004165A0`、释放 `0x004166B0`、源 vtable 与 CRT 大小写函数；原函数会转入
inner-patched 代码的情形（请求超过 0x7FFF 字节、ASCII 空串追加）移植不做任何事并返回
false，hook 改调原函数并计入 skipped。除两个拷贝构造返回 this 外，原函数的 EAX 是
`LeaveCriticalSection` 等的残值，调用点扫描中标为“使用”的 EAX 均是先写 AL/AX 的误报。

新增目标：在 `Targets.cpp` 写一个与原函数同 ABI 的 hook（shadow 调原函数并比较，
replace 只调我们的实现），在表里加一行 `{键, RVA, 前 16 字节, stolen 长度, hook,
状态}`，并在本表登记。RVA 以 0x00400000 为基址；文件偏移 = VA − 0x00400000。

本批跳过 `0x0050B5F0`、`0x006C3A10`、`0x009C9980..0x009C9A50` 等 EASTL
容器内部；跳过依赖全局随机状态的 `0x005A94C0`；跳过 x87/SSE 浮点候选
`0x004D2EE0`、`0x005A9550`、`0x004D9100`、`0x009C9DD0`；`0x009F3760`
虽是纯优化字节复制，但对重写价值低且重叠输入契约不明确，也未接入。

### 4.2 refpack

RefPack hook 的签名是
`int __stdcall Decode(uint8_t* dest, const uint8_t* src)`。

`shadow` 先通过 trampoline 调用原函数，返回值和 `dest` 对调用者保持不变。然后
用同一 `src` 调用 `ra3::io::decompressRefPackPrefix`，以上述返回大小为比较范围，
逐字节比较结果。比较代码的异常被截断为 mismatch，不能穿过游戏 ABI。

`replace` 不调用原函数。`src == nullptr` 时返回 0；否则读取 RefPack 声明大小，
用重建解码器写入 `dest` 并返回大小。无长度 ABI 意味着它和原函数一样信任输入是
可访问的完整 RefPack 流；当前只应用于游戏内部已验证的调用链。

### 4.3 namehash

`shadow` 用原函数结果作返回值，同时用同一参数调用 `ra3::asset::hashName` 比较；
不一致时记录名字（最多 64 字节，不可打印字节转义）、长度、seed、finalize 和两边
结果。`replace` 只调用我们的实现。原函数里的 `tolower` 走 CRT（IAT `0x00BBA4D4`），
所以 shadow 也同时检验了“游戏运行时是默认 C locale”这一假设。

### 4.3.1 refpackprobe

`0x009F74F0` 先按 `(头字 & 0x3EFF) == 0x10FB` 判定 RefPack，不是则返回 0；是则经
`0x009F6810` 按大端读紧跟签名的 3 或 4 字节（flag 0x80）。flag 0x01 时读到的是压缩
大小而不是解压大小，与 `readRefPackHeader` 不同，`probeRefPackSize` 原样保留。离线
预言机（`tools/analysis/emulate.py`）对 256 种 flag 字节与 3,000 组随机头共 4,024 例
0 不一致，栈清理 4 字节。shadow 的不一致只打印原函数实际读过的 2/5/6 个字节。

### 4.4 pathcompare

原函数只在两边是大小写相反的字母时才调整左边字符的大小写，且按有符号字节比较，
所以相等判定等于 ASCII 不分大小写，但排序与 `stricmp` 不同（`"z" > "_"`，而
`"Z" < "_"`）。`comparePathNames` 原样复刻这一排序；`BigArchive::find` 用哈希表，
只依赖相等判定，两者一致。shadow 的不一致记录两边字符串和返回值。

### 4.5 收缩流程

逐函数收缩流程固定为：

1. 从台账选择 `.text/clean` 且已有等价重建实现的低风险叶函数。
2. 静态确认签名、调用约定、完整指令边界、原字节和所有已知调用方。
3. dummy 单测覆盖 expected-byte 拒绝、trampoline 和卸载。
4. 先跑 shadow；代表性场景中调用数充足且 mismatch 始终为零。
5. 新进程改跑 replace，重复相同场景并与未注入基线比较。
6. 任一阶段异常就撤回该目标，不扩大 hook 范围。

## 5. 首次真实游戏实验

以下命令只在主 agent 审核代码后执行。PowerShell 当前目录为
`D:\Game\Ra3\RevRa3\Ra3Source`，先 `cmake --build build/win32 --config Release`。
配置和 DLL 都在构建目录，游戏安装目录保持只读。结果见 5.4。

### 5.1 shadow

```
@'
mode=shadow
summary_interval_ms=1000
'@ | Set-Content -Encoding ASCII build\win32\bin\Release\ra3hook.ini

Remove-Item "$env:LOCALAPPDATA\Ra3Source\logs\ra3hook.log" -ErrorAction SilentlyContinue
```

用原版启动器正常启动 RA3。启动器 `RA3.exe` 本身没有 SecuROM 区段（只有
`.text/.rdata/.data/.rsrc`）；按它的字符串，它按语言挑一个 `*_*.SkuDef`，以
`-config "<SkuDef 路径>"`（可选 `-runver`）启动 SkuDef 里 `set-exe` 指定的
`Data\ra3_1.0.game`。

游戏进程出现后立即执行下面的注入命令：`--wait-window` 会在第一个可见顶层窗口出现时
注入。这时保护代码的启动阶段已经结束（窗口由游戏代码创建），而大部分 BIG 压缩成员
还没读，正好让 RefPack hook 覆盖整个加载过程。等到主菜单再注入，多数解码已经做完。

```
build\win32\bin\Release\ra3inject.exe --name ra3_1.0.game --dll D:\Game\Ra3\RevRa3\Ra3Source\build\win32\bin\Release\ra3hook.dll --wait-window
Get-Content "$env:LOCALAPPDATA\Ra3Source\logs\ra3hook.log" -Wait
```

若同名进程不止一个，先运行 `Get-Process ra3_1.0.game` 查 PID（镜像扩展名不是
`.exe`，进程名保留 `.game`），再把 `--name
ra3_1.0.game` 换成 `--pid <PID>`。注入器应报告非零 `LoadLibraryW result` 和
`GetLastError=0`。日志必须显示基址 `0x400000`、发行版时间戳和镜像大小，随后每个
目标一行 `installed <键> mode=shadow`。进入一次会读取 BIG 压缩成员的正常流程；
只接受每个目标的 `calls` 增长且 `mismatches=0`。

### 5.2 replace

完全退出游戏。把配置第一行改成 `mode=replace`，清空日志，重新正常启动一个新
游戏进程，再执行同一条注入命令。重复 shadow 的场景，确认
`installed <键> mode=replace`、`calls` 增长、`mismatches=0`，并检查加载、主菜单和
进入对局没有可见偏差。也可以只把某个目标设为 replace（如 `namehash=replace`），
其余保持 shadow。

### 5.3 撤销

补丁只存在于进程内。完全退出游戏即可撤销；不要在已注入进程中覆盖 DLL或重复
注入。删除构建目录旁的 `ra3hook.ini` 只会阻止下一次安装，不改变游戏文件。若
注入、保护或游戏行为出现任何异常，退出进程、保存日志并停止路线 A 实验。

### 5.4 结果（2026-10-16，`RA3.exe -win`，英文 SkuDef）

实际游戏命令行：`D:\Game\Ra3\Game\Data\ra3_1.0.game -win -config
"D:\Game\Ra3\Game\RA3_english_1.0.SkuDef"`。每次注入都报告非零
`LoadLibraryW result` 与 `GetLastError=0`，日志首行均为
`host base=0x400000 timestamp=0x48ef023a image_size=0x26a14f0`，没有出现完整性
检查拒绝。唯一一次崩溃（运行 19）是我们 hook 包装的 ABI 缺陷，见表后说明。

| 运行 | 模式 | 注入时机 | 到主菜单时的计数 | 结果 |
|---|---|---|---|---|
| 1 | shadow | 主菜单之后（按 PID） | `calls=0` | 安装成功；菜单期间不再解码 RefPack |
| 2 | shadow | `--wait-window`，进程出现约 1 秒 | `calls=38 matches=38 mismatches=0` | 加载期全部调用逐字节一致 |
| 3 | replace | `--wait-window`，进程出现约 3 秒 | `calls=38 matches=38 mismatches=0` | 原函数不再执行；主菜单正常渲染 |
| 4 | 两目标 shadow | `--wait-window` | refpack 38/38；namehash `calls=5579 matches=5579 mismatches=0` | 加载期名称哈希全部一致（含运行时 C locale 假设） |
| 5 | 两目标 replace | `--wait-window` | refpack 38/38；namehash 5554/5554 | 两个原函数都不再执行；完整主菜单正常渲染 |
| 6 | 三目标 shadow | `--wait-window` | refpack 38/38；namehash 5573/5573；pathcompare `calls=227359 matches=227359 mismatches=0` | 路径比较（含排序怪癖）全部一致 |
| 7 | 三目标 replace | `--wait-window` | refpack 38/38；namehash 5603/5603；pathcompare 230337/230337 | 三个原函数都不再执行；主菜单正常 |
| 8 | 三目标 shadow，进入遭遇战 | `--wait-window` | 对局中 refpack 58/58；namehash 117975/117975；pathcompare 451773/451773 | 地图加载与对局期间全部一致 |
| 9 | 三目标 replace，进入遭遇战 | `--wait-window` | 对局中 refpack 58/58；namehash 115099/115099；pathcompare 389740/389740 | 原函数不执行也能进入对局并正常运行 |
| 10 | 四目标 shadow，进入遭遇战（asset2 合并后的解码器） | `--wait-window` | 对局中 refpack 58/58；namehash 116231/116231；pathcompare 467104/467104；refpackprobe 556/556 | 全部一致 |
| 11 | 四目标 replace，进入遭遇战 | `--wait-window` | 对局中 refpack 58/58；namehash 117452/117452；pathcompare 575667/575667；refpackprobe 549/549 | 四个原函数都不执行，对局正常运行 |
| 12 | 23 目标 shadow（含显式启用的 bigfind*），进入遭遇战 | `--wait-window` | 对局中 19 个目标被调用，全部 0 不一致；如 bigfindentry 2632470/2632470、pathcompare 762327/762327、stringlength 273450/273450、bytecompare 269853/269853、fnvhash 45230/45230、bigfindhash 4627/4627 | stringfindlast、bytewiden、utf8decode、utf8encode 未被调用 |
| 13 | 23 目标 replace，进入遭遇战 | `--wait-window` | 对局中 bigfindentry 2694036、pathcompare 763499、stringlength 276354、bytecompare 257685 次全部由 Ra3Source 处理；bigfindhash 0 次（唯一调用者 bigfindentry 已被替换） | 对局正常运行 |
| 14 | 同运行 13，改用 Release 构建的 DLL | `--wait-window` | 对局中 bigfindentry 2694111、stringlength 309882、pathcompare 348589、bytecompare 257687 次全部由 Ra3Source 处理 | 约 50 秒到主菜单，对局正常运行 |
| 15 | 32 目标 shadow（Release；6 个 explicitOnly 键全开），进入遭遇战 | `--wait-window` | 28 个目标被调用，全部 0 不一致；bigfindentry 内部的辅助函数如 bigformat 10778341/10778341、bigheadersize 5388212/5388212、bigreadbe64 2689159/2689159、bigentrycount 2696023/2696023、bigtrailersize 2678773/2678773，另 bigabsoluteoffset 7150/7150、bighashpath 4947/4947、bigstoredsize 368/368 | 未被调用的仍是 stringfindlast、bytewiden、utf8decode、utf8encode |
| 16 | 32 目标 replace（Release），进入遭遇战 | `--wait-window` | bigfindentry 2632470、bigentrycount 2629704、bigabsoluteoffset 7006、bigstoredsize 340 次由 Ra3Source 处理；bigformat 等 6 个辅助函数 0 次（调用者已被替换，改走同一份 C++） | 对局正常运行 |
| 17 | 35 目标 shadow（Release；新增 gametexttable、gametextmanager、bigseek 显式键），进入遭遇战 | `--wait-window` | 31 个目标被调用，全部 0 不一致；gametexttable 707/707、gametextmanager 673/673、bigseek 3682/3682 | bigclose 未启用（shadow 只转发，无可比较对象） |
| 18 | 36 目标 replace（Release，含 bigclose），进入遭遇战 | `--wait-window` | gametextmanager 640、gametexttable 290、bigseek 3574、bigclose 341、bigfindentry 2663337 次由 Ra3Source 处理 | 主菜单与遭遇战界面文字正常，对局正常运行；bigclose 的释放由我们的代码经游戏分配器完成 |
| 19 | 53 目标 shadow（Release，新增 ports2 的 17 个数学目标，bigclose 也开），遭遇战加一个 Easy AI | `--wait-window` | 崩溃前全部 0 不一致；normalize2 1/1 | BEGIN 后约 60 秒 `ra3_1.0.game` 在 0x005869B3 访问违例；同样设置不注入的基线对局正常 |
| 20 | 同运行 19，修复 4 个包装的 EAX 返回值后 | `--wait-window` | 53 个目标全部 0 不一致；normalize2 570/570、distance3 919/919（80-bit 比较）、transformdirection3 1208/1208、scalevector3 225/225、clampoffset68 31/31、vectorrangeequal2 4/4、vectorequal3 1/1；游戏 CW 0x007F、MXCSR 0x1FA0，`cw_changes=0` | 对局 2 分钟正常；ports2 另 10 个目标未被调用 |
| 21 | 53 目标 replace（Release），遭遇战加 Easy AI | `--wait-window` | normalize2 570、distance3 262、scalevector3 224、transformdirection3 198、clampoffset68 30、bigclose 388、bigseek 3761、gametextmanager 875、bigfindentry 2661046 次由 Ra3Source 处理 | 对局 2 分钟正常，无崩溃事件 |
| 22 | 58 目标 shadow（Release，新增 simrng 的 5 个显式键），遭遇战加 Easy AI | `--wait-window` | 58 个目标全部 0 不一致；randomstep 256113/256113、logicrandomint 23888/23888、clientrandomint 248/248、audiorandomint 278/278（返回值与六 dword 状态都比较） | randominit 0 次（播种早于注入或走别的路径）；对局 2 分钟正常 |
| 23 | 58 目标 replace（Release），遭遇战加 Easy AI | `--wait-window` | randomstep 221723、logicrandomint 22174、clientrandomint 109、audiorandomint 159、bigfindentry 2632560 次由 Ra3Source 处理 | 对局 2 分钟正常，无崩溃事件 |
| 24 | 12 个 BIG/GameText 显式键 shadow（Release，新增 bigopen、bigread），遭遇战加 Easy AI | `--wait-window` | bigopen 486 次中 357 次不一致；bigread 705/3800（其余 skipped）；其余目标 0 不一致 | 对局正常；bigfindfirst/next/close 0 次 |
| 25 | 同运行 24，bigopen 不一致改为逐 dword 报告 | `--wait-window` | 不一致全部只在 +0x30 `sourcePosition`，游戏值比我们多 0x10 | 只到主菜单；原因见表后说明 |
| 26 | 同运行 24，`prepareGameBigFile` 修复后 | `--wait-window` | bigopen 232 次中 67 次不一致，全部只在 +0x14 | 只到主菜单；+0x14 是原函数从不写入的分配器残留 |
| 27 | 同运行 24，shadow 比较改取游戏的 +0x14 | `--wait-window` | 全部 0 不一致；bigopen 502/502、bigread 725/3725（3000 次成员大于 8 MiB 而 skipped）、bigseek 3725/3725、bigclose 370/370 | 对局 2 分钟正常，无崩溃事件 |
| 28 | 15 个显式键 replace（Release，含 bigopen、bigread、bigfind*），遭遇战加 Easy AI | `--wait-window` | bigopen 480 次（351 次打开、129 次未找到）、bigread 3676、bigseek 3687、bigclose 354、bigfindentry 2627891、gametextmanager 736 次由 Ra3Source 处理 | 对局 2 分钟正常，无崩溃事件；枚举三目标仍 0 次 |
| 29 | 全局 shadow 加 13 个字符串显式键 shadow（Release），遭遇战加 Easy AI | `--wait-window` | 13 个字符串目标全部被调用、0 不一致：asciistringcopy 3094060、asciistringensure 645065、asciistringassign 461287、unicodestringrelease 33658 等；asciistringconcat 20480 次中 8528 次是空串目标而 skipped | 对局 2 分钟正常，无崩溃事件 |
| 30 | 全局 replace 加全部 33 个显式键 replace（共 76 目标，Release），遭遇战加 Easy AI | `--wait-window` | 字符串 13 目标共约 427 万次由 Ra3Source 处理（asciistringcopy 3092434、asciistringensure 625688、asciistringassign 460862、unicodestringrelease 33144）；asciistringconcat 7869 次空串目标交回原函数 | 对局 2 分钟正常，无崩溃事件 |
| 31 | 全局 shadow 加 8 个 RNG 显式键 shadow（Release），遭遇战加 Easy AI | `--wait-window` | logicrandomreal 16828/16828、clientrandomreal 18437/18437、audiorandomreal 220713/220713（十字节 ST0 与六 dword 状态逐字节），randomstep 179620/179620；client 实数调用者 CW 变化 18372 次 | 对局 2 分钟正常，无崩溃事件 |
| 32 | 全局 replace 加全部 36 个显式键 replace（共 79 目标，Release），遭遇战加 Easy AI | `--wait-window` | logicrandomreal 17379、clientrandomreal 19944、audiorandomreal 231678 次由 Ra3Source 处理；randomstep 降为 9961（实数 helper 改走 `nextRandom`） | 对局 2 分钟正常，无崩溃事件 |
| 33 | 全局 shadow 加 14 个 RNG/CRC 显式键 shadow（Release），遭遇战加 Easy AI | `--wait-window` | clientrandomvariable 108747/108747（十字节 ST0 与状态逐字节，调用者 CW 变化 108558 次）、randomvariablecopytype 17152/17152、randomvariablecopyrange 11630/11630、randomvariableset 6108/6108、rotatingcrc 2811/2811、logicseedcrc 6/6；logicrandomvariable 0 次 | 对局 2 分钟正常，无崩溃事件 |
| 34 | 全局 replace 加全部 43 个显式键 replace（共 86 目标，Release），遭遇战加 Easy AI | `--wait-window` | clientrandomvariable 68235、randomvariablecopytype 17152、randomvariablecopyrange 11630、randomvariableset 6108、rotatingcrc 2805、logicseedcrc 4 次由 Ra3Source 处理；randomstep 只剩 8 次 | 对局 2 分钟正常，无崩溃事件 |

运行 19 的崩溃点紧跟 `call 0x0056F370`（normalize2）：调用者把 EAX 当作结果指针
解引用（`mov edx, [eax]`）。原函数按 MSVC 返回结构体的惯例把 `output` 留在 EAX，
而包装声明为 `void`，EAX 是垃圾值。离线 oracle 只比较输出内存，所以没发现。同类的
colorargb、scalevector3、transformdirection3 一并改为返回 `output`（shadow 返回原函数的
EAX）；utf8encode、separate2、clampoffset68、bigclose 的原函数在 `ret` 时 EAX 只是
临时值，保持 `void`。`verify_math.py` 现在对这 4 个函数断言 `eax == output`。之前的
运行都没有 AI 对手，normalize2 没被调用过，所以直到运行 19 才暴露。

运行 24 的 bigopen 不一致同样是离线 oracle 的桩掩盖的。原函数在
0x009F6110–0x009F6167 用一次异步读取成员开头 16 字节做 RefPack 探测：0x009CA390
排队，0x009CA280 等待并返回 op +0x38 的已传输字节数，随后
`sourcePosition = offset + 已传输字节数`。旧移植让 `sourcePosition = offset`，
`verify_bigfs4_open.py` 的 0x009CA280 桩恰好返回 0，所以两者一致。现在
`prepareGameBigFile` 无条件读 16 字节，不论成员大小都探测，oracle 桩返回真实的
传输字节数。原函数从不写 +0x14，值是分配器残留（运行 26 见到 0x6265645C 等），
Read/Seek/Close/Size 也不读它，所以 shadow 比较取游戏值。Viv4（format 2）档案改走
0x009F71D0，并把 `sourcePosition` 设为 `streamBase`。这条路径未移植；遇到时
replace 交回原函数并计入 `skipped`，发行版 BIG 不走这条路。运行 24 的 129 次
"一致"其实全是双方都未找到的调用。

运行 8、9 的操作：主菜单点掉离线提示 → Versus → Skirmish → 默认设置（Battlebase
Beta，单人，阵营随机）→ BEGIN。点击由 `SetCursorPos`/`mouse_event` 发往前台游戏
窗口，截图核对每一步。运行 9 之前有一次注入在进程出现约 1 秒时失败（远程模块快照
尚未就绪，游戏未被注入，结果作废）；注入器现对远程函数解析重试 5 秒。

四个未被调用的目标的直接调用点（扫描 .text 的 `call/jmp rel32`，无函数指针引用）：
stringfindlast 只被 `AddArchiveFromFile` 0x009F5AF0 调用，挂载档案发生在第一个窗口
之前，按注入时机永远观察不到；bytewiden 只被 0x009F1F80–0x009F47F0 的 9 个格式化
辅助函数调用（它们用常量 "(null)" 代替空字符串指针）；utf8decode 的唯一调用点在
0x00A3EB32，utf8encode 的在 0x00A34300/0x00A34370，遭遇战流程都没有走到。

运行 12、13 的 DLL 是 Debug 构建：bigfindentry 在加载期约 220 万次调用，加载到主菜单
要数分钟。Release 构建（`cmake --build build/win32 --config Release`，同样零警告、
ctest 全过）在运行 14 中约 50 秒到主菜单，此后实机运行默认用 `bin/Release`。运行 13 之前有一次作废：配置脚本与启动命令并行执行，DLL
读到了上一轮的 shadow 配置（日志显示 `mode=shadow`）；脚本现等待旧进程退出、确认日志
已删除后才返回，启动必须在它之后。

replace 模式下 `matches` 表示 Ra3Source 解码成功并把结果交给游戏。运行 3 的主菜单
截图中字体、贴图和菜单框体完整；弹出的 "Failed to connect to servers" 是 GameSpy
下线后的离线提示，与 hook 无关。

结论：路线 A 在真实进程上成立——注入时机在保护启动阶段之后，`.text` 内联补丁
未被拒绝，已复刻的函数可以整体替换原实现，多个目标可以同时替换，并且替换后能进入
对局。尚未覆盖：长时间对局与存读档；窗口出现前的少量调用可能早于注入，shadow 无法
证明覆盖了全部调用。

## 6. 已知风险

- Toolhelp32 快照与新线程创建之间仍有理论竞态；当前事务覆盖快照中所有其他线程，
  并拒绝 EIP 位于改写范围的时刻。
- 无输入长度的原 ABI 无法在坏指针或截断流上提供内存安全保证。
- shadow 会额外分配输出缓冲并增加解码开销，不用于性能结论。
- 当前 DLL 面向“注入后随进程退出”的实验，不支持 `FreeLibrary` 热卸载。撤销方式
  是退出目标进程。进程退出时 worker 线程先被系统终止，可能正持有日志锁，所以
  `DLL_PROCESS_DETACH` 只在拿得到锁时写最后一次汇总。
- 代码完整性检查属于必须尊重的外部约束，不在本项目规避范围内。
- 包装必须复现原函数的完整返回约定，不只是写出的内存：结构体结果经隐藏指针返回时
  EAX 也是返回值，x87 结果要保留 80 位 ST0。新目标要在反汇编里核对每个 `ret` 前的
  EAX/ST0，oracle 也要比较它们。
