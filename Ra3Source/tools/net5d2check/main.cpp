#include "ra3/net/ReliableTransport5D.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

template <typename T>
T load(const std::uint8_t* base, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

template <typename T>
void store(std::uint8_t* base, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(base + offset, &value, sizeof(T));
}

std::uint8_t* gEndpoint = nullptr;

std::uint32_t zeroStub(void*) noexcept { return 0u; }

std::uint32_t endpointStub(void*) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(gEndpoint));
}

void networkStub(void* network) noexcept {
    auto* raw = static_cast<std::uint8_t*>(network);
    store<std::uint32_t>(raw, 4, load<std::uint32_t>(raw, 4) + 1u);
}

std::uint32_t stubAddress(int kind) noexcept {
    switch (kind) {
    case 0x10:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&zeroStub));
    case 0x20:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&endpointStub));
    case 0x7C:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&networkStub));
    default:
        return 0u;
    }
}

std::vector<std::uint8_t> parseHex(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
    }
    return out;
}

std::string toHex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

struct FixupEntry {
    int stubKind = -1;
    std::size_t offset = 0;
};

std::vector<FixupEntry> parseFixups(const std::string& text) {
    std::vector<FixupEntry> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::string item =
            text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            FixupEntry entry{};
            if ((item[0] == 'v' || item[0] == 'V') && item.find('@') != std::string::npos) {
                const std::size_t at = item.find('@');
                entry.stubKind = static_cast<int>(std::strtoul(item.substr(1, at - 1).c_str(), nullptr, 16));
                entry.offset = static_cast<std::size_t>(std::strtoul(item.substr(at + 1).c_str(), nullptr, 0));
            } else {
                entry.offset = static_cast<std::size_t>(std::strtoul(item.c_str(), nullptr, 0));
            }
            out.push_back(entry);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return out;
}

std::uint8_t* ptrAt(std::vector<std::uint8_t>& blob, std::uint32_t offset) noexcept {
    if (offset == 0xFFFFFFFFu || offset >= blob.size()) {
        return nullptr;
    }
    return blob.data() + offset;
}

std::uint32_t ptrToResult(const std::vector<std::uint8_t>& blob, std::uint32_t value) noexcept {
    if (value == 0u) {
        return 0u;
    }
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    if (value >= base && value < base + blob.size()) {
        return value - base;
    }
    return value;
}

void runCase(std::uint32_t va, std::size_t selfOffset, std::vector<std::uint8_t>& blob,
             const std::vector<FixupEntry>& fixups, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto baseAddr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const FixupEntry& fix : fixups) {
        if (fix.offset + 4 > blob.size()) {
            continue;
        }
        if (fix.stubKind >= 0) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), stubAddress(fix.stubKind));
        } else {
            const auto rel = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset));
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), baseAddr + rel);
        }
    }
    gEndpoint = blob.size() > 0x500 ? blob.data() + 0x500 : nullptr;
    std::uint8_t* self = selfOffset < blob.size() ? blob.data() + selfOffset : nullptr;
    std::uint32_t ret = 0;
    std::uint8_t* gameInfo = ptrAt(blob, a1);

    switch (va) {
    case 0x005D3770: ret = ra3::net::childByte2E(self); break;
    case 0x005D3790: ret = ra3::net::childByte2D(self); break;
    case 0x005D37B0: ret = ra3::net::childDword30(self); break;
    case 0x005D37D0: ret = ra3::net::childField8C(self); break;
    case 0x005D3C50: ret = ra3::net::childByte29(self); break;
    case 0x005D3C70: ret = ra3::net::childByte28(self); break;
    case 0x005D4010: ret = ra3::net::cursorFieldsDiffer(self); break;
    case 0x005D4920: ret = ra3::net::storePair124(self, a0, a1); break;
    case 0x005D4960: ret = ra3::net::flagOrPointerSet(self); break;
    case 0x005D5450:
        ret = static_cast<std::uint32_t>(ra3::net::natBufferRead(self, ptrAt(blob, a0), static_cast<std::int32_t>(a1)));
        break;
    case 0x005D5540: ret = ra3::net::natBufferSeek(self, a0, a1); break;
    case 0x005D5BD0: ret = ra3::net::clearSlotIpAddresses(self); break;
    case 0x005D5C40: ret = ra3::net::dismissIndexedSlot(self); break;
    case 0x005DC180: ret = ra3::net::findLocalHumanSlot(self); break;
    case 0x005DC1C0: ret = ra3::net::slotZeroIsLocalHuman(self); break;
    case 0x005DC1F0: ret = ra3::net::otherSlotIsHuman(self); break;
    case 0x005DBFF0: ret = ra3::net::countAcceptedSlots(self); break;
    case 0x005DCF10: ret = ra3::net::clearLanHumanReady(self, ptrAt(blob, a0)); break;
    case 0x005D68B0: ret = ra3::net::linkStateIsOne(self); break;
    case 0x005D68C0: ret = ra3::net::linkChildField8(self); break;
    case 0x005D68D0: ret = ra3::net::clampChannelStamp(self, a0, a1); break;
    case 0x005D68F0: ret = ra3::net::setChannelBit(self, a0); break;
    case 0x005DD100:
        ret = ra3::net::channelFrameReached(self, a0, ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x005D6010: ret = ra3::net::chainNodeAt(self, static_cast<std::int32_t>(a0)); break;
    case 0x005D60B0: ret = ra3::net::findEndpointNode(self, ptrAt(blob, a0)); break;
    case 0x005D6140: ret = ra3::net::headEndpointMatches(self); break;
    case 0x005D3260: ret = ra3::net::endpointsEqual(self, ptrAt(blob, a0)); break;
    case 0x005D3290: ret = ra3::net::endpointsDiffer(self, ptrAt(blob, a0)); break;
    case 0x005D32C0: ret = ra3::net::clampTinyState(self, a0); break;
    case 0x005D6430: ret = ra3::net::recordAtFc(self, a0); break;
    case 0x005DB640: ret = ra3::net::occupiedRecordField(self, a0); break;
    case 0x005DB670: ret = ra3::net::setOccupiedRecordField(self, a0, a1); break;
    case 0x005D6040: ret = ra3::net::unlinkWideChain(self, ptrAt(blob, a0), gameInfo); break;
    case 0x005D60F0: ret = ra3::net::unlinkShortChain(self, ptrAt(blob, a0)); break;
    case 0x005D4DF0: ret = ra3::net::initTaggedPointer(self, ptrAt(blob, a0)); break;
    case 0x005D5C00: ret = ra3::net::stateIsSixOrSeven(self); break;
    case 0x005D6840: ret = ra3::net::attachAndRetain(self, ptrAt(blob, a0)); break;
    case 0x005DB600: ra3::net::initFloatExtents(self); break;
    case 0x005D5B50: ret = ra3::net::otherSlotSharesId(self, a0, a1); break;
    case 0x005D5F80: ret = ra3::net::slotStateIsAi(self); break;
    case 0x005D8480: ret = ra3::net::childDword6C(self); break;
    case 0x005D84A0: ret = ra3::net::childDword7C(self); break;
    default:
        std::cout << "unknown va\n";
        return;
    }

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off), cur - baseAddr);
        } else {
            for (int kind : {0x10, 0x20, 0x7C}) {
                if (cur != 0u && cur == stubAddress(kind)) {
                    store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off),
                                         0xF0000000u | static_cast<std::uint32_t>(kind));
                }
            }
        }
    }
    if (va == 0x005D6040 && blob.size() >= 0xF04) {
        store<std::uint32_t>(blob.data(), 0xF00, ptrToResult(blob, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(gameInfo))));
    }
    ret = ptrToResult(blob, ret);

    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
              << " blob=" << toHex(blob.data(), blob.size())
              << " st=0x" << std::setw(8) << 0u << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--batch") {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            std::istringstream iss(line);
            std::string vaStr;
            std::string selfStr;
            std::string blobHex;
            std::string fixStr;
            std::string a0Str;
            std::string a1Str;
            std::string a2Str;
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
                return 1;
            }
            auto blob = parseHex(blobHex);
            runCase(static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0)),
                    static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0)), blob,
                    parseFixups(fixStr),
                    static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0)),
                    static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0)),
                    static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0)));
        }
        return 0;
    }
    return 2;
}
