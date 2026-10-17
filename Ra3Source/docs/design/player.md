# Player / ScoreKeeper 逆向记录

本页只记录 `ra3_1.0.game` clean `.text` 的静态反汇编与 Unicorn 离线差分结论。
Generals 的 `Common/RTS/Player.cpp`、`Money.cpp`、`ScoreKeeper.cpp` 和
`Include/Common/Player.h` 仅用于命名与结构参照；RA3 偏移均由下列地址直接恢复。

## 1. 已恢复布局

`Player` 的完整构造函数仍含大量外部依赖，本批不声称恢复完整对象大小；clean 成员访问
证明其可见下界为 `0x123d`。`PlayerList` 的 20 槽数组由
`0x007e1b50/0x007e1c70/0x007e2060` 交叉确认。

| 类 | 偏移 | 类型 | 名称 | 证据 |
|---|---:|---|---|---|
| Player | `+0x8c` | pointer | AI/控制器对象 | `007e15e0`, `007e17a0`, `007e18c0` |
| Player | `+0xf8` | u32 | 未命名运行值 | `007e1930` |
| Player | `+0x106` | bool | observer | `007e1b80`, `007e1ba0` |
| Player | `+0x10b` | bool | listInScoreScreen | `007e1840`，对应 Generals 同名 setter |
| Player | `+0x10d` | bool[] | indexed state | `007e1b40` |
| Player | `+0x128` | pointer[20] | indexed object slots | `007e1b50` |
| Player | `+0x1c8` | u32 | 未命名值 | `007e1b70` |
| Player | `+0x1f8` | float | 未命名比例 | `007e1950` |
| Player | `+0x123c` | bool | playerDead | `007e1b90`, `007e1ba0` |
| Player | `+0x1258` | u32 | 未命名值 | `007e15d0` |
| Player | `+0x125c` | s32 | 饱和到零的计数器 | `007e1530` |
| Player | `+0x1278` | pointer | selection 状态 | `007e1c00–007e1c40` |
| PlayerList | `+0x30` | s32 | playerCount | `007e1c80`, `007e2060` |
| PlayerList | `+0x34` | pointer[20] | players | `007e1c70`, `007e2060` |
| ScoreKeeper | `+0x04,+0x0c` | s32 | 基础统计值 | `007e25d0`, `007e25e0` |
| ScoreKeeper | `+0x10` | s32 | 基础值写锁 | `007e25d0`, `007e25e0` |
| ScoreKeeper | `+0x74..+0xc0` | s32[20] | 统计矩阵 B | `007e27a0` |
| ScoreKeeper | `+0xd0..+0x11c` | mixed | 对局统计与计时 | `007e2600–007e2760` |
| ScoreKeeper | `+0x134,+0x135` | bool | 状态标志 | `007e2800–007e2820` |
| ScoreKeeper | `+0x144` | s32 | 累计资金 | `007e2830` |

`ScoreKeeper` 当前可见下界为 `0x148`。统计开关来自全局对象
`*(0x00deba64)+0xab`，当前帧来自 `+0x58`，每秒帧数是 `0x00cb0910`。

## 2. 函数与 hook

本批 C++ 入口集中在 `ra3/common/Player.h`，覆盖 Player、PlayerList、
ScoreKeeper 以及同窗口的小型布局算法。离线 oracle 对返回值、`ret n` 和函数实际写入
字节逐项比较。

| 地址范围 | Ra3Source | Generals 对应 | hook key |
|---|---|---|---|
| `007e1530–007e20f0` | `Player`, `PlayerList` | `Player` / `PlayerList` | `playeradjustcounter`, `playeractive` |
| `007e25d0–007e2830` | `ScoreKeeper` | `ScoreKeeper` | `scoreaddd0`, `scoreincrement100`, `scoreincremente8`, `scoreincrementec`, `scoreincrementf0`, `scoreincrementd8`, `scoreadddc`, `scorestoptimer`, `scoreflag134`, `scoreflag135`, `scoremark134`, `scoreaddmoney` |
| `007e13b0,007e1480,007e21e0,007e2260–007e2470,007e2ca0,007e2d30–007e2d40` | `player_port` 小型算法 | 无可靠一一对应 | 未挂 |

所有 hook 都读取游戏对象布局，故 `explicitOnly=true`。状态变换使用
“复制最小恢复对象 → 在副本执行 Ra3Source → 调原函数 → 只比较原函数会写的字段”；
`playeractive` 是只读标量比较。

## 3. 怪癖与边界

- `Player::isPlayerActive` 的 RA3 语义是 `!observer && !dead`。
- `ScoreKeeper` 的多个增量函数在全局 `+0xab` 为零时完全不写对象。
- 计时使用无符号 32 位减法，因此保留帧计数回绕行为。
- `007e25d0/007e25e0` 的前缀含短条件跳转，当前 detour 不重定位短跳，故只移植、不挂。
- x87 返回的 `007e13b0` 已实现，但未挂；普通 C++ 包装不能保证完整 80 位 ST0。
- 构造/析构路径会进入尚未移植的分配器、字符串和受保护函数，本批止步于 clean 边界。

## 4. 未解决

- Player 完整对象大小、vptr 和完整构造函数仍需从分配调用者及运行时对象样本交叉确认；
  当前 API 明确只声明“最小恢复大小”，不把下界伪装成完整大小。
- `+0x1258/+0x125c`、`+0x1c8/+0x1f8` 尚缺足够调用者证据，暂不套用 Generals 字段名。
- Money/Energy 的 RA3 独立对象边界尚未在本窗口内可靠分离。
