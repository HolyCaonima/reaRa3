#include "ra3/net/CommandMessage.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> decode(const std::string& text) {
    if (text == ".") return {};
    if ((text.size() & 1U) != 0) throw std::runtime_error("odd hex");
    std::vector<std::byte> result(text.size() / 2);
    const auto nibble = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return static_cast<unsigned>(value - '0');
        if (value >= 'a' && value <= 'f') return static_cast<unsigned>(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return static_cast<unsigned>(value - 'A' + 10);
        throw std::runtime_error("hex");
    };
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            (nibble(text[index * 2]) << 4) | nibble(text[index * 2 + 1]));
    }
    return result;
}

std::string encode(const std::vector<std::byte>& data) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0');
    for (const auto value : data) {
        out << std::setw(2) << std::to_integer<unsigned>(value);
    }
    return out.str();
}

std::string hexBytes(const char* text) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0');
    if (text != nullptr) {
        for (const auto* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor) {
            out << std::setw(2) << static_cast<unsigned>(*cursor);
        }
    }
    return out.str();
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

std::uint32_t number(const std::string& value) { return std::stoul(value, nullptr, 16); }

class RecordingText final : public ra3::net::CommandText {
public:
    std::string id;
    RecordingText* other = nullptr;
    bool split = false;
    std::vector<std::string>* trace = nullptr;

    CommandText* appendText(const char* text) override {
        record("t", hexBytes(text));
        if (split && other != nullptr) return other;
        return this;
    }
    CommandText* appendInt(std::int32_t value) override {
        record("i", hex32(static_cast<std::uint32_t>(value)));
        return this;
    }
    CommandText* appendByte24(std::uint32_t value) override {
        record("y", hex32(value));
        return this;
    }
    CommandText* appendByte34(std::uint32_t value) override {
        record("b", hex32(value));
        return this;
    }
    CommandText* appendWord(std::uint32_t value) override {
        record("w", hex32(value));
        return this;
    }
    CommandText* appendReal(std::uint32_t bits) override {
        record("r", hex32(bits));
        return this;
    }

private:
    void record(const char* kind, const std::string& payload) {
        trace->push_back(id + ":" + kind + ":" + payload);
    }
};

class RecordingXfer final : public ra3::net::FieldXfer {
public:
    char id = 'A';
    RecordingXfer* product6C = nullptr;
    RecordingXfer* product7C = nullptr;
    const std::byte* base = nullptr;
    std::size_t extent = 0;
    std::vector<std::string>* trace = nullptr;

    FieldXfer* slot6C(void* field) override { return touch(field, "6c", product6C); }
    FieldXfer* slot70(void* field) override { return touch(field, "70", this); }
    FieldXfer* slot74(void* field) override { return touch(field, "74", this); }
    FieldXfer* slot7C(void* field) override { return touch(field, "7c", product7C); }
    FieldXfer* slot60(void* field) override { return touch(field, "60", this); }
    FieldXfer* slot58(void* field) override { return touch(field, "58", this); }
    FieldXfer* slot54(void* field) override { return touch(field, "54", this); }
    FieldXfer* slot84(void* field) override { return touch(field, "84", this); }

private:
    FieldXfer* touch(void* field, const char* slot, RecordingXfer* returned) {
        auto* bytes = static_cast<std::uint8_t*>(field);
        const auto* begin = reinterpret_cast<const std::uint8_t*>(base);
        std::string where = "S";
        if (bytes >= begin && bytes < begin + extent) {
            where = hex32(static_cast<std::uint32_t>(bytes - begin));
        }
        trace->push_back(std::string(1, id) + ":x:" + slot + ":" + where);
        bytes[0] = static_cast<std::uint8_t>(bytes[0] ^ 0x01U);
        return returned == nullptr ? this : returned;
    }
};

struct CaptionBlock {
    std::uint32_t unused0;
    std::uint32_t unused1;
    const char* text;
};

struct UiNode {
    void** vtable;
    CaptionBlock* caption;
    std::vector<std::string>* trace;
};

void __fastcall uiDescribe(UiNode* self, void*, ra3::net::CommandText*) {
    self->trace->push_back("U:u:");
}

struct ForwardNode {
    void** vtable;
    void* unused;
    std::vector<std::string>* trace;
};

std::uint32_t __fastcall uiForward(ForwardNode* self, void*, std::uint32_t player) {
    self->trace->push_back("N:n:" + hex32(player));
    return 0x4B;
}

std::string join(const std::vector<std::string>& parts) {
    std::ostringstream out;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) out << ',';
        out << parts[index];
    }
    return out.str();
}

const char* textId(const ra3::net::CommandText* value, const RecordingText& primary, const RecordingText& secondary) {
    if (value == &primary) return "T";
    if (value == &secondary) return "T2";
    return "?";
}

const char* xferId(const ra3::net::FieldXfer* value, const RecordingXfer& a, const RecordingXfer& b, const RecordingXfer& c) {
    if (value == &a) return "A";
    if (value == &b) return "B";
    if (value == &c) return "C";
    return "?";
}

void run(const std::vector<std::string>& tokens) {
    auto data = decode(tokens.at(1));
    if (data.size() < ra3::net::CommandMessage::minimumRecoveredSize) {
        throw std::runtime_error("object too small");
    }
    const auto address = number(tokens.at(0));
    const auto flags = tokens.size() > 2 ? number(tokens[2]) : 0U;
    const std::string extra = tokens.size() > 3 ? tokens[3] : "-";
    std::vector<std::string> trace;
    ra3::net::CommandMessage message(data.data());
    std::string ret = "?";

    const bool xfer = address == 0x005D9E40 || address == 0x005D9F00 || address == 0x005DA030 ||
        address == 0x005DA160 || address == 0x005DA520 || address == 0x005DA5A0 ||
        address == 0x005DA740 || address == 0x005DA810 || address == 0x005DA950 ||
        address == 0x005DAA50 || address == 0x005DAED0;
    const bool printer = !xfer && address != 0x005DB0B0;

    if (printer) {
        RecordingText primary;
        RecordingText secondary;
        primary.id = "T";
        secondary.id = "T2";
        primary.trace = &trace;
        secondary.trace = &trace;
        primary.split = (flags & 1U) != 0;
        primary.other = &secondary;
        std::string caption;
        CaptionBlock block{};
        UiNode node{};
        void* slots[4] = {};
        if (address == 0x005DB000 && extra != "-") {
            const auto bytes = decode(extra);
            caption.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            block.text = caption.c_str();
            slots[3] = reinterpret_cast<void*>(&uiDescribe);
            node.vtable = slots;
            node.caption = &block;
            node.trace = &trace;
            void* pointer = &node;
            std::memcpy(data.data() + 0x14, &pointer, sizeof(pointer));
        } else if (address == 0x005DB000) {
            void* pointer = nullptr;
            std::memcpy(data.data() + 0x14, &pointer, sizeof(pointer));
        }
        ra3::net::CommandText* returned = nullptr;
        switch (address) {
        case 0x005D67E0: returned = message.appendFrame(primary); break;
        case 0x005D9DF0: returned = message.appendAuthServerNewGameAccepted(primary); break;
        case 0x005D9EB0: returned = message.appendAuthServerNewGameRejected(primary); break;
        case 0x005D9F80: returned = message.appendCamera(primary); break;
        case 0x005DA100: returned = message.appendPlayerJoin(primary); break;
        case 0x005DA200: returned = message.appendPlayerLeavesServer(primary); break;
        case 0x005DA2E0: returned = message.appendFileData(primary); break;
        case 0x005DA430: returned = message.appendGameCommandDone(primary); break;
        case 0x005DA550: returned = message.appendGameClientRequestNewGame(primary); break;
        case 0x005DA660: returned = message.appendGrantLeave(primary); break;
        case 0x005DA6E0: returned = message.appendIgnoreVoice(primary); break;
        case 0x005DA790: returned = message.appendGameCommandDoneOld(primary); break;
        case 0x005DA8F0: returned = message.appendCreateTime(primary); break;
        case 0x005DA970: returned = message.appendPingPong(primary); break;
        case 0x005DA9F0: returned = message.appendProgress(primary); break;
        case 0x005DAAA0: returned = message.appendRequestLeave(primary); break;
        case 0x005DAB30: returned = message.appendServerJoinAccept(primary); break;
        case 0x005DABC0: returned = message.appendServerJoinReject(primary); break;
        case 0x005DAC50: returned = message.appendServerKick(primary); break;
        case 0x005DACE0: returned = message.appendServerStartupComplete(primary); break;
        case 0x005DAD70: returned = message.appendServerStartupFailed(primary); break;
        case 0x005DAE00: returned = message.appendVoteDisconnectSuggested(primary); break;
        case 0x005DAF30: returned = message.appendTelestrator(primary); break;
        case 0x005DB000: returned = message.appendUi(primary); break;
        case 0x005DB190: returned = message.appendVoice(primary); break;
        case 0x005DB270: returned = message.appendVoteDisconnectSlot(primary); break;
        default: throw std::runtime_error("unknown printer");
        }
        ret = textId(returned, primary, secondary);
    } else if (xfer) {
        RecordingXfer a;
        RecordingXfer b;
        RecordingXfer c;
        a.id = 'A';
        b.id = 'B';
        c.id = 'C';
        a.base = b.base = c.base = data.data();
        a.extent = b.extent = c.extent = data.size();
        a.trace = b.trace = c.trace = &trace;
        a.product6C = b.product6C = c.product6C = &b;
        a.product7C = b.product7C = c.product7C = &c;
        ra3::net::FieldXfer* returned = nullptr;
        switch (address) {
        case 0x005D9E40: returned = message.xferPair(a); break;
        case 0x005D9F00: {
            const auto word = message.xferDwordAndWord(a);
            if (word == 0) ret = "0";
            else {
                std::ostringstream out;
                out << '?' << std::hex << std::nouppercase << word;
                ret = out.str();
            }
            std::cout << ret << ' ' << encode(data) << ' ' << join(trace) << std::endl;
            return;
        }
        case 0x005DA030: returned = message.xferCameraPayload(a); break;
        case 0x005DA160: returned = message.xferPlayerJoinPayload(a); break;
        case 0x005DA520: returned = message.xferTimedPair(a); break;
        case 0x005DA5A0: returned = message.xferTwelveDwords(a); break;
        case 0x005DA740: returned = message.xferIgnoreVoicePayload(a); break;
        case 0x005DA810: returned = message.xferDoneOldPayload(a); break;
        case 0x005DA950: returned = message.xferField14Slot6C(a); break;
        case 0x005DAA50: returned = message.xferField14Slot7C(a); break;
        case 0x005DAED0: returned = message.xferVotePayload(a); break;
        default: throw std::runtime_error("unknown xfer");
        }
        ret = xferId(returned, a, b, c);
    } else if (address == 0x005DB0B0) {
        if (extra == "-") {
            void* pointer = nullptr;
            std::memcpy(data.data() + 0x14, &pointer, sizeof(pointer));
            ret = message.forwardNested() == 0 ? "0" : "?";
        } else {
            ForwardNode node{};
            void* slots[4] = {};
            slots[2] = reinterpret_cast<void*>(&uiForward);
            node.vtable = slots;
            node.trace = &trace;
            void* pointer = &node;
            std::memcpy(data.data() + 0x14, &pointer, sizeof(pointer));
            ret = message.forwardNested() == 0x4B ? "K" : "?";
        }
    } else {
        throw std::runtime_error("unknown address");
    }
    std::cout << ret << ' ' << encode(data) << ' ' << join(trace) << std::endl;
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") return 0;
        try {
            std::istringstream input(line);
            std::vector<std::string> tokens;
            for (std::string token; input >> token;) tokens.push_back(std::move(token));
            if (tokens.empty()) continue;
            run(tokens);
        } catch (const std::exception&) {
            std::cout << "error" << std::endl;
        }
    }
    return 0;
}
