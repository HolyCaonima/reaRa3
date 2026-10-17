# Common subsystem name slots (`0x007E0000`–`0x0080FFFF`)

34 clean functions. Each counted address is a slot of a vtable whose dword at that slot is the function. The slot-2 bodies are `mov eax, imm32; ret`. The immediate is the ASCII name of that record in the image (`Money`, `Energy`, `Player`, `Team`, and the other names below). The remaining eleven functions are other slots of those same tables: a linked-text length, a tunnel counter, three Player nested forwards, and five flag bytes on the `0x00C4DEF8` object.

`work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/PlayerList.h`, `Include/Common/Money.h`, `Include/Common/Energy.h`, `Include/Common/ScoreKeeper.h`, `Include/Common/TunnelTracker.h`, and `Source/Common/RTS/PlayerList.cpp` were read only to recognise that these names are separate records. No Generals offset or body is copied. The RA3 strings and the instruction bytes are the implementation.

`byteB8UnlessBaMutant` inverts the `+0xBA` test of `0x00805760`. It is not a shipped function and is not counted. The oracle requires it to disagree with the shipped predicate.

## Counted functions

| VA | Method | Ownership |
|---|---|---|
| `0x007E0BA0` | `disguiseGuardName` | vtable `00c348c8` slot 2 |
| `0x007E1470` | `engagementTrackerName` | vtable `00c35008` slot 2 |
| `0x007E24B0` | `redAlertName` | vtable `00c34c6c` slot 2 |
| `0x007E25F0` | `scoredKillTrackerName` | vtable `00c350ec` slot 2 |
| `0x007E72B0` | `moneyName` | vtable `00c34ed0` slot 2 |
| `0x007E72C0` | `commandName` | vtable `00c34ee8` slot 2 |
| `0x007E7C00` | `playerListName` | vtable `00c34f84` slot 2 |
| `0x007E9BC0` | `energyName` | vtable `00c34fe8` slot 2 |
| `0x007E9FF0` | `playerRelationMapName` | vtable `00c35028` slot 2 |
| `0x007EAE70` | `playerTechManagerName` | vtable `00c3504c` slot 2 |
| `0x007EB200` | `resourceGatheringManagerName` | vtable `00c350c0` slot 2 |
| `0x007EB480` | `teamRelationMapName` | vtable `00c350fc` slot 2 |
| `0x007EBFC0` | `tunnelTrackerName` | vtable `00c35158` slot 2 |
| `0x007EE9B0` | `playerTeamListName` | vtable `00c351b4` slot 2 |
| `0x007F0F90` | `teamFactoryName` | vtable `00c351dc` slot 2 |
| `0x007F5FA0` | `playerName` | vtable `00c35320` slot 2 |
| `0x007F74A0` | `teamTemplateInfoName` | vtable `00c35380` slot 2 |
| `0x007F7850` | `teamName` | vtable `00c353c0` slot 2 |
| `0x007F9F70` | `teamPrototypeName` | vtable `00c353dc` slot 2 |
| `0x007FC3A0` | `scoreKeeperName` | vtable `00c354f0` slot 2 |
| `0x008028F0` | `logicEvaTrackerName` | vtable `00c4dfc8` slot 49 |
| `0x00802900` | `logicPlayingAudioTrackerName` | vtable `00c4dfc8` slot 67 |
| `0x00802910` | `logicViewTrackerName` | vtable `00c4dcec` slot 3 |
| `0x007E96C0` | `linkedTextLength` | vtable `00c34eb0` slot 1 |
| `0x007E9710` | `linkedTextLengthPlus` | vtable `00c34ec0` slot 1 |
| `0x007E2DD0` | `adjustTunnelCount` | vtable `00c35154` slot 0 |
| `0x007E16F0` | `forwardNested34` | vtable `00c35320` slot 5 |
| `0x007E1710` | `copyNestedTriple` | vtable `00c35320` slot 6 |
| `0x007E1740` | `forwardNested38` | vtable `00c35320` slot 7 |
| `0x00805720` | `availabilityByte` | vtable `00c4def8` slot 21 |
| `0x00805760` | `byteB8UnlessBa` | vtable `00c4def8` slot 22 |
| `0x008052D0` | `storeFlagCf` | vtable `00c4def8` slot 31 |
| `0x00805840` | `byteAtD0` | vtable `00c4def8` slot 39 |
| `0x00805850` | `byteAt40` | vtable `00c4def8` slot 40 |

Name slots replace EAX and do not read ECX. `mov al` bodies keep EAX[31:8]. `0x007E1710` leaves the nested pointer in EAX[31:8] and the byte at nested `+0x50` in AL. A null nested object clears only AL and does not write the destination. The alias case writes the first dword into the nested object before the second dword is read.

`0x007E2DD0` adds `+1` or `-1` at `+0x10` and leaves EAX unchanged (`ret 4`). `0x008052D0` stores the argument low byte at `+0xCF` and returns that byte in AL (`ret 4`). `0x00805720` calls vtable `+0x44` only when byte `+0xBA` is zero and the signed index at `+0x9C` is in `0..0x13`; that path is not hooked.

## Hook descriptors

Registered in `Ra3Source/src/hook/dll/targets/CommonSubsystemTargets.cpp`. All eight are name slots. `expected16` and the stolen length come from `tools/analysis/disasm.py <VA> --hook-only`. The stolen prefix is the five-byte `mov eax, imm32`. Shadow compares the returned dword and returns the original.

| Hook Name | Address | Stolen | `expected16` | ABI / Return | `explicitOnly` | Notes |
|---|---|---|---|---|---|---|
| `subsysdisguise` | `0x007E0BA0` | 5 | `B81049C300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | DisguiseGuardPositionStateMachine |
| `subsysgather` | `0x007EB200` | 5 | `B8D050C300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | ResourceGatheringManager |
| `subsyscommand` | `0x007E72C0` | 5 | `B8F84EC300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | Command |
| `subsysmoney` | `0x007E72B0` | 5 | `B8E04EC300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | Money |
| `subsysenergy` | `0x007E9BC0` | 5 | `B8F84FC300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | Energy |
| `subsysplayer` | `0x007F5FA0` | 5 | `B84053C300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | Player |
| `subsysteamproto` | `0x007F9F70` | 5 | `B8EC53C300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | TeamPrototype |
| `subsysscore` | `0x007FC3A0` | 5 | `B80055C300C3CCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> uint32` | `true` | ScoreKeeper name slot |

All 8 targets are `explicitOnly=true`.

Functions that call `0x004166B0` or a freelist, tail-jump another body, read the global at `0x00DF6B1C`, or return through x87 are not hooked. `0x00805720` and the two nested forwards call a virtual and are reconstructed without a hook.
