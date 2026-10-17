# SageAudio 设备层（`recon/audio50`）

## 范围

`ra3_1.0.game` 里 `0x00500000`–`0x0050FFFF` 的 30 个 clean、原先 unanalyzed 的入口。实现在 `ra3::audio` 的 `SageAudioManager50.h` / `SageAudioManager50.cpp`。离线 oracle 是 `tools/analysis/ports/verify_audio50.py`，对照 Release `ra3_audio50check`。没有 hook。

`0051BA00` 在 `this+0` 写入 `00BDF5A8`，在 `this+0x24` 写入 `00BDF594`。`00BDF594` 的 slot 2 是 `mov eax, 00BDEDB4; ret`，字符串为 `SageAudioManager`。`00BDF5A8` 的 slot 2（`004D6430`）不是这种取名函数，类名用这次写入和 `00BDF594` 确定。`00BDF3A4` 的 slot 2 返回 `SagePathMusicManager`（`00BDEDC8`）。

样板 3 个（两个 slot 2 类名 getter，一个 `mov eax, imm32; ret`），占 30 个计数函数的 10%。

## 计数函数

| VA | C++ | 证据 |
| --- | --- | --- |
| `0x00502270` | `resolveSpeakerCenterChannel` | `00515050` 在 `00515423` 调用；`00515050` 由 RenderingSound.cpp `00515B70` 在 `00515E2F` 调用 |
| `0x00502540` | `bindSoundProvider` | SagePathMusicManager `00BDF3A4` slot 1 `00506710` 在 `0050674E` 调用 |
| `0x00502FB0` | `sageAudioManagerEmptyEventAddress` | `00BDF5A8` slot 73，返回 `00CAA11C` |
| `0x00502FF0` | `sageAudioManagerStopAllImmediately` | `00BDF5A8` slot 4 |
| `0x00503290` | `sageAudioManagerListenerOrientationCount` | `00BDF5A8` slot 20 |
| `0x00503300` | `sageAudioManagerSubsystemNameAddress` | `00BDF594` slot 2 |
| `0x00503400` | `sagePathMusicManagerSubsystemNameAddress` | `00BDF3A4` slot 2 |
| `0x005044F0` | `initAudioEventHandle` | SageAudioManager.cpp `0051E560` 在 `0051E698` 调用 |
| `0x00504560` | `releaseAudioEventHandle` | SageAudioManager.cpp `005161C0` 在 `00516398` 调用 |
| `0x005045C0` | `releasePlayingAudioHandle` | SageAudioManager.cpp `0051E560` 在 `0051E9F4` 调用 |
| `0x005045E0` | `unlinkPlayingAudioNode` | `00519740` 在 `005198E9` 调用；`00519740` 由 `00519C50` 在 `00519FB0` 调用；`00519C50` 由 `00BDF594` slot 3 `0051A1C0`（SageAudioManager）在 `0051A3E1` 调用 |
| `0x00504AC0` | `testAudioEventCategoryBit` | SageAudioManager.cpp `0051D3C0` 在 `0051D607` 调用 |
| `0x00504AF0` | `intersectsAudioEventCategoryMask` | `00508510` 在 `005085E2` 调用；`00508510` 由 RenderingSound.cpp `00515B70` 在 `00515E28` 调用 |
| `0x00504C80` | `recomputePlayerCategoryVolume` | `00505340` 在 `00505384` 调用 |
| `0x00504DF0` | `recomputePlayerCategoryProduct` | `00505340` 在 `00505375` 调用 |
| `0x00504EC0` | `decrementPlayerVolumeCounters` | `00BDF5A8` slot 5 `0051EFF0` 在 `0051F182` 调用 |
| `0x00505340` | `acquireVolumeModifierLock` | `00BDF5A8` slot 71 `00506670` 在 `00506675` 调用 |
| `0x005053B0` | `releaseVolumeModifierLock` | `00BDF5A8` slot 72 `00506680` 在 `00506685` 调用 |
| `0x00506430` | `sageAudioManagerQuerySuppressed` | `00BDF5A8` slot 101 |
| `0x00506670` | `sageAudioManagerOnAcquireVolumeModifierLock` | `00BDF5A8` slot 71 |
| `0x00506680` | `sageAudioManagerOnReleaseVolumeModifierLock` | `00BDF5A8` slot 72 |
| `0x00506C20` | `initCategoryBitsFour` | vtable `00BDC45C` slot 2 `004D9E20` 在 `004D9E2F` 调用。该 slot 不是类名 getter |
| `0x00506C90` | `initCategoryBitsTwo` | `00508510` 在 `00508594` 调用 |
| `0x00507720` | `hasActiveRenderCursor` | SageAudioManager.cpp `0051D3C0` 在 `0051D6BB` 调用 |
| `0x00508640` | `appendPlayingAudioNode` | `005181E0` 在 `005182DD` 调用；`005181E0` 由 `00519740` 在 `005198C8` 调用；`00519740` 经 `00519C50` 归到 `00BDF594` slot 3 `0051A1C0`（同上） |
| `0x0050B090` | `sagePathMusicManagerTransferState` | `00BDF3A4` slot 3 |
| `0x0050DF80` | `eraseListenerRecord` | `0050E510` 在 `0050E517` 尾调用 |
| `0x0050E510` | `sageAudioManagerRemoveListenerEntry` | `00BDF5A8` slot 92 |
| `0x0050FA80` | `applyListedVolumeModifiers` | `0050FD50` 在 `0050FD6D` 调用 |
| `0x0050FD50` | `refreshRenderingPlayerVolumes` | RenderingSound.cpp `00515B70` 在 `00515DB6` 调用 |

主控复核：`00519C50` 只压入 EASTL `allocator.h` 断言串，通用头文件不能证明归属，所以上面两行改为从 SageAudioManager vtable `00BDF594` slot 3 起算；SageAudioManager.cpp（`00BDF4D8`）与 RenderingSound.cpp（`00BDF450`）的压栈分别在 `0051E857`、`005162BF`、`0051D51C` 与 `00515DD7`。以上均经 `work/probe/verify_ownership.py` 复算，`fails=0`。

`00504DF0` 在锁 `00DEE1C8` 大于 0 时调用 `00559D60`（`mov eax, [00DF03F8]; ret`）取得设置对象，再按 `+0xD0` 计数和 `+0xD4` 行指针做乘法。音量函数 `00504C80` 用同一次调用。`00502270` 在模式为 -1 时只改 AL，高 24 位仍是 `00DF6098` 里的系统指针。`005044F0` 先读参数指向的 dword，再写入句柄。

## Hooks

无。

## Oracle 与突变

Release `audio50 oracle mismatches=0 cases=118`。每个计数函数至少 3 个用例。

语义突变：把 `00504AC0` 的位下标掩码从 `0x1F` 改成 `0x0F`。Oracle 报 `0x00504AC0` 返回值 game=`0x00000001`、cpp=`0x00000000`。改回 `0x1F` 后再次 `mismatches=0 cases=118`。

## 未计数

窗口内、有所有权边、但调用了窗口外函数，只做锚点：

- `0x00506710`：`00BDF3A4` slot 1。先调用 `0x004A62D0`，再在 `0050674E` 调用 `00502540`。
- `0x00508510`：RenderingSound.cpp `00515B70` 在 `00515E28` 调用。入口先调用 `0x0055A0D0`，随后才调用 `00506C90` 和 `00504AF0`。
- `0x00515050`、`0x00519740`、`0x005181E0`、`0x0051EFF0` 在 `0x00510000` 以上，只作调用者。

`30643c3` 草稿里其余窗口内入口没有 vtable 槽或锚点调用边，仍为 clean unanalyzed，未计数：`0x00502F00`、`0x00503000`、`0x00504590`、`0x00504B20`、`0x00504F20`、`0x00504F80`、`0x00505150`、`0x00506CD0`、`0x00508330`、`0x005084B0`、`0x00508AF0`、`0x00508D50`、`0x0050A220`、`0x0050AD80`、`0x0050BE70`、`0x0050DE20`、`0x0050E040`。

`00502540` 在两个槽都非空且名字非空时调用 `0x00B76D80`；`00506430` 在抑制位成立后调用 `0x0055C680`；`0050B090` 在虚调用把局部字节降到 1 以下时调用 `0x00509710`；`0050FA80` 在向量容量不足时调用 `0x007F17C0`。这些路径不在 oracle 覆盖里，C++ 遇到时直接退出。
