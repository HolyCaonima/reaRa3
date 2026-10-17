#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::net {

// Text sink used by the command-message debug slots.  The shipped calls are
// virtual and return the next receiver:
//   +0x3C appendText, +0x30 appendInt, +0x24 appendByte24, +0x34 appendByte34,
//   +0x14 appendWord, +0x20 appendReal (float bits, not a formatted numeral).
// A representative xfer body at 0x004D38E0 returns its own this; these text
// slots are consumed the same way, so each method returns the receiver the
// caller actually uses.
class CommandText {
public:
    virtual ~CommandText() = default;
    virtual CommandText* appendText(const char* text) = 0;
    virtual CommandText* appendInt(std::int32_t value) = 0;
    virtual CommandText* appendByte24(std::uint32_t value) = 0;
    virtual CommandText* appendByte34(std::uint32_t value) = 0;
    virtual CommandText* appendWord(std::uint32_t value) = 0;
    virtual CommandText* appendReal(std::uint32_t bits) = 0;
};

// Field transfer used by the command-message xfer slots.  Slot names are the
// byte offsets of the stream vtable.  0x004D38E0 (slot +0x6C, tag "uint",
// size 4) and 0x004D3820 (slot +0x7C, tag "byte", size 1) return this after
// transferring the pointed-to field.  The command bodies follow that return
// value when the instruction stream does, and reload the original stream
// when the instruction stream does.
class FieldXfer {
public:
    virtual ~FieldXfer() = default;
    virtual FieldXfer* slot6C(void* field) = 0;
    virtual FieldXfer* slot70(void* field) = 0;
    virtual FieldXfer* slot74(void* field) = 0;
    virtual FieldXfer* slot7C(void* field) = 0;
    virtual FieldXfer* slot60(void* field) = 0;
    virtual FieldXfer* slot58(void* field) = 0;
    virtual FieldXfer* slot54(void* field) = 0;
    virtual FieldXfer* slot84(void* field) = 0;
};

// Non-owning view of one command message.  The original class names are not
// in the image.  The cluster is the set of vtables whose debug slot formats
// "<frame=%d, player=%d, type=%d>" from dwords +0x04, +0x08 and +0x0C and
// then a per-type literal (AUTHSERVER_NEW_GAME_ACCEPTED, PLAYER_LEAVES_SERVER,
// VOTE_DISCONNECT, and the others listed on the methods below).
//
// Generals GameNetwork/NetCommandMsg.h was read only as architecture context
// for a frame/player/type command message.  RA3 offsets and literals are the
// ones named here; the GPL field layout was not copied.
//
// minimumRecoveredSize covers the highest byte these methods read or write
// (the twelve-dword transfer ending at +0x48).  A null this is outside the
// contract: every body dereferences ecx.
class CommandMessage {
public:
    explicit CommandMessage(void* storage) noexcept;
    static constexpr std::size_t minimumRecoveredSize = 0x48;

    CommandText* appendFrame(CommandText& text) const;  // 0x005D67E0 vtable 00BEE53C slot 2
    CommandText* appendAuthServerNewGameAccepted(CommandText& text) const;  // 0x005D9DF0 00BEE924 slot 14
    CommandText* appendAuthServerNewGameRejected(CommandText& text) const;  // 0x005D9EB0 00BEE988 slot 2
    CommandText* appendCamera(CommandText& text) const;  // 0x005D9F80 00BEE9BC slot 2
    CommandText* appendPlayerJoin(CommandText& text) const;  // 0x005DA100 00BEE9F0 slot 2
    CommandText* appendPlayerLeavesServer(CommandText& text) const;  // 0x005DA200 00BEEA20 slot 2
    CommandText* appendFileData(CommandText& text) const;  // 0x005DA2E0 00BEEA4C slot 2
    CommandText* appendGameCommandDone(CommandText& text) const;  // 0x005DA430 00BEEA74 slot 2
    CommandText* appendGameClientRequestNewGame(CommandText& text) const;  // 0x005DA550 00BEEAA4 slot 7
    CommandText* appendGrantLeave(CommandText& text) const;  // 0x005DA660 00BEEAE4 slot 3
    CommandText* appendIgnoreVoice(CommandText& text) const;  // 0x005DA6E0 00BEEB0C slot 2
    CommandText* appendGameCommandDoneOld(CommandText& text) const;  // 0x005DA790 00BEEB30 slot 2
    CommandText* appendCreateTime(CommandText& text) const;  // 0x005DA8F0 00BEF4D4 slot 7
    CommandText* appendPingPong(CommandText& text) const;  // 0x005DA970 00BEF4D4 slot 12
    CommandText* appendProgress(CommandText& text) const;  // 0x005DA9F0 00BEEBA0 slot 2
    CommandText* appendRequestLeave(CommandText& text) const;  // 0x005DAAA0 00BEEBC0 slot 2
    CommandText* appendServerJoinAccept(CommandText& text) const;  // 0x005DAB30 00BEEBF0 slot 2
    CommandText* appendServerJoinReject(CommandText& text) const;  // 0x005DABC0 00BEEC18 slot 2
    CommandText* appendServerKick(CommandText& text) const;  // 0x005DAC50 00BEEC40 slot 2
    CommandText* appendServerStartupComplete(CommandText& text) const;  // 0x005DACE0 00BEEC5C slot 3
    CommandText* appendServerStartupFailed(CommandText& text) const;  // 0x005DAD70 00BEEC8C slot 2
    CommandText* appendVoteDisconnectSuggested(CommandText& text) const;  // 0x005DAE00 00BEECB8 slot 2
    CommandText* appendTelestrator(CommandText& text) const;  // 0x005DAF30 00BEED00 slot 2
    CommandText* appendUi(CommandText& text) const;  // 0x005DB000 00BEED2C slot 2
    CommandText* appendVoice(CommandText& text) const;  // 0x005DB190 00BEED50 slot 2
    CommandText* appendVoteDisconnectSlot(CommandText& text) const;  // 0x005DB270 00BEED84 slot 2

    // 0x005D9E40 is slot 16 of 00BEE924 and slot 14 of 00BEF4D4.
    FieldXfer* xferPair(FieldXfer& xfer);
    // EAX is the zero-extended word stored back at +0x18, not the stream.
    std::uint32_t xferDwordAndWord(FieldXfer& xfer);  // 0x005D9F00 00BEE988 slot 4
    FieldXfer* xferCameraPayload(FieldXfer& xfer);  // 0x005DA030 00BEE9BC slot 4
    FieldXfer* xferPlayerJoinPayload(FieldXfer& xfer);  // 0x005DA160 00BEE9F0 slot 4
    FieldXfer* xferTimedPair(FieldXfer& xfer);  // 0x005DA520 00BEF4D4 slot 4
    FieldXfer* xferTwelveDwords(FieldXfer& xfer);  // 0x005DA5A0 00BEEAA4 slot 9
    FieldXfer* xferIgnoreVoicePayload(FieldXfer& xfer);  // 0x005DA740 00BEEB0C slot 4
    FieldXfer* xferDoneOldPayload(FieldXfer& xfer);  // 0x005DA810 00BEEB30 slot 4
    // 0x005DA950 is slot 4 of 00BEEA74, slot 4 of 00BEEBC0 and slot 9 of 00BEF4D4.
    FieldXfer* xferField14Slot6C(FieldXfer& xfer);
    // 0x005DAA50 is slot 4 of 00BEEBA0 and slot 4 of 00BEED84.
    FieldXfer* xferField14Slot7C(FieldXfer& xfer);
    FieldXfer* xferVotePayload(FieldXfer& xfer);  // 0x005DAED0 00BEECB8 slot 4
    // 0x005DB0B0 is slot 1 of 00BEED2C.  EAX is 0 when +0x14 is null, otherwise
    // the return of that node's vtable slot +0x08 called with dword +0x08.
    std::uint32_t forwardNested() const;

private:
    CommandText* formatFrame(CommandText& text) const;
    CommandText* appendNamed(CommandText& text, const char* name) const;
    CommandText* appendNamedInt(CommandText& text, const char* name, std::size_t offset) const;
    std::int32_t loadI32(std::size_t offset) const;
    std::uint32_t loadU32(std::size_t offset) const;
    std::uint16_t loadU16(std::size_t offset) const;
    std::uint8_t loadU8(std::size_t offset) const;
    void* loadPtr(std::size_t offset) const;
    std::uint32_t loadBits(std::size_t offset) const;
    void* field(std::size_t offset);
    void storeU32(std::size_t offset, std::uint32_t value);

    std::byte* data_;
};

}  // namespace ra3::net
