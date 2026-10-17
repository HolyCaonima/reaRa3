# View 摄像机簇（`recon/client52`）

## 范围

`ra3_1.0.game` 窗口 `0x00520000`–`0x0053FFFF` 里仍有 754 个 clean 且 unanalyzed 的函数。本批只计数 `View` 主虚表上能证明所有权、并且离线 oracle 可以逐位对照的 58 个。实现在 `ra3::gameclient` 的 `GameClient52.h` / `GameClient52.cpp`。检查器是 `ra3_client52check`，oracle 是 `tools/analysis/ports/verify_client52.py`。没有 hook。

主虚表 `0x00BE0440`（133 槽）。槽 2 `0x00529C20` 是 `mov eax, 0x00BE0658; ret`，`0x00BE0658` 的 C 字符串是 `View`。另一张拆分表 `0x00BE55C8` 的槽 2 也是这个 getter，并重复了其中一部分函数体；每个函数体只计一次，所有权都记在 `0x00BE0440`。

`0x00529C60` 不在虚表里。槽 5 `0x00531330` 在写入 `0x280` / `0x1E0` 之后用 `E9` 尾调用它（指令在 `0x0053133E`）。

字段偏移来自这些函数自己的访存，不是 Generals 的 `View` 头文件。位置在 `+0x0C/+0x10/+0x14`，视口宽高在 `+0x18/+0x1C`，原点在 `+0x20/+0x24`，角度三元组在 `+0x28/+0x2C/+0x30`，FOV 在 `+0x44`，缩放历史在 `+0x6C/+0x70`，守卫点在 `+0x8C/+0x90/+0x94`，抖动在 `+0x98`，近远裁剪在 `+0x9C/+0xA4`。

常量也是从映像读的：`0x00BD9528` = 0.5，`0x00BD94C4` = 度到弧度，`0x00BDD198` = 12，`0x00BDF8F0` = 1e10，`0x00BDFBD8` = 0.96，`0x00BDFBDC` = 1.05，`0x00BDFBE4` / `0x00BDFBE0` = ±0.62831855，`0x00BD9548` = 0.2，`0x00BE0654` = 0.87266463，`0x00BD90A4` = 1。

## 函数

| VA | C++ | 所有权 |
| --- | --- | --- |
| `0x00529C60` | `View_resetCameraDefaults` | `0x00531330` 在 `0x0053133E` 尾调用 |
| `0x00531330` | `View_initDefaultViewport` | `0x00BE0440` 槽 5 |
| `0x00529820` | `View_setLockByte` | 槽 8 |
| `0x00529830` | `View_setViewportWidth` | 槽 10 |
| `0x00529840` | `View_setViewportHeight` | 槽 12 |
| `0x00529850` | `View_setViewportOrigin` | 槽 14 |
| `0x00529870` | `View_getViewportOrigin` | 槽 15 |
| `0x00529D40` | `View_setPositionFromScreen` | 槽 18 |
| `0x00529DB0` | `View_addPositionXY` | 槽 20 |
| `0x00529890` | `View_setPositionFromScreenIf` | 槽 21 |
| `0x005225F0` | `View_setAngleClamped` | 槽 50 |
| `0x005298E0` | `View_getAngle` | 槽 51 |
| `0x005298F0` | `View_getPitch` | 槽 53 |
| `0x00522620` | `View_copyAnglePitchDefaults` | 槽 54 |
| `0x00529900` | `View_copyPosition` | 槽 55 |
| `0x00529920` | `View_getZoom` | 槽 59 |
| `0x00529930` | `View_getFov` | 槽 61 |
| `0x00529940` | `View_setFov` | 槽 62 |
| `0x00522580` | `View_scaleFovDown` | 槽 63 |
| `0x005225B0` | `View_scaleFovUp` | 槽 64 |
| `0x00529950` | `View_setByte7D` | 槽 67 |
| `0x00529960` | `View_getHeight` | 槽 68 |
| `0x00529970` | `View_setHeight` | 槽 69 |
| `0x00529980` | `View_getMaxHeight` | 槽 70 |
| `0x00529990` | `View_setMaxHeight` | 槽 71 |
| `0x005299A0` | `View_setTerrainHeight` | 槽 72 |
| `0x005299B0` | `View_getTerrainHeight` | 槽 73 |
| `0x00529DE0` | `View_captureCamera` | 槽 74 |
| `0x00522630` | `View_applyCamera` | 槽 75 |
| `0x005299C0` | `View_clearFilter` | 槽 78 |
| `0x005299E0` | `View_setFilterReady` | 槽 79 |
| `0x005299F0` | `View_setFilter` | 槽 80 |
| `0x00522530` | `View_pushZoom` | 槽 81 |
| `0x00529A10` | `View_getZoomCurrent` | 槽 82 |
| `0x00529A20` | `View_getZoomMode` | 槽 83 |
| `0x00529A30` | `View_setZoomMode` | 槽 84 |
| `0x00529A40` | `View_clearZoomFilter` | 槽 85 |
| `0x00529A60` | `View_setCameraByte` | 槽 86 |
| `0x00529A70` | `View_getExtraPitch` | 槽 89 |
| `0x00529A80` | `View_setExtraPitch` | 槽 90 |
| `0x00529A90` | `View_getGuardFlag` | 槽 94 |
| `0x00529AA0` | `View_getGuardId` | 槽 96 |
| `0x00529AB0` | `View_setGuardId` | 槽 97 |
| `0x00529AC0` | `View_setGuardPosition` | 槽 99 |
| `0x00529AE0` | `View_clearGuardPosition` | 槽 100 |
| `0x00529AF0` | `View_hasGuardPosition` | 槽 101 |
| `0x00529B10` | `View_copyGuardPosition` | 槽 102 |
| `0x00529B40` | `View_addShakeMagnitude` | 槽 106 |
| `0x00529B80` | `View_clearShake` | 槽 107 |
| `0x00529B90` | `View_getShake` | 槽 108 |
| `0x00529BA0` | `View_setShakeRange` | 槽 111 |
| `0x00529E90` | `View_distanceFromFov` | 槽 112 |
| `0x00522750` | `View_distanceFromHalfAngle` | 槽 113 |
| `0x00529BC0` | `View_getNearClip` | 槽 118 |
| `0x00529BD0` | `View_getFarClip` | 槽 119 |
| `0x00529BE0` | `View_setNearClip` | 槽 120 |
| `0x00529C00` | `View_setFarClip` | 槽 121 |
| `0x00522570` | `View_setViewId` | 槽 131 |

槽号都是 `0x00BE0440` 的十进制槽。槽 2 返回 `View`。

`View_setPositionFromScreen` 用 `cvtsi2ss` / `mulss` / `subss` 把屏幕点减去半宽半高，Z 保持原位模式。`View_setAngleClamped` 把角度夹到 `±0.62831855`。`View_addShakeMagnitude` 对负数先乘 `-1` 再累加。`View_pushZoom` 在当前缩放与 `+0` 做 `ucomiss`：相等时上一帧取 FOV（经 `fld`/`fstp`），否则复制当前缩放。`View_scaleFovDown` / `Up` 走虚表槽 `0xF4` 和 `0xF8`，分别做 `* 0.96 - 1` 和 `* 1.05 + 1`。`View_distanceFromFov` 是 `tan(angle * deg2rad * 0.5)`，正切为 0 时返回 `1e10`，否则返回 `12 / tan`；结果再 `fld m32`，所以 ST0 是舍入后的 float32。`View_distanceFromHalfAngle` 少了度到弧度那一次乘法。`View_captureCamera` 和 `View_applyCamera` 按虚表调用槽 `0xC4/0xCC/0xD4/0xF4` 与 `0x48/0xC0/0xC8/0xD0/0xF8/0x40`。槽 `0x40` 在原程序里是 `0x00411440`，会读全局 `0x00DE9B1C`；它不写 View 对象。oracle 把该槽换成返回 0 并记录 `n16` 的桩。

样板（槽 2 名字 getter、NAMEKEY 缓存、返回常量、只转发基类的 xfer）0 个。同形工厂 0 个。

## 未计数

同一张 View 表上读过、但没有计数的入口：

- `0x00529C20`：槽 2 类名 getter，`mov eax, imm32; ret`。
- `0x00529C30`：槽 0 析构，写虚表后调用 `0x004166B0` 释放。
- `0x005225E0`：槽 17，六字节跳进槽 18，没有自身行为。
- `0x005298B0`、`0x005298C0`：空的 `ret 0x1C` / `ret 0x18`。
- `0x005298D0`：`fldz; ret`，返回常量。
- `0x00531350`：槽 3 的 xfer，分配 `0x30` 字节节点、遍历 `+0x80` 链表，并调用 `0x0054A090` / `0x0054A0B0`。
- `0x0053BF20`：槽 6，调用链表释放 `0x00539D40`（里面再调用 `0x004CA930` 和 `0x004166B0`）。
- `0x00539D40`：上面的释放路径，不单独计数。
- `0x005226B0`：只被 `0x00531350` 调用的记录传输，依赖 xfer 虚表。
- `0x00529EF0`、`0x005227A0`：槽 114 / 115。正数距离走 `fpatan`，80 位尾数和本机不一致。输入 `0x3F800000` 时检查器得到 `aa0cfdc7570379aa0640`，Unicorn 得到 `230afdc7570379aa0640`。指数相同，低位尾数不同，不能逐位通过。

窗口里其余 clean 函数没有 View 槽位或来自上述锚点的直接调用边，本批不认领。

## Oracle

Release `ra3_client52check` 对 332 个输入（含 0、−0、Inf、QNaN、夹取边界、视口宽为 0 和 `0xFFFFFFFF`、缩放历史的相等/不等路径、apply 的 flag 0/1）比较 EAX、日志、对象内存和 80 位 ST0。

```
oracle mismatches=0 compared=332
```

语义突变：把 `View_setAngleClamped` 的写入偏移从 `0x2C` 改成 `0x30`，重新编译后 oracle 报 `mismatches=12 compared=332`（10 个 clamp 用例和 2 个 apply 用例）。改回之后再次得到 `oracle mismatches=0 compared=332`。

## Hooks

无。
