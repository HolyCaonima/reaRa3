# 0x005D0000 可靠传输与房间槽

## 范围

本批保留 `0x005D0000–0x005DFFFF` 里 42 个 clean、原先 `unanalyzed` 的函数，全部在 `ra3::net`（`ReliableTransport5D`）。每个函数要么是 `vtables_split.tsv` 的槽，要么是这种槽的直接 `E8`/`E9`，要么被一个推出源码路径字符串的函数调用。`verify_ownership.py` 对 `work/net5d2_claims.txt` 的结果是 `fails=0`。

有类名 getter（slot 2 为 `mov eax, imm32; ret`）的只有这些表：

- `SkirmishGameInfo` `00BDC918`
- `GameInfo` `00BE8018`
- `LANGameInfo` `00BEFCA0`

其余表的 slot 2 不是类名 getter，函数按操作命名。构造函数写虚表的位置是从镜像里读出来的：

- `005DD0A0` 在 `005DD0AE` 写 `00BEEDB0`，并清 `+0x24/+0x28/+0x2C`
- `005E6D20`（账本源文件 `GameSPYNATImpl.cpp`，长度 391）在 `005E6DC7` 写 `00BEE470`
- `005E71B0` 在 `005E71BD` 写 `00BEF6E8`
- `0061FF10` 在 `0061FF22` 写 `00BF0DDC`
- `00611930` 在 `00611942` 写 `00BF0BB4`
- `0061C900` 在 `0061C912` 写 `00BF1198`
- `00622B30` 在 `00622B38` 写 `00BF1558`

`00BE700C` 的四个字节是 `ffff7f7f`，即 `FLT_MAX` 的位型 `0x7F7FFFFF`。`00BBED30` 是空 C 字符串，子对象为空时若干槽返回它。

## 类和布局

`GameInfo` / `SkirmishGameInfo` 在 `this+4` 放六个槽指针。人类状态是 6。本地人类还要求 `this+0x2C` 非 0，并且槽 `+0x38` 等于 `this+0x30`。已接受槽的状态是 `{2,3,4,5,6}` 且 `+0x54` 非 0。

`00BEE470` 是 NAT 缓冲：长度在 `+0x14`，位置在 `+0x1C`，数据指针在 `+0x20`。读是有符号 `cmovg` 截断，`count < 0` 返回 `-1`。定位原点 0/1/2，`ja` 拒绝越过长度，`pos == size` 允许，失败返回 `0xFFFFFFFF` 且不改位置。

`00BEEDB0` 的子对象在 `this+0x28`。通道戳在 `child + index*0x188 + 0x4C`，只在新值无符号小于旧值时写入。`005DD100` 的对局对象是全局 `00DF6B1C`，帧在全局 `00DEBA64` 的 `+0x58`。

`00BEF6E8` 有两条链：宽链 next 在 `+0x3FC`、头在 `this+0x28`；短链 next 在 `+0x10`、头在 `this+0x24`。宽链摘除时若全局 `00DF6B1C` 等于被摘节点，先把全局写成 0，并且不清除被摘节点自己的 next。短链在找到或没找到时都把节点 next 清 0；头节点为空且节点非空会解引用空指针，oracle 不覆盖这条。端口 0 是地址通配。

`LANGameInfo::clearLanHumanReady` 读全局 `00DF6B2C`。非空时调用其虚表 `+0x7C` 并丢掉返回值，然后在 `0x100/0x180/0x200/0x280/0x300/0x380` 处状态为 6 时把后一个字节清 0。函数末尾 `eax` 仍是常量 6。

## 函数

| VA | 归属 | C++ |
| --- | --- | --- |
| `005D3260` | `0060D390` @ `0060D3E6`（`00BEF6E8` slot 37） | `endpointsEqual` |
| `005D3290` | `0060CD10` @ `0060CE85`（slot 43） | `endpointsDiffer` |
| `005D32C0` | `0060CD10` @ `0060CFB9` | `clampTinyState` |
| `005D3770` | `00BF0DDC` slot 4 | `childByte2E` |
| `005D3790` | `00BF0DDC` slot 5 | `childByte2D` |
| `005D37B0` | `00BF0DDC` slot 10 | `childDword30` |
| `005D37D0` | `00BF0DDC` slot 13 | `childField8C` |
| `005D3C50` | `00BF0BB4` slot 4 | `childByte29` |
| `005D3C70` | `00BF0BB4` slot 5 | `childByte28` |
| `005D4010` | `00BF1198` slot 8 | `cursorFieldsDiffer` |
| `005D4920` | `00BF1558` slot 97 | `storePair124` |
| `005D4960` | `00BF1558` slot 64 | `flagOrPointerSet` |
| `005D4DF0` | `00AB5000` @ `00AB5018`（`00C730A4` slot 8） | `initTaggedPointer` |
| `005D5450` | `00BEE470` slot 3 | `natBufferRead` |
| `005D5540` | `00BEE470` slot 5 | `natBufferSeek` |
| `005D5B50` | `005C5650` @ `005C5A37`，推 `00BE9860` | `otherSlotSharesId` |
| `005D5BD0` | SkirmishGameInfo slot 14 | `clearSlotIpAddresses` |
| `005D5C00` | `00B1F5C0` @ `00B1F5CC`（`00C7D3F8` slot 3） | `stateIsSixOrSeven` |
| `005D5C40` | SkirmishGameInfo slot 34 | `dismissIndexedSlot` |
| `005D5F80` | `005CD2B0` @ `005CD2DE`，推 `00BE9F30` | `slotStateIsAi` |
| `005D6010` | `00BEF6E8` slot 48 | `chainNodeAt` |
| `005D6040` | `005EF520` @ `005EF62D`（slot 18） | `unlinkWideChain` |
| `005D60B0` | `00BEF6E8` slot 61 | `findEndpointNode` |
| `005D60F0` | `00601210` @ `0060130B`（slot 29） | `unlinkShortChain` |
| `005D6140` | `00BEF6E8` slot 52 | `headEndpointMatches` |
| `005D6430` | `0060CD10` @ `0060CF0E` | `recordAtFc` |
| `005D6840` | `005C0E80` @ `005C0E95`（`00BE8538` slot 1） | `attachAndRetain` |
| `005D68B0` | `00BEEDB0` slot 15 | `linkStateIsOne` |
| `005D68C0` | `00BEEDB0` slot 27 | `linkChildField8` |
| `005D68D0` | `00BEEDB0` slot 35 | `clampChannelStamp` |
| `005D68F0` | `00BEEDB0` slot 36 | `setChannelBit` |
| `005D8480` | `00BF0DDC` slot 11 | `childDword6C` |
| `005D84A0` | `00BF0DDC` slot 12 | `childDword7C` |
| `005DB600` | `004F6C50` @ `004F6E11`（`00BDCC74` slot 4） | `initFloatExtents` |
| `005DB640` | `0062F890` @ `0062FD32`（slot 5） | `occupiedRecordField` |
| `005DB670` | `0060CD10` @ `0060CEAE` | `setOccupiedRecordField` |
| `005DBFF0` | `004DD2C0` 的 `E9`（SkirmishGameInfo slot 6） | `countAcceptedSlots` |
| `005DC180` | GameInfo slot 12 | `findLocalHumanSlot` |
| `005DC1C0` | GameInfo slot 11 | `slotZeroIsLocalHuman` |
| `005DC1F0` | GameInfo slot 17 | `otherSlotIsHuman` |
| `005DCF10` | LANGameInfo slot 13 | `clearLanHumanReady` |
| `005DD100` | `00BEEDB0` slot 31 | `channelFrameReached` |

`005D4010` 只改 AL：返回值是 `(field_f0 & 0xFFFFFF00) | (field_f0 != field_f4)`。`005D4DF0` 把 `00BEE370` 写进 `this`，所以它是构造函数。`005D5450` 经 `004D2EDA`（`jmp [0xBBA53C]`，memcpy 导入桩）拷贝，调用方 `add esp, 0xC`。`005D5C40` 只在有符号下标属于 1..5 时调用槽虚表 `+0x10`，然后把 `+0xFA` 写成 `0xFF`。`005D6010` 在减到 0 之后仍会再读一次 `+0x3FC`，因为 `test` 的标志穿过了那条 `mov`。`005D68F0` 的 `shl edx, cl` 只使用低 5 位。`005DC1F0` 每轮都虚调用 slot 12，成功路径是 `mov al, 1`。

样板按简报的四种形态计数为 0：没有 slot 2 类名 getter、NAMEKEY 缓存、返回常量桩、只转发基类的 xfer。另外 8 个 `+0xC0` 子字段中继（四个字节、三个 dword、一个 `+0x8C` 指针）是薄访问器，8/42 约为 19%，没有算进那四种样板。

## 未计入

- `005D3F40`、`005D6C40`、`005D6CD0`、`005D6D60`、`005DD1E0`、`005DD2F0`、`005DD3D0`、`005DD4C0`、`005DD5B0`、`005DD670`、`005DD6F0`、`005DD8A0`、`005DD9B0`：默认分支 `push` 路径字符串并 `call 0040EBF0`。账本已把它们标成 net5d 推迟的断言门面，状态仍是 `unanalyzed`，本批不改记。
- `005D7A00`：前缀是比较和 `fld1`，随后 `push 0xBEE840`。账本源文件是 `TickerDataHandler.cpp`，已由 net5d 推迟，不是可单独对齐的队列叶。
- `005D63B0`：状态为 6 时 `jmp dword ptr [0x18ED698]`，账本 `inner-patched`。`005D6450`（LANGameInfo slot 12）调用它，无法在模拟器里跑真实循环。
- `005DD150`、`005DD160`：`mov ecx, [ecx+0x28]; jmp` 后接 `int3`，没有自己的行为。
- `005D3450`：`add ecx, 0x1CC0; jmp 004CA9B0`。
- 析构、释放、分配器、套接字和线程调用按简报跳过。没有一条能证明的一跳边的纯叶也不计入。

## Hook

无。

## Oracle

`tools/net5d2check` 与 `tools/analysis/ports/verify_net5d2.py` 对 42 个入口比较 blob 和 EAX。指针返回折回 blob 内偏移；只写 AL 的函数比较低 8 位。覆盖了序号回绕、空链、满缓冲、`pos == size`、负计数、端口 0 通配，以及本地人类落在第 6 个槽。

```
net5d2 oracle: 0 mismatches, 128 cases, 42 functions
```

把 `findLocalHumanSlot` 的槽上限从 6 改成 5 并重新编译后，oracle 报：

```
0x005DC180: return (full) game=0x00000005 cpp=0xFFFFFFFF
```

改回 6 并重新编译后再次是 `0 mismatches, 128 cases, 42 functions`。Release 与 Debug 的 `ra3_net5d2check` 都编过，没有新警告。
