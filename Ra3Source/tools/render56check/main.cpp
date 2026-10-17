#include <ra3/render/W3DTerrainCluster56.h>

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

std::vector<std::byte> decode(const std::string& text)
{
    if ((text.size() & 1U) != 0) {
        throw std::runtime_error("odd hex input");
    }
    std::vector<std::byte> result(text.size() / 2);
    for (std::size_t index = 0; index != result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            std::stoul(text.substr(index * 2, 2), nullptr, 16));
    }
    return result;
}

std::string encode(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index != size; ++index) {
        output << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return output.str();
}

template <class T>
T load(const void* object, std::size_t offset)
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template <class T>
void store(void* object, std::size_t offset, T value)
{
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

float asFloat(std::uint32_t bits)
{
    float result = 0.0F;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::vector<std::size_t> parseFixups(const std::string& text)
{
    std::vector<std::size_t> result;
    if (text == ".") {
        return result;
    }
    std::istringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        result.push_back(std::stoul(item, nullptr, 16));
    }
    return result;
}

std::uint32_t offsetOf(const std::vector<std::byte>& blob, const void* pointer)
{
    if (pointer == nullptr) {
        return 0;
    }
    const auto* base = blob.data();
    const auto* value = static_cast<const std::byte*>(pointer);
    if (value < base || value > base + blob.size()) {
        return 0xFFFFFFFFU;
    }
    return static_cast<std::uint32_t>(value - base);
}

// ============================================================================
// Mock Vtable Callbacks for Vtable-Dispatching Functions in W3DTerrainCluster56
// ============================================================================

// 1. For 0x00561d70: W3DTerrainVisual vtable+0x28(float x, float y) -> void* subobj
// Object layout in test blob:
//   +0x00: vtable ptr
//   +0x04: subobj ptr (returned if x >= 0.0f)
//   +0x08: recorded x bits
//   +0x0c: recorded y bits
void* __fastcall mockTerrainVisualSlot10(void* self, void* /*edx*/, float x, float y)
{
    store(self, 0x08, x);
    store(self, 0x0c, y);
    if (x >= 0.0F) {
        return load<void*>(self, 0x04);
    }
    return nullptr;
}

// 2. For 0x00561e10: m_terrainRenderObject vtable+0x28(const int32_t* region)
// RenderObj layout in test blob:
//   +0x00: vtable ptr
//   +0x04: call count
//   +0x08: non-null region flag
//   +0x0c..+0x18: copied 4 int32s from region if non-null
void __fastcall mockRenderObjSlot10Update(void* self, void* /*edx*/, const std::int32_t* region)
{
    store<std::uint32_t>(self, 0x04, load<std::uint32_t>(self, 0x04) + 1U);
    if (region != nullptr) {
        store<std::uint32_t>(self, 0x08, 1U);
        store<std::int32_t>(self, 0x0c, region[0]);
        store<std::int32_t>(self, 0x10, region[1]);
        store<std::int32_t>(self, 0x14, region[2]);
        store<std::int32_t>(self, 0x18, region[3]);
    } else {
        store<std::uint32_t>(self, 0x08, 0U);
    }
}

// 3. For 0x00561e80: m_terrainRenderObject vtable+0x4c()
void __fastcall mockRenderObjSlot19RemoveBibs(void* self, void* /*edx*/)
{
    store<std::uint32_t>(self, 0x04, load<std::uint32_t>(self, 0x04) + 0x100U);
}

// 4. For 0x005882c0: m_terrainRenderObject vtable+0x08(void* rayCollisionTest)
// RenderObj layout in test blob:
//   +0x00: vtable ptr
//   +0x04: return value (uint32_t, low byte = hit bool, upper 24 bits = residue)
//   +0x08..+0x10: simulated contact point (3 floats) written into castResult+0x18..+0x20
//   +0x14: recorded ray Length (from rayCollisionTest+0x38)
//   +0x18: recorded collisionType (from rayCollisionTest+0x04)
std::uint32_t __fastcall mockRenderObjSlot11CastRay(void* self, void* /*edx*/, void* rayTest)
{
    void* castRes = load<void*>(rayTest, 0x00);
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(rayTest, 0x38));
    store<std::uint32_t>(self, 0x18, load<std::uint32_t>(rayTest, 0x04));
    store<std::uint32_t>(castRes, 0x18, load<std::uint32_t>(self, 0x08));
    store<std::uint32_t>(castRes, 0x1c, load<std::uint32_t>(self, 0x0c));
    store<std::uint32_t>(castRes, 0x20, load<std::uint32_t>(self, 0x10));
    return load<std::uint32_t>(self, 0x04);
}

// 5. For 0x00562d30: InputStream vtable+0x04(void* dst, int32_t size)
// Stream layout in test blob:
//   +0x00: vtable ptr
//   +0x04: data buffer ptr
//   +0x08: total bytes available
//   +0x0c: current read cursor
std::int32_t __fastcall mockInputStreamRead(void* self, void* /*edx*/, void* dst, std::int32_t size)
{
    const auto* data = load<const std::uint8_t*>(self, 0x04);
    const auto total = load<std::int32_t>(self, 0x08);
    const auto pos = load<std::int32_t>(self, 0x0c);
    const std::int32_t avail = total - pos;
    const std::int32_t toRead = avail < size ? (avail > 0 ? avail : 0) : size;
    if (toRead > 0) {
        std::memcpy(dst, data + pos, static_cast<std::size_t>(toRead));
        store<std::int32_t>(self, 0x0c, pos + toRead);
    }
    return toRead;
}

// 6. For 0x00563520: StreamObj vtable+0x0c(uint32_t lockArg)
// StreamObj layout in test blob:
//   +0x00: vtable ptr
//   +0x04: returned uint32_t value
//   +0x08: recorded lockArg
std::uint32_t __fastcall mockStreamObjSlot3(void* self, void* /*edx*/, std::uint32_t lockArg)
{
    store<std::uint32_t>(self, 0x08, lockArg);
    return load<std::uint32_t>(self, 0x04) + lockArg;
}

std::uint32_t run(
    std::uint32_t address, std::vector<std::byte>& blob,
    const std::vector<std::uint32_t>& arguments)
{
    using namespace ra3::render;
    auto* base = blob.data();
    const auto at = [&](std::size_t index) -> void* {
        const auto off = arguments.at(index);
        return off == 0xFFFFFFFFU ? nullptr : base + off;
    };

    switch (address) {
    // 1. W3DTerrainVisual & ray/water helpers
    case 0x00561D70: {
        // Install mockTerrainVisualSlot10 at vtable+0x28
        void* vt = load<void*>(at(0), 0x00);
        store(vt, 0x28, &mockTerrainVisualSlot10);
        const auto res = queryTerrainVisualSubobjectSlot6(
            at(0), asFloat(arguments.at(1)), asFloat(arguments.at(2)));
        store<std::uint32_t>(vt, 0x28, 0U);
        return res;
    }
    case 0x00561DB0:
        return static_cast<std::uint32_t>(getRawMapHeight(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2))));
    case 0x00561E10: {
        void* renderObj = load<void*>(at(0), 0x28);
        void* vt = renderObj != nullptr ? load<void*>(renderObj, 0x00) : nullptr;
        if (vt != nullptr) {
            store(vt, 0x28, &mockRenderObjSlot10Update);
        }
        partialMapUpdate(at(0), at(1));
        if (vt != nullptr) {
            store<std::uint32_t>(vt, 0x28, 0U);
        }
        return 0;
    }
    case 0x00561E70:
        return static_cast<std::uint32_t>(getTerrainVisualBorderSize(at(0)));
    case 0x00561E80: {
        void* renderObj = load<void*>(at(0), 0x28);
        void* vt = renderObj != nullptr ? load<void*>(renderObj, 0x00) : nullptr;
        if (vt != nullptr) {
            store(vt, 0x4c, &mockRenderObjSlot19RemoveBibs);
        }
        removeAllBibs(at(0));
        if (vt != nullptr) {
            store<std::uint32_t>(vt, 0x4c, 0U);
        }
        return 0;
    }
    case 0x0056B290:
        getTerrainVisualColorAt(
            at(0), asFloat(arguments.at(1)), asFloat(arguments.at(2)), at(3));
        return 0;
    case 0x0056B2F0:
        return setRawMapHeightAndFlags(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::int32_t>(arguments.at(3)), arguments.at(4));
    case 0x0056B380:
        return getCellFlags(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x00570D50:
        recalculateLineSeg(at(0));
        return 0;
    case 0x00572070:
        return offsetOf(blob, initRayCollisionTest(
            at(0), at(1), at(2), arguments.at(3),
            static_cast<std::uint8_t>(arguments.at(4)),
            static_cast<std::uint8_t>(arguments.at(5))));
    case 0x00580690:
        return offsetOf(blob, setLineSegPoints(at(0), at(1), at(2)));
    case 0x005882C0: {
        void* renderObj = load<void*>(at(0), 0x28);
        void* vt = renderObj != nullptr ? load<void*>(renderObj, 0x00) : nullptr;
        if (vt != nullptr) {
            store(vt, 0x08, &mockRenderObjSlot11CastRay);
        }
        const auto res = intersectTerrain(at(0), at(1), at(2), at(3));
        if (vt != nullptr) {
            store<std::uint32_t>(vt, 0x08, 0U);
        }
        return res;
    }
    case 0x0057A140:
        initWaterWaveGridState(at(0));
        return 0;

    // 2. W3DTerrainLogic & helpers
    case 0x0056EC10:
        getTerrainLogicExtent(at(0), at(1));
        return 0;
    case 0x0056ECC0:
        getMaximumPathfindExtent(at(0), at(1));
        return 0;
    case 0x0056EDB0:
        getSmallestTerrainExtent(at(0), at(1));
        return 0;
    case 0x00563980:
        orderCoord3DPair(
            asFloat(arguments.at(0)), asFloat(arguments.at(1)), asFloat(arguments.at(2)),
            asFloat(arguments.at(3)), asFloat(arguments.at(4)), asFloat(arguments.at(5)),
            at(6), at(7));
        return 0;

    // 3. WorldHeightMap
    case 0x00562BB0:
        setFlipState(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::uint8_t>(arguments.at(3)));
        return 0;
    case 0x00562D30: {
        void* vt = load<void*>(at(0), 0x00);
        store(vt, 0x00, &mockInputStreamRead);
        const auto res = static_cast<std::uint32_t>(countTilesFromTgaHeader(
            at(0), reinterpret_cast<std::uint8_t*>(at(1))));
        store<std::uint32_t>(vt, 0x00, 0U);
        return res;
    }
    case 0x00562DF0:
        getTerrainColorAt(
            at(0), asFloat(arguments.at(1)), asFloat(arguments.at(2)), at(3));
        return 0;
    case 0x00562F10:
        evaluateBlendTileCoeffs(
            at(0), at(1), reinterpret_cast<float*>(at(2)));
        return 0;
    case 0x005642B0:
        return getCellFlagBit78(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x00564300:
        return getCellFlagBit88(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056D970:
        return getCliffState(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056D9D0:
        return getCellFlagBit98(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DA40:
        return getCellFlagBitB8(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DAB0:
        return getCellFlagBitA8(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DB20:
        return getCellFlagBitD8(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DB80:
        return getCellFlagBitC8(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DC30:
        setCliffState(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::uint8_t>(arguments.at(3)));
        return 0;
    case 0x0056DFB0:
        setCellCliffFlagFromHeights(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
        return 0;
    case 0x0057E4E0:
        initCliffFlagsFromHeights(at(0));
        return 0;
    case 0x0056E0A0:
        getCellBlendClassesAndCoeffs(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            reinterpret_cast<std::int32_t*>(at(3)),
            reinterpret_cast<float*>(at(4)));
        return 0;
    case 0x0056DD70:
        return isCliffMappedTextureCheck(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0057DB70:
        return shouldFlipCellQuad(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
    case 0x0056DE70:
        getBlendVertexAlphaMask(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            reinterpret_cast<std::uint8_t*>(at(3)), arguments.at(4));
        return 0;

    // 4. BaseHeightMapRenderObjClass, Scorch, Road & Prop Buffers
    case 0x00565250:
        clearPatchVisibilityAndBounds(at(0));
        return 0;
    case 0x00563520: {
        void* vt = load<void*>(at(1), 0x00);
        store(vt, 0x0c, &mockStreamObjSlot3);
        const auto res = offsetOf(blob, initStreamLockHelper(at(0), at(1), arguments.at(2)));
        store<std::uint32_t>(vt, 0x0c, 0U);
        return res;
    }
    case 0x005635F0:
        return offsetOf(blob, vec3SignMaskMin(at(0), at(1), at(2)));
    case 0x005636E0:
        return offsetOf(blob, vec3SignMaskMax(at(0), at(1), at(2)));
    case 0x00564E70:
        aaboxFromMinMax(at(0), at(1));
        return 0;
    case 0x00564F20:
        minMaxAABoxMergePoint(at(0), at(1));
        return 0;
    case 0x00564F80:
        minMaxAABoxMergeCenterExtentBox(at(0), at(1));
        return 0;
    case 0x00565130:
        packCompressedTerrainVertex(
            at(0), at(1), at(2), at(3), at(4), at(5));
        return 0;
    case 0x00565700:
        copyTerrainRenderState52(at(0), at(1));
        return 0;
    case 0x005615A0:
        clearAllScorches(at(0));
        return 0;
    case 0x0056AB00:
        invalidateScorchesInRadius(at(0), at(1), asFloat(arguments.at(2)));
        return 0;
    case 0x005612F0:
        clearRoadTypeLinks(at(0));
        return 0;
    case 0x00561340:
        adjustRoadStacking(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)));
        return 0;
    case 0x00569570:
        linkRoadsToRoadTypes(at(0));
        return 0;
    case 0x005695E0:
        updateRoadSegmentCounts(at(0), at(1));
        return 0;
    case 0x00569750:
        updateAllRoadCountsAndFlags(at(0));
        return 0;
    case 0x0056A8A0:
        rotateRoadPointAbout(
            at(0), at(1), asFloat(arguments.at(2)),
            asFloat(arguments.at(3)), asFloat(arguments.at(4)));
        return 0;
    case 0x00568EA0:
        return offsetOf(blob, initRoadSegment(at(0)));
    case 0x00569480:
        return offsetOf(blob, copyRoadSegment(at(0), at(1)));
    case 0x0058D1B0:
        return offsetOf(blob, initPropBuffer(at(0)));

    default:
        throw std::runtime_error("unknown address");
    }
}

void setFpuControlWord(std::uint16_t value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fldcw value
    }
#else
    (void)value;
#endif
}

void command(const std::string& line)
{
    std::istringstream input(line);
    std::string addressText;
    input >> addressText;
    if (addressText.empty()) {
        return;
    }
    if (addressText == "fpcw") {
        std::string value;
        input >> value;
        setFpuControlWord(
            static_cast<std::uint16_t>(std::stoul(value, nullptr, 16)));
        std::cout << "ok\n";
        return;
    }
    std::string blobText;
    std::string fixupText;
    input >> blobText >> fixupText;
    auto blob = decode(blobText);
    const auto fixups = parseFixups(fixupText);
    for (const auto offset : fixups) {
        const auto relative = load<std::uint32_t>(blob.data(), offset);
        const auto value = relative == 0 ? nullptr : blob.data() + relative;
        store(blob.data(), offset, value);
    }
    std::vector<std::uint32_t> arguments;
    std::string argument;
    while (input >> argument) {
        arguments.push_back(static_cast<std::uint32_t>(std::stoul(argument, nullptr, 16)));
    }
    const auto result = run(
        static_cast<std::uint32_t>(std::stoul(addressText, nullptr, 16)), blob, arguments);
    for (const auto offset : fixups) {
        const auto value = load<void*>(blob.data(), offset);
        store(blob.data(), offset, offsetOf(blob, value));
    }
    std::cout << std::hex << std::setfill('0') << std::setw(8) << result
              << ' ' << encode(blob.data(), blob.size()) << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }
        try {
            command(line);
        } catch (const std::exception&) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
