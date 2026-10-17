#include <ra3/asset/ReloDetokenizers.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

static_assert(sizeof(void*) == 4, "ra3_relodetokcheck requires 32-bit x86 pointers");

constexpr std::uintptr_t kArenaBase = 0x60000000u;
constexpr std::size_t kArenaSize = 0x00020000u;
constexpr std::uintptr_t kInstanceOffset = 0x1000u;
constexpr std::uintptr_t kSrcOffset = 0x2000u;
constexpr std::uintptr_t kDstOffset = 0x3000u;
constexpr std::uintptr_t kHeapStartOffset = 0x4000u;

struct ArenaState {
    std::uint8_t* base = nullptr;
    std::size_t heapCursor = kHeapStartOffset;
    std::vector<std::string> events;

    void reset() noexcept
    {
        std::memset(base, 0, kArenaSize);
        heapCursor = kHeapStartOffset;
        events.clear();
    }

    void* ptrAt(std::size_t offset) noexcept
    {
        return base + offset;
    }

    std::uintptr_t addrOf(const void* ptr) const noexcept
    {
        if (ptr == nullptr) {
            return 0u;
        }
        return static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(ptr));
    }

    void* allocAligned(std::uint32_t bytes) noexcept
    {
        const std::size_t aligned = (heapCursor + 15u) & ~std::size_t(15u);
        const std::size_t step = (bytes == 0u) ? 16u : ((std::size_t(bytes) + 15u) & ~std::size_t(15u));
        void* result = base + aligned;
        std::memset(result, 0xCC, step);
        heapCursor = aligned + step;
        return result;
    }
};

void* hookAllocTagged(std::uint32_t size, void* user) noexcept
{
    auto* arena = static_cast<ArenaState*>(user);
    void* block = arena->allocAligned(size);
    std::ostringstream oss;
    oss << "alloc:" << std::hex << size << "@" << arena->addrOf(block);
    arena->events.push_back(oss.str());
    return block;
}

void* hookAllocRaw(std::uint32_t size, void* user) noexcept
{
    auto* arena = static_cast<ArenaState*>(user);
    void* block = arena->allocAligned(size);
    std::ostringstream oss;
    oss << "alloc_raw:" << std::hex << size << "@" << arena->addrOf(block);
    arena->events.push_back(oss.str());
    return block;
}

void hookFreeRaw(void* ptr, void* user) noexcept
{
    auto* arena = static_cast<ArenaState*>(user);
    std::ostringstream oss;
    oss << "free_raw:" << std::hex << arena->addrOf(ptr);
    arena->events.push_back(oss.str());
}

void hookExternalDetokenizer(
    std::uint32_t calleeVa,
    const void* instanceBase,
    const void* srcField,
    void* dstField,
    void* context,
    void* user) noexcept
{
    auto* arena = static_cast<ArenaState*>(user);
    std::uint32_t srcWord = 0x55555555u;
    if (srcField != nullptr) {
        std::memcpy(&srcWord, srcField, sizeof(srcWord));
    }
    const auto ctxVal = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
    const std::uint32_t tag = calleeVa ^ (srcWord * 0x9E3779B1u) ^ ctxVal;
    std::memcpy(dstField, &tag, sizeof(tag));

    std::ostringstream oss;
    oss << "call:" << std::hex << calleeVa
        << ":" << arena->addrOf(instanceBase)
        << ":" << arena->addrOf(srcField)
        << ":" << arena->addrOf(dstField)
        << ":" << ctxVal;
    arena->events.push_back(oss.str());
}

std::vector<std::uint8_t> decodeHex(const std::string& text)
{
    if (text == "-") {
        return {};
    }
    if ((text.size() & 1u) != 0u) {
        throw std::runtime_error("odd hex length");
    }
    const auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return unsigned(c - '0');
        if (c >= 'a' && c <= 'f') return unsigned(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return unsigned(c - 'A' + 10);
        throw std::runtime_error("invalid hex digit");
    };
    std::vector<std::uint8_t> out(text.size() / 2u);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>((nibble(text[i * 2u]) << 4u) | nibble(text[i * 2u + 1u]));
    }
    return out;
}

std::string encodeHex(const std::uint8_t* data, std::size_t size)
{
    if (size == 0u) {
        return "-";
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

std::string joinEvents(const std::vector<std::string>& events)
{
    if (events.empty()) {
        return "-";
    }
    std::ostringstream oss;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i > 0u) {
            oss << ",";
        }
        oss << events[i];
    }
    return oss.str();
}

std::uint32_t parseHexU32(const std::string& s)
{
    return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16));
}

using CloneFn = void (*)(
    const void*,
    const void*,
    void**,
    void*,
    const ra3::asset::ReloRuntimeHooks&) noexcept;

using WalkerFn = void (*)(
    const void*,
    const ra3::asset::ReloFieldDescriptor*,
    void*,
    void*,
    const ra3::asset::ReloRuntimeHooks&) noexcept;

CloneFn resolveCloneFn(std::uint32_t va) noexcept
{
    using namespace ra3::asset;
    switch (va) {
    case 0x00417550u: return &reloCloneCoord3D;
    case 0x004176B0u: return &reloCloneString;
    case 0x0043BAD0u: return &reloCloneValue8;
    case 0x0043BF60u: return &reloCloneObjectStatusBitFlags;
    case 0x0043BFB0u: return &reloCloneValue60;
    case 0x0043D830u: return &reloCloneStruct60_419580;
    case 0x0043FC60u: return &reloCloneStruct20_423860;
    case 0x004404A0u: return &reloCloneScalar32;
    case 0x004428C0u: return &reloCloneStruct12_41D1E0;
    case 0x004430E0u: return &reloCloneObjectStatusValidationDataType;
    case 0x00445C10u: return &reloCloneStruct36_4323B0;
    case 0x0045D9F0u: return &reloCloneSoundOrEvaEvent;
    case 0x0045E570u: return &reloCloneStruct44_44A230;
    case 0x0045F460u: return &reloCloneStruct8_455B10;
    case 0x0045F5D0u: return &reloCloneStruct8_448070;
    case 0x004617B0u: return &reloCloneStruct8_45B590;
    case 0x00478BA0u: return &reloCloneStruct24_467520;
    case 0x004812D0u: return &reloCloneStruct8_47EF30;
    case 0x00495180u: return &reloCloneStruct88_4944E0;
    case 0x004A9AB0u: return &reloCloneStruct40_4A7120;
    default: return nullptr;
    }
}

WalkerFn resolveWalkerFn(std::uint32_t va) noexcept
{
    using namespace ra3::asset;
    switch (va) {
    case 0x0041CF20u: return &reloDetokenizeModuleData;
    case 0x0041D0B0u: return &reloDetokenizeObjectStatusValidationDataType;
    case 0x0042C120u: return &reloDetokenizeAttackAssistResponseBehaviorModuleData;
    case 0x0042C8B0u: return &reloDetokenizeQueueDockModuleData;
    case 0x0042CDC0u: return &reloDetokenizeStructurePlacementBehaviorModuleData;
    case 0x00448170u: return &reloDetokenizeAudioVoiceEntry;
    case 0x00454BF0u: return &reloDetokenizeHighlanderBodyModuleData;
    case 0x00459520u: return &reloDetokenizeLaserStateModuleData;
    case 0x00472180u: return &reloDetokenizeSubObjectsUpgradeModuleData;
    case 0x00473100u: return &reloDetokenizeShipSlowDeathBehaviorModuleData;
    case 0x0047C190u: return &reloDetokenizeFXShaderMaterial;
    case 0x0048A430u: return &reloDetokenizeGarrisonContainModuleData;
    case 0x0048A520u: return &reloDetokenizeHordeGarrisonContainModuleData;
    case 0x0048B940u: return &reloDetokenizeCaptureSpecialAbilityUpdateModuleData;
    case 0x00493870u: return &reloDetokenizeW3DTankDrawModuleData;
    case 0x00494430u: return &reloDetokenizeToggleStatusSpecialAbilityUpdateModuleData;
    case 0x004A8280u: return &reloDetokenizeRecallUnitsSpecialPowerModuleData;
    case 0x004A8A50u: return &reloDetokenizeAttackTargetSpecialPowerModuleData;
    case 0x004A8EA0u: return &reloDetokenizeActivateModuleSpecialPower;
    case 0x004A94E0u: return &reloDetokenizeMoneyCrateCollideModuleData;
    case 0x004A9600u: return &reloDetokenizeVeterancyCrateCollideModuleData;
    default: return nullptr;
    }
}

}  // namespace

int main()
{
    void* mapped = VirtualAlloc(
        reinterpret_cast<void*>(kArenaBase),
        kArenaSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
    if (mapped != reinterpret_cast<void*>(kArenaBase)) {
        std::cerr << "VirtualAlloc failed to map 0x60000000\n";
        return 1;
    }

    ArenaState arena;
    arena.base = static_cast<std::uint8_t*>(mapped);

    ra3::asset::ReloRuntimeHooks hooks{};
    hooks.allocTagged = &hookAllocTagged;
    hooks.invokeExternalDetokenizer = &hookExternalDetokenizer;
    hooks.user = &arena;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        arena.reset();
        std::istringstream iss(line);
        std::string kind;
        iss >> kind;

        if (kind == "memcmp") {
            std::string lhsHex;
            std::string rhsHex;
            std::string countHex;
            iss >> lhsHex >> rhsHex >> countHex;
            const auto lhs = decodeHex(lhsHex);
            const auto rhs = decodeHex(rhsHex);
            const std::uint32_t count = parseHexU32(countHex);
            std::memcpy(arena.ptrAt(kInstanceOffset), lhs.data(), lhs.size());
            std::memcpy(arena.ptrAt(kSrcOffset), rhs.data(), rhs.size());
            const int res = ra3::asset::assetMemoryCompare(
                arena.ptrAt(kInstanceOffset),
                arena.ptrAt(kSrcOffset),
                count);
            std::cout << std::hex << static_cast<std::uint32_t>(res) << "\n";
        } else if (kind == "relofixup") {
            std::string tableHex;
            std::string instHex;
            iss >> tableHex >> instHex;
            const auto tableBytes = decodeHex(tableHex);
            const auto instBytes = decodeHex(instHex);
            std::memcpy(arena.ptrAt(kSrcOffset), tableBytes.data(), tableBytes.size());
            std::memcpy(arena.ptrAt(kInstanceOffset), instBytes.data(), instBytes.size());
            ra3::asset::reloApplyPointerOffsets(
                reinterpret_cast<const std::uint32_t*>(arena.ptrAt(kSrcOffset)),
                arena.ptrAt(kInstanceOffset));
            std::cout << encodeHex(static_cast<const std::uint8_t*>(arena.ptrAt(kInstanceOffset)), instBytes.size()) << "\n";
        } else if (kind == "scratch") {
            std::string dataHex;
            std::string capHex;
            std::string reqHex;
            iss >> dataHex >> capHex >> reqHex;
            auto* buf = reinterpret_cast<ra3::asset::ReloScratchBuffer*>(arena.ptrAt(kDstOffset));
            buf->data = reinterpret_cast<void*>(static_cast<std::uintptr_t>(parseHexU32(dataHex)));
            buf->capacity = parseHexU32(capHex);
            void* ret = buf->ensureCapacity(parseHexU32(reqHex), &hookAllocRaw, &hookFreeRaw, &arena);
            std::cout << std::hex << arena.addrOf(ret)
                      << " " << encodeHex(static_cast<const std::uint8_t*>(arena.ptrAt(kDstOffset)), sizeof(ra3::asset::ReloScratchBuffer))
                      << " " << joinEvents(arena.events) << "\n";
        } else if (kind == "clearhdr") {
            std::string hdrHex;
            iss >> hdrHex;
            const auto hdrBytes = decodeHex(hdrHex);
            auto* hdr = reinterpret_cast<ra3::asset::ReloStreamHeader*>(arena.ptrAt(kDstOffset));
            std::memcpy(hdr, hdrBytes.data(), hdrBytes.size());
            ra3::asset::ReloStreamHeader* ret = hdr->clear();
            std::cout << std::hex << arena.addrOf(ret)
                      << " " << encodeHex(static_cast<const std::uint8_t*>(arena.ptrAt(kDstOffset)), sizeof(ra3::asset::ReloStreamHeader)) << "\n";
        } else if (kind == "spanless") {
            std::string lhsHex;
            std::string rhsHex;
            std::string lhsLenHex;
            std::string rhsLenHex;
            iss >> lhsHex >> rhsHex >> lhsLenHex >> rhsLenHex;
            const auto lhsBytes = decodeHex(lhsHex);
            const auto rhsBytes = decodeHex(rhsHex);
            const std::uint32_t lhsLen = parseHexU32(lhsLenHex);
            const std::uint32_t rhsLen = parseHexU32(rhsLenHex);
            auto* lhsBuf = static_cast<std::uint8_t*>(arena.ptrAt(kInstanceOffset));
            auto* rhsBuf = static_cast<std::uint8_t*>(arena.ptrAt(kSrcOffset));
            std::memcpy(lhsBuf, lhsBytes.data(), lhsBytes.size());
            std::memcpy(rhsBuf, rhsBytes.data(), rhsBytes.size());
            auto* spans = reinterpret_cast<ra3::asset::AssetByteSpan*>(arena.ptrAt(kDstOffset));
            spans[0].begin = lhsBuf;
            spans[0].end = lhsBuf + lhsLen;
            spans[1].begin = rhsBuf;
            spans[1].end = rhsBuf + rhsLen;
            const bool less = ra3::asset::assetByteSpanLess(&spans[0], &spans[1]);
            std::cout << (less ? 1 : 0) << "\n";
        } else if (kind == "clone") {
            std::string vaHex;
            std::string isNullStr;
            std::string srcHex;
            std::string initDstPtrHex;
            std::string ctxHex;
            iss >> vaHex >> isNullStr >> srcHex >> initDstPtrHex >> ctxHex;
            const std::uint32_t va = parseHexU32(vaHex);
            const bool isNull = (isNullStr == "1");
            const auto srcBytes = decodeHex(srcHex);
            const std::uint32_t initDstPtr = parseHexU32(initDstPtrHex);
            const std::uint32_t ctx = parseHexU32(ctxHex);

            void* instanceBase = arena.ptrAt(kInstanceOffset);
            void* srcPtr = nullptr;
            if (!isNull) {
                srcPtr = arena.ptrAt(kSrcOffset);
                std::memcpy(srcPtr, srcBytes.data(), srcBytes.size());
            }
            auto* dstSlot = reinterpret_cast<void**>(arena.ptrAt(kDstOffset));
            *dstSlot = reinterpret_cast<void*>(static_cast<std::uintptr_t>(initDstPtr));

            CloneFn fn = resolveCloneFn(va);
            if (fn == nullptr) {
                std::cerr << "Unknown clone VA: " << vaHex << "\n";
                return 2;
            }
            fn(instanceBase, srcPtr, dstSlot, reinterpret_cast<void*>(static_cast<std::uintptr_t>(ctx)), hooks);
            const std::size_t heapUsed = arena.heapCursor - kHeapStartOffset;
            std::cout << std::hex << arena.addrOf(*dstSlot)
                      << " " << encodeHex(static_cast<const std::uint8_t*>(arena.ptrAt(kHeapStartOffset)), heapUsed)
                      << " " << joinEvents(arena.events) << "\n";
        } else if (kind == "walk") {
            std::string vaHex;
            std::string isNullStr;
            std::string instHex;
            std::string fieldsHex;
            std::string dstHex;
            std::string ctxHex;
            iss >> vaHex >> isNullStr >> instHex >> fieldsHex >> dstHex >> ctxHex;
            const std::uint32_t va = parseHexU32(vaHex);
            const bool isNull = (isNullStr == "1");
            const auto instBytes = decodeHex(instHex);
            const auto fieldsBytes = decodeHex(fieldsHex);
            const auto dstBytes = decodeHex(dstHex);
            const std::uint32_t ctx = parseHexU32(ctxHex);

            void* instanceBase = arena.ptrAt(kInstanceOffset);
            std::memcpy(instanceBase, instBytes.data(), instBytes.size());

            const ra3::asset::ReloFieldDescriptor* fieldsPtr = nullptr;
            if (!isNull) {
                void* rawFields = arena.ptrAt(kSrcOffset);
                std::memcpy(rawFields, fieldsBytes.data(), fieldsBytes.size());
                fieldsPtr = static_cast<const ra3::asset::ReloFieldDescriptor*>(rawFields);
            }

            void* dstStruct = arena.ptrAt(kDstOffset);
            std::memcpy(dstStruct, dstBytes.data(), dstBytes.size());

            WalkerFn fn = resolveWalkerFn(va);
            if (fn == nullptr) {
                std::cerr << "Unknown walker VA: " << vaHex << "\n";
                return 2;
            }
            fn(instanceBase, fieldsPtr, dstStruct, reinterpret_cast<void*>(static_cast<std::uintptr_t>(ctx)), hooks);
            const std::size_t heapUsed = arena.heapCursor - kHeapStartOffset;
            std::cout << encodeHex(static_cast<const std::uint8_t*>(dstStruct), dstBytes.size())
                      << " " << encodeHex(static_cast<const std::uint8_t*>(arena.ptrAt(kHeapStartOffset)), heapUsed)
                      << " " << joinEvents(arena.events) << "\n";
        } else {
            std::cerr << "Unknown command kind: " << kind << "\n";
            return 2;
        }
    }
    return 0;
}
