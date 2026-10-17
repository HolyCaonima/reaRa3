#include "ra3/net/CommandMessage.h"

#include <cstdio>
#include <cstring>

namespace ra3::net {
namespace {

constexpr char kFrameFormat[] = "<frame=%d, player=%d, type=%d>";

// 0x005AAC60 is not a ledger function (it sits in the gap after 0x005AABE0).
// 0x005D9F80 calls it with the text receiver and a pointer to three floats.
// The body appends "(x, y, z)" and returns the text it was given.
void appendPosition(CommandText* text, const std::byte* xyz) {
    auto load = [](const std::byte* address) {
        std::uint32_t bits;
        std::memcpy(&bits, address, sizeof(bits));
        return bits;
    };
    auto* current = text->appendText("(");
    current = current->appendReal(load(xyz));
    current = current->appendText(", ");
    current = current->appendReal(load(xyz + 4));
    current = current->appendText(", ");
    current = current->appendReal(load(xyz + 8));
    current->appendText(")");
}

// 0x005D81F0 is not a ledger function (the gap between 0x005D81E0 and
// 0x005D8290).  0x005D9F80 calls it with four floats at the rotation field.
// The body appends "<quat a/b/c/d>" and returns the text it was given.
void appendQuat(CommandText* text, const std::byte* quat) {
    auto load = [](const std::byte* address) {
        std::uint32_t bits;
        std::memcpy(&bits, address, sizeof(bits));
        return bits;
    };
    auto* current = text->appendText("<quat ");
    current = current->appendReal(load(quat));
    current = current->appendText("/");
    current = current->appendReal(load(quat + 4));
    current = current->appendText("/");
    current = current->appendReal(load(quat + 8));
    current = current->appendText("/");
    current = current->appendReal(load(quat + 12));
    current->appendText(">");
}

}  // namespace

CommandMessage::CommandMessage(void* storage) noexcept
    : data_(static_cast<std::byte*>(storage)) {}

std::int32_t CommandMessage::loadI32(std::size_t offset) const {
    std::int32_t value;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}

std::uint32_t CommandMessage::loadU32(std::size_t offset) const {
    std::uint32_t value;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}

std::uint16_t CommandMessage::loadU16(std::size_t offset) const {
    std::uint16_t value;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}

std::uint8_t CommandMessage::loadU8(std::size_t offset) const {
    return static_cast<std::uint8_t>(data_[offset]);
}

void* CommandMessage::loadPtr(std::size_t offset) const {
    void* value = nullptr;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}

std::uint32_t CommandMessage::loadBits(std::size_t offset) const {
    return loadU32(offset);
}

void* CommandMessage::field(std::size_t offset) { return data_ + offset; }

void CommandMessage::storeU32(std::size_t offset, std::uint32_t value) {
    std::memcpy(data_ + offset, &value, sizeof(value));
}

CommandText* CommandMessage::formatFrame(CommandText& text) const {
    char buffer[0x200];
    std::snprintf(buffer, sizeof(buffer), kFrameFormat, loadI32(0x04), loadI32(0x08), loadI32(0x0c));
    return text.appendText(buffer);
}

CommandText* CommandMessage::appendNamed(CommandText& text, const char* name) const {
    formatFrame(text);
    return text.appendText(name);
}

CommandText* CommandMessage::appendNamedInt(
    CommandText& text, const char* name, std::size_t offset) const {
    formatFrame(text);
    return text.appendText(name)->appendInt(loadI32(offset));
}

CommandText* CommandMessage::appendFrame(CommandText& text) const {
    return formatFrame(text);
}

CommandText* CommandMessage::appendAuthServerNewGameAccepted(CommandText& text) const {
    return appendNamed(text, "AUTHSERVER_NEW_GAME_ACCEPTED");
}

CommandText* CommandMessage::appendAuthServerNewGameRejected(CommandText& text) const {
    return appendNamed(text, "AUTHSERVER_NEW_GAME_REJECTED");
}

CommandText* CommandMessage::appendCamera(CommandText& text) const {
    formatFrame(text);
    CommandText* result = text.appendText(" CAMERA frame=")->appendInt(loadI32(0x14));
    const auto flags = loadU8(0x18);
    if ((flags & 1U) != 0) {
        CommandText* position = text.appendText(" pos=");
        appendPosition(position, data_ + 0x1c);
        result = position;
    }
    if ((flags & 2U) != 0) {
        CommandText* rotation = text.appendText(" rot=");
        appendQuat(rotation, data_ + 0x28);
        result = rotation;
    }
    return result;
}

CommandText* CommandMessage::appendPlayerJoin(CommandText& text) const {
    return appendNamedInt(text, "PlayerJoinMsg for gameID ", 0x14);
}

CommandText* CommandMessage::appendPlayerLeavesServer(CommandText& text) const {
    return appendNamed(text, "PLAYER_LEAVES_SERVER");
}

CommandText* CommandMessage::appendFileData(CommandText& text) const {
    formatFrame(text);
    CommandText* current = text.appendText(" FILE_DATA ")->appendInt(loadI32(0x18));
    return current->appendText(" bytes");
}

CommandText* CommandMessage::appendGameCommandDone(CommandText& text) const {
    return appendNamedInt(text, " GAMECOMMANDDONE for frame ", 0x14);
}

CommandText* CommandMessage::appendGameClientRequestNewGame(CommandText& text) const {
    return appendNamed(text, "GAMECLIENT_REQUEST_NEW_GAME");
}

CommandText* CommandMessage::appendGrantLeave(CommandText& text) const {
    return appendNamed(text, " GRANT_LEAVE");
}

CommandText* CommandMessage::appendIgnoreVoice(CommandText& text) const {
    formatFrame(text);
    return text.appendText(" IGNORE_VOICE ")->appendByte24(loadU8(0x14));
}

CommandText* CommandMessage::appendGameCommandDoneOld(CommandText& text) const {
    formatFrame(text);
    text.appendText(" GAMECOMMANDDONE for frame ")->appendInt(loadI32(0x14));
    return text.appendText(", old data for slot ")->appendByte34(loadU8(0x18));
}

CommandText* CommandMessage::appendCreateTime(CommandText& text) const {
    return appendNamedInt(text, " create time = ", 0x14);
}

CommandText* CommandMessage::appendPingPong(CommandText& text) const {
    formatFrame(text);
    CommandText* current = text.appendText(" ping time = ")->appendInt(loadI32(0x18));
    current = current->appendText(" pong time = ");
    return current->appendInt(loadI32(0x14));
}

CommandText* CommandMessage::appendProgress(CommandText& text) const {
    formatFrame(text);
    return text.appendText(" PROGRESS ")->appendByte34(loadU8(0x14));
}

CommandText* CommandMessage::appendRequestLeave(CommandText& text) const {
    return appendNamedInt(text, " REQUEST_LEAVE on frame ", 0x14);
}

CommandText* CommandMessage::appendServerJoinAccept(CommandText& text) const {
    return appendNamed(text, "SERVER_JOIN_ACCEPT");
}

CommandText* CommandMessage::appendServerJoinReject(CommandText& text) const {
    return appendNamed(text, "SERVER_JOIN_REJECT");
}

CommandText* CommandMessage::appendServerKick(CommandText& text) const {
    return appendNamed(text, "SERVER_KICK");
}

CommandText* CommandMessage::appendServerStartupComplete(CommandText& text) const {
    return appendNamed(text, "SERVER_STARTUPCOMPLETE");
}

CommandText* CommandMessage::appendServerStartupFailed(CommandText& text) const {
    return appendNamed(text, "SERVER_STARTUPFAILED");
}

CommandText* CommandMessage::appendVoteDisconnectSuggested(CommandText& text) const {
    formatFrame(text);
    CommandText* current = text.appendText(" VOTE_DISCONNECT for slot ")->appendByte34(loadU8(0x14));
    current = current->appendText(", suggested kick frame ");
    return current->appendInt(loadI32(0x18));
}

CommandText* CommandMessage::appendTelestrator(CommandText& text) const {
    formatFrame(text);
    CommandText* current = text.appendText(" TELESTRATOR, frame ")->appendInt(loadI32(0x14));
    current = current->appendText(", ")->appendByte34(loadU8(0x18));
    return current->appendText(" bytes");
}

CommandText* CommandMessage::appendUi(CommandText& text) const {
    formatFrame(text);
    text.appendText(" UI ");
    auto* node = static_cast<std::byte*>(loadPtr(0x14));
    if (node == nullptr) return text.appendText("<Empty>");
    auto* block = *reinterpret_cast<std::byte**>(node + 4);
    auto* caption = *reinterpret_cast<const char**>(block + 8);
    text.appendText(caption)->appendText("(");
    auto* vtable = *reinterpret_cast<void***>(node);
    using Describe = void(__thiscall*)(void*, CommandText*);
    reinterpret_cast<Describe>(vtable[3])(node, &text);
    return text.appendText(")");
}

CommandText* CommandMessage::appendVoice(CommandText& text) const {
    formatFrame(text);
    CommandText* current = text.appendText(" VOICE, frame ")->appendInt(loadI32(0x14));
    current = current->appendText(", ")->appendWord(loadU16(0x1a));
    current = current->appendText(" bytes, serial ");
    return current->appendWord(loadU16(0x18));
}

CommandText* CommandMessage::appendVoteDisconnectSlot(CommandText& text) const {
    formatFrame(text);
    return text.appendText(" VOTE_DISCONNECT for slot ")->appendByte34(loadU8(0x14));
}

FieldXfer* CommandMessage::xferPair(FieldXfer& xfer) {
    xfer.slot6C(field(0x14));
    return xfer.slot6C(field(0x18));
}

std::uint32_t CommandMessage::xferDwordAndWord(FieldXfer& xfer) {
    xfer.slot6C(field(0x14));
    std::uint32_t temporary = loadU16(0x18);
    xfer.slot74(&temporary);
    const auto word = temporary & 0xFFFFu;
    storeU32(0x18, word);
    return word;
}

FieldXfer* CommandMessage::xferCameraPayload(FieldXfer& xfer) {
    FieldXfer* result = xfer.slot6C(field(0x14))->slot7C(field(0x18));
    const auto flags = loadU8(0x18);
    if ((flags & 1U) != 0) result = xfer.slot58(field(0x1c));
    if ((flags & 2U) != 0) result = xfer.slot54(field(0x28));
    return result;
}

FieldXfer* CommandMessage::xferPlayerJoinPayload(FieldXfer& xfer) {
    xfer.slot6C(field(0x14));
    xfer.slot70(field(0x18));
    xfer.slot6C(field(0x1c));
    auto* cursor = static_cast<std::byte*>(field(0x20));
    FieldXfer* result = nullptr;
    for (int index = 0; index < 6; ++index) {
        result = xfer.slot6C(cursor);
        cursor += 4;
    }
    return result;
}

FieldXfer* CommandMessage::xferTimedPair(FieldXfer& xfer) {
    xfer.slot60(field(0x14));
    return xfer.slot70(field(0x18));
}

FieldXfer* CommandMessage::xferTwelveDwords(FieldXfer& xfer) {
    xfer.slot6C(field(0x14));
    FieldXfer* result = nullptr;
    auto* cursor = static_cast<std::byte*>(field(0x18));
    for (int index = 0; index < 6; ++index) {
        result = xfer.slot6C(cursor);
        cursor += 4;
    }
    for (int index = 0; index < 6; ++index) {
        result = xfer.slot6C(cursor);
        cursor += 4;
    }
    return result;
}

FieldXfer* CommandMessage::xferIgnoreVoicePayload(FieldXfer& xfer) {
    return xfer.slot84(field(0x14));
}

FieldXfer* CommandMessage::xferDoneOldPayload(FieldXfer& xfer) {
    xfer.slot6C(field(0x14));
    return xfer.slot7C(field(0x18));
}

FieldXfer* CommandMessage::xferField14Slot6C(FieldXfer& xfer) {
    return xfer.slot6C(field(0x14));
}

FieldXfer* CommandMessage::xferField14Slot7C(FieldXfer& xfer) {
    return xfer.slot7C(field(0x14));
}

FieldXfer* CommandMessage::xferVotePayload(FieldXfer& xfer) {
    return xfer.slot7C(field(0x14))->slot6C(field(0x18));
}

std::uint32_t CommandMessage::forwardNested() const {
    auto* node = static_cast<std::byte*>(loadPtr(0x14));
    if (node == nullptr) return 0;
    auto* vtable = *reinterpret_cast<void***>(node);
    using Forward = std::uint32_t(__thiscall*)(void*, std::uint32_t);
    return reinterpret_cast<Forward>(vtable[2])(node, loadU32(0x08));
}

}  // namespace ra3::net
