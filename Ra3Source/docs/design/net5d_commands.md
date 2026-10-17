# Command-message cluster (`0x005D0000–0x005DFFFF`)

## Observed

The accepted bodies are slots of the small vtables from `0x00BEE53C` through
`0x00BEED84`, plus `0x00BEF4D4`.  Each debug slot formats

`<frame=%d, player=%d, type=%d>`

from the dwords at `+0x04`, `+0x08` and `+0x0C` (`0x0040EB30` is the
`sprintf` thunk) and appends it through text vtable `+0x3C`.  A second
literal names the message: `AUTHSERVER_NEW_GAME_ACCEPTED`,
`PLAYER_LEAVES_SERVER`, `VOTE_DISCONNECT`, `TELESTRATOR`, `VOICE`, and the
others in `CommandMessage.cpp`.  That literal is the ownership proof that
the slot belongs to this family, together with the vtable index recorded
in `net5d_annotations.tsv`.

Sibling slots of those same vtables transfer fields through a stream
object.  `0x004D38E0` (a `+0x6C` implementation) writes the tag `uint` and
four bytes, then returns `this`.  `0x004D3820` (`+0x7C`) writes the tag
`byte` and one byte, then returns `this`.  The command bodies follow that
return when the instruction uses `eax`, and reload the original stream
when they read `[esi]` or `[edi]` again.

`0x005D9F80` also calls `0x005AAC60` and `0x005D81F0`.  Neither address is a
ledger function start.  Their effect is inlined: three floats as
`(x, y, z)`, and four floats as `<quat a/b/c/d>`, each number passed as raw
bits to text slot `+0x20`.

`minimumRecoveredSize` is `0x48`, the first byte past the twelve-dword
transfer in `0x005DA5A0`.

## Inferred

The strings and the shared frame/player/type prefix put the vtables in one
command-message family.  The view is `ra3::net::CommandMessage`.  Generals
`GameNetwork/NetCommandMsg.h` was read as architecture context only.  Its
field offsets were not copied, and method names follow the RA3 literals
and the slot offsets rather than the GPL class names.

## Contract

A null `this` is outside the contract.  Debug slots take the text object on
the stack (`ret 4`) and do not write the command.  Transfer slots take the
stream on the stack (`ret 4`).  `0x005D9F00` reuses that argument slot as a
word temporary and stores the word back as a zero-extended dword, clearing
the high half of `+0x18`.  EAX is that word (`movzx`), not the stream
pointer.  `0x005DB0B0` takes no stack argument.  EAX is 0
when `+0x14` is null; otherwise it is the return of the nested vtable slot
`+0x08`.

No entry is hooked.  The bodies call text or stream virtuals, or a nested
virtual, so a shadow hook would invoke those callees twice.

## Deferred

These addresses were inspected and are not counted:

- `0x005D4D90`, `0x005D4DD0`, `0x005DAE80`, `0x005DB2D0`: read the process
  global at `0x00DF6B38` and forward into it.
- `0x005D4DB0`: calls `0x005BC790`, which is not a ledger function and is
  outside this vtable family.
- `0x005D8D60`: stores its argument at global `0x00DF6C2C`.
- `0x005DA350`: calls allocators `0x004166B0` and `0x004165A0`.
- `0x005DA4C0`: tail jump to `0x005B03C0` with no body of its own.
- `0x005DA0C0` and `0x005DA400`: slots of `0x00BEF488` and `0x00BEF4A4`.
  Those vtables have no frame-format printer, so the bodies are not members
  of this class.  `0x005DA400` also calls `0x0054EDA0`.
- ReliableTransport facades `0x005D6C40`, `0x005D6CD0`, `0x005D6D60`,
  `0x005DD1E0`, `0x005DD2F0`, `0x005DD3D0`, `0x005DD4C0`, `0x005DD5B0`,
  `0x005DD670`, `0x005DD6F0`, `0x005DD8A0`, `0x005DD9B0`: source-anchored
  assert/dispatch into an implementation type, including the logger
  `0x0040EBF0`.  They are not field accessors of this message family.
- `0x005D3F40` (PeerThread) and `0x005D7A00` (TickerDataHandler): different
  source files, and `0x005D3F40` is an assert that the thread entry is dead.

`0x005DA950` is one body shared by three vtable slots.  `0x005DAA50` and
`0x005D9E40` are each one body shared by two slots.  Each body is counted
once.
