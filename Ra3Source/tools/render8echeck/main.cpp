#include <ra3/render/W3DAnimation.h>
#include <ra3/render/W3DDrawable.h>
#include <ra3/render/W3DMaterial.h>

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

std::uint32_t asBits(float value)
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
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

std::uint32_t run(
    std::uint32_t address, std::vector<std::byte>& blob,
    const std::vector<std::uint32_t>& arguments)
{
    using namespace ra3::render;
    auto* base = blob.data();
    const auto at = [&](std::size_t index) { return base + arguments.at(index); };
    switch (address) {
    case 0x008E4610: setDrawableWord104(at(0), arguments.at(1)); break;
    case 0x008E4620: setDrawableWord108(at(0), arguments.at(1)); break;
    case 0x008E4680: setDrawableExtent(at(0), arguments.at(1), arguments.at(2)); break;
    case 0x008E46A0:
        setDrawableViewport(at(0), asFloat(arguments.at(1)), asFloat(arguments.at(2)),
                            asFloat(arguments.at(3)), asFloat(arguments.at(4)));
        break;
    case 0x008E4820: disableDrawableMode(at(0)); break;
    case 0x008E4840:
        configureDrawableMode(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::int32_t>(arguments.at(3)),
            static_cast<std::int32_t>(arguments.at(4)));
        break;
    case 0x008E4910: enableDrawableMode2(at(0)); break;
    case 0x008E4950: copyDrawableState68(at(0), at(1)); break;
    case 0x008E49E0: {
        std::int32_t x = 0;
        std::int32_t y = 0;
        normalizedToPixel(
            asFloat(arguments.at(2)), asFloat(arguments.at(3)), x, y,
            static_cast<std::int32_t>(arguments.at(4)),
            static_cast<std::int32_t>(arguments.at(5)));
        store(at(0), 0, x);
        store(at(1), 0, y);
        break;
    }
    case 0x008E4A40: {
        float x = 0.0F;
        float y = 0.0F;
        pixelToNormalized(
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::int32_t>(arguments.at(3)), x, y,
            static_cast<std::int32_t>(arguments.at(4)),
            static_cast<std::int32_t>(arguments.at(5)));
        store(at(0), 0, x);
        store(at(1), 0, y);
        break;
    }
    case 0x008E4AA0:
        assignPassMask(*reinterpret_cast<PassMask*>(at(0)),
                       *reinterpret_cast<const PassMask*>(at(1)));
        break;
    case 0x008E4AE0:
        return equalPassMask(*reinterpret_cast<const PassMask*>(at(0)),
                             *reinterpret_cast<const PassMask*>(at(1)));
    case 0x008E4B30:
        assignDrawablePassMask(at(0), *reinterpret_cast<const PassMask*>(at(1)));
        break;
    case 0x008E56B0: setAnimationVisible(at(0), static_cast<std::uint8_t>(arguments.at(1))); break;
    case 0x008E5710: return consumeCachedProperty50(at(0));
    case 0x008E5740: return refreshCachedProperty54(at(0));
    case 0x008E5550:
        approachWrappedAngle(
            *reinterpret_cast<float*>(at(0)), asFloat(arguments.at(1)));
        break;
    case 0x008E5B00: return modulatePackedColor(arguments.at(0), arguments.at(1));
    case 0x008E5B80: return addPackedColor(arguments.at(0), arguments.at(1));
    case 0x008E5F70: copyFloatWords18(at(0), at(1)); break;
    case 0x008E62E0: copyRecords24Backward(at(0), at(1), at(2)); break;
    case 0x008E6330:
        return equalMatrix3x4(
            reinterpret_cast<const float*>(at(0)), reinterpret_cast<const float*>(at(1)));
    case 0x008E6620:
        quaternionToMatrix3x4(
            reinterpret_cast<const float*>(at(0)), reinterpret_cast<float*>(at(1)));
        break;
    case 0x008E6770: copyDrawableDescriptor(at(0), at(1)); break;
    case 0x008E6800: initializeDrawableDescriptor(at(0)); break;
    case 0x008E87D0: return compatibleRenderFlags(arguments.at(0), arguments.at(1));
    case 0x008E88A0: copyFloatWords13(at(0), at(1)); break;
    case 0x008E8AB0: copyFloatWords15(at(0), at(1)); break;
    case 0x008EADD0: {
        auto* result = renderTreeLowerBound(at(0), load<std::int32_t>(at(2), 0));
        store(at(1), 0, result);
        return offsetOf(blob, result);
    }
    case 0x008EAE00: {
        auto* result = renderTreeUpperBound(at(0), load<std::int32_t>(at(2), 0));
        store(at(1), 0, result);
        return offsetOf(blob, result);
    }
    case 0x008EAF70: copyRenderState201(at(0), at(1)); break;
    case 0x008EB160:
        interpolateQuaternion(
            reinterpret_cast<float*>(at(0)), reinterpret_cast<const float*>(at(1)),
            reinterpret_cast<const float*>(at(2)), asFloat(arguments.at(3)));
        break;
    case 0x008EB2C0: copyRenderRecord68(at(0), at(1)); break;
    case 0x008EB620:
        return offsetOf(blob, copyRenderRecords68(at(0), at(1), at(2)));
    case 0x008EB6D0:
        return offsetOf(blob, copyRenderRecords68Backward(at(0), at(1), at(2)));
    case 0x008ED5B0: return hasExtendedRenderFlags(arguments.at(0));
    case 0x008EE0B0:
        buildDrawableTransform(
            reinterpret_cast<float*>(at(0)), reinterpret_cast<const float*>(at(1)), at(2),
            reinterpret_cast<const float*>(at(3)), reinterpret_cast<const float*>(at(4)));
        break;
    case 0x008EFE00: {
        void* cursor = load<void*>(at(0), 0);
        emitSegmentQuad(
            cursor, reinterpret_cast<const float*>(at(1)),
            reinterpret_cast<const float*>(at(2)), asFloat(arguments.at(3)), arguments.at(4));
        store(at(0), 0, cursor);
        return offsetOf(blob, cursor);
    }
    case 0x008E6DD0:
        return asBits(terrainLineMaximum(
            at(0), reinterpret_cast<const float*>(at(1)),
            reinterpret_cast<const float*>(at(2))));
    case 0x008E7000:
        return asBits(terrainCellMaximum(
            at(0), asFloat(arguments.at(1)), asFloat(arguments.at(2))));
    case 0x008E70F0: terrainExtent(at(0), reinterpret_cast<float*>(at(1))); break;
    case 0x008E71E0: terrainCorners(at(0), reinterpret_cast<float*>(at(1))); break;
    case 0x008E89D0: return modelHasTextureSlots(at(0));
    case 0x008E8A00:
        return static_cast<std::uint32_t>(chooseWeightedModel(
            at(0), static_cast<std::int32_t>(arguments.at(1)),
            static_cast<std::int32_t>(arguments.at(2)),
            static_cast<std::int32_t>(arguments.at(3))));
    case 0x008E8B20:
        return offsetOf(blob, findConditionSubset260(
            at(0), reinterpret_cast<const std::uint32_t*>(at(1))));
    case 0x008E8C10:
        return offsetOf(blob, findConditionSubset2c(
            at(0), reinterpret_cast<const std::uint32_t*>(at(1))));
    case 0x008E92A0: return modelConditionStateDifferent(at(0), at(1));
    case 0x008E9350:
        setDrawableSelectable(at(0), static_cast<std::uint8_t>(arguments.at(1)));
        break;
    case 0x008E95F0: setAnimationFrame(at(0), asFloat(arguments.at(1))); break;
    case 0x008E96B0: return animationBlendActive(at(0));
    case 0x008E98A0: buildHermiteTangents(reinterpret_cast<float*>(at(0))); break;
    case 0x008E9980:
        evaluateHermite(
            reinterpret_cast<const float*>(at(0)), reinterpret_cast<float*>(at(1)),
            asFloat(arguments.at(2)));
        break;
    case 0x008E9BE0: advanceTextureFrames(at(0), asFloat(arguments.at(1))); break;
    default: throw std::runtime_error("unknown address");
    }
    return 0;
}

void command(const std::string& line)
{
    std::istringstream input(line);
    std::string addressText;
    std::string blobText;
    std::string fixupText;
    input >> addressText >> blobText >> fixupText;
    if (addressText.empty()) {
        return;
    }
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
