# Lobby session field leaves

`ra3::net::LobbySession` is a non-owning view of one object. The shipped type
name is unknown. The module name is an inference from the strings below.

## Observed

- `0x006225A0` stores primary vptr `0x00BF1558` at offset 0 and secondary vptr
  `0x00BF1548` at offset 4. It zeroes the dwords at `+0x18` and `+0x1C`.
- `0x00622B30`, called by slot 0 `0x00627820`, stores the same two vptrs and
  writes the dwords at `+0x1EA0`, `+0x1EA4`, `+0x1EA8`, `+0x1EAC` and `+0x1EB0`.
- `0x0062A636` allocates `0x1ED8` bytes, calls `0x006225A0`, and stores the
  pointer at global `0x00DF6AF4`. `minimumRecoveredSize` is that allocation.
- The 46 accepted addresses are slots of the 141-entry table at `0x00BF1558`.
  Each body is one of: `mov al`, `mov eax`, `lea eax`, a matching store from
  `[esp+4]`, or `cmp dword [ecx+0xE8], 0; setne`.
- Byte loads and byte stores touch only AL. A byte load leaves EAX[31:8] as
  the caller left it. The C++ methods return the byte.

Slot 4 is `0x00627840`. Its body references the literals `quickmatch`,
`ChatRoom1`, `LobbyCustomMap` and `GUI:%s`. That method is not reconstructed.

## Inferred

The strings and the singleton global put the object in the lobby/session
family, so the view lives in `ra3::net` under the name `LobbySession`.
Field methods are named by offset because the field meanings are not known.

Generals `Code/GameEngine/Include/GameNetwork/GameInfo.h` and
`GameSpy/StagingRoomGameInfo.h` were read as architecture context. Their
field offsets are not the RA3 offsets and were not copied.

## Unknown

- The original class name. This is not `GameSpyStagingRoom` (`0x00BEFFE0`).
- The meaning of every field.
- Whether the object continues past the last byte written by these leaves.
  The allocator size is the bound used here.

## Contract

A null `this` is outside the contract: every body dereferences `ecx`.
Setters take the value on the stack, so the value cannot alias the object.
Getters do not write. A dword store writes four bytes and a byte store writes
one. No entry is hooked.

## Deferred

These slots of the same vtable are not counted:

- `0x008E4610`, the setter of `+0x104`. `recon/render8e` already claims it.
  The local getter `0x006228C0` remains.
- Folded accessors outside this compilation neighborhood: `0x0073CD80`,
  `0x00560A10`, `0x00B11280`, `0x00A26140`, `0x00B64F30`, `0x00B7A4B0`,
  `0x00A260A0`, `0x00A26080`.
- Constructors, the slot-0 destructor, allocator calls, string thunks, and
  every larger slot, including `0x00627840` and `0x00622B30`.
