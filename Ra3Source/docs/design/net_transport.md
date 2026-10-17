# 网络传输层

## 证据与边界

本轮依据 `ra3_1.0.game` 的 clean `.text`、私有 Ghidra 工程和离线 Unicorn
差分重建 `0x005D6000–0x005D7FC0` 中可独立验证的传输辅助代码。命名参考
Generals `GameNetwork/Transport.cpp`、`NetPacket.cpp`、`NetCommandMsg.cpp`
的概念，代码为按 RA3 反汇编重新实现。`0x005D63B0`、`0x005D7460`、
`0x005D7AC0` 是 inner-patched，均停在边界。

## 布局与格式

- `ByteQueue {data, capacity, readOffset, writeOffset}` 为 16 字节。帧长度小于
  `0x80` 时使用一个字节，否则使用大端的 15 位两字节长度，首字节置 `0x80`。
  pop 在目标容量不足时返回所需长度但不推进读游标。
- 连接矩阵有 6×6 个 32 位状态：`+0x1C` 为状态，`+0xAC` 为值，
  `+0x424` 为另一组计数，`+0x4B4` 为 16 位表。RA3 的有效玩家槽为 0–5。
- peer 延迟窗口位于 `+0x40`，128 个 32 位样本；`+0x240` 是环索引，
  `+0x244` 是总和。单个样本上限为 500。
- 包 `+8` 的高四位给出可变字段数；字段长度由首字节的 `0x80` 决定为
  1 或 2 字节，因此 payload 偏移从 10 开始逐项推进。
- 时间桶 `{float* values, count, interval, epoch}` 为 16 字节。跨
  `uint32` 时间回绕时原程序使用 `now - epoch - 1`，保留该少一毫秒的怪癖。
- endpoint 表有 6 个槽，每槽 `{handle,address,port}` 共 12 字节，基址
  `+0x40E00`。

## 已重建函数

| VA | Ra3Source | 行为 |
|---|---|---|
| `005d6000` | `clearPairPointers` | 清零两个内部指针 |
| `005d6010` | `linkedNodeAt` | 按 `+0x3FC` 链取第 N 节点 |
| `005d6040` | `unlinkNode` | 从节点链摘除 |
| `005d64c0` | `copyBounded` | 带可选 end 的复制 |
| `005d69c5` | `matrixCell` | 7×7 参数检查及单元定位 |
| `005d6a00` | `resetConnectionMatrix` | 清空六玩家连接矩阵 |
| `005d6a70` | `rowHasConnectedPeer` | 检查双向状态 2 |
| `005d6ac0` | `connectionMatrixReady` | 局部或全体连接完成检查 |
| `005d6b90` | `connectionPairReady` | 两玩家连接可用性 |
| `005d6c00` | `connectionValue` | 读取矩阵值 |
| `005d6e50`,`005d7250` | `elapsedPastTimeout` | 处理回绕的 5 秒超时 |
| `005d6ea0` | `addLatencySample` | 128 项限幅环形窗口 |
| `005d6fa0` | `compactAndAppend` | 队列压紧、扩容、追加 |
| `005d7020` | `popFramed` | 帧反序列化 |
| `005d70a0` | `appendFramed` | 帧序列化 |
| `005d72a0` | `packetHeaderSize` | 可变包头长度 |
| `005d7500` | `activePeerPendingTotal` | 六 peer 求和 |
| `005d75e0` | `copyTransportRecord` | tag 加 14 dword 复制 |
| `005d7640` | `makeConnectionCode` | 生成 `A/S/J + 两位数 + E/M/H` |
| `005d77d0` | `setOpaqueValue` | 四字节状态设置 |
| `005d77e0`,`005d7850`,`005d7880`,`005d78f0` | `TimeBuckets` 系列 | 初始化、清空、累计、求和 |
| `005d7ae0` | `scramblePacketWords` | XOR 序列并逐 dword 网络字节序 |
| `005d7ea0`,`005d7f20`,`005d7f70`,`005d7fc0` | endpoint 槽系列 | 清除、设置、取走、更新 |
| `005d88e0` | `countNonzeroBytes` | 统计字节区间中的非零项 |
| `005d8910` | `minimumCombinedMagnitude` | 两个等长整数区间的最小合成幅值 |

## Hook 策略

只挂了九个不会触发 socket、线程同步或分配器的入口。矩阵、包和游戏对象读取
目标均 `explicitOnly=true`。只读入口先计算 Ra3Source 结果，再调用原函数并比较
标量；两个状态变换对调用前快照应用我们的实现，再与原函数写出的精确范围比较。
延迟窗口、队列、endpoint 槽和时间桶可能由网络线程并发修改，刻意不挂。

## 未解决问题

- `ReliableTransport` 是 UDP 实现与 PeerThread 实现的运行时分派门面；其余门面函数
  含断言/日志或会进入 socket、临界区，留待有完整导入调用轨迹后处理。
- 单机遭遇战预计可能调用包头计算、连接代码和 active-pending 查询；联机握手、
  endpoint 槽和重传队列在单机流程中预计低覆盖或零覆盖。
- `0x005D7AE0` 以 EAX 传长度、ECX 传缓冲区，是编译器内部约定，不适合作为普通
  C++ hook 入口，故只保留离线实现。
