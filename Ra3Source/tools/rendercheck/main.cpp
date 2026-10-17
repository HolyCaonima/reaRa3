#include <ra3/render/W3DRainManager.h>
#include <ra3/render/W3DView.h>

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
    if (text == ".") {
        return {};
    }
    if ((text.size() & 1U) != 0) {
        throw std::runtime_error("odd hex");
    }
    std::vector<std::byte> result(text.size() / 2);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            std::stoul(text.substr(index * 2, 2), nullptr, 16));
    }
    return result;
}

std::string encode(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        out << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return out.str();
}

template <class T>
T bits(std::uint32_t value)
{
    static_assert(sizeof(T) == sizeof(value));
    T result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::uint32_t floatBits(float value)
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void mutate(const std::string& operation, std::vector<std::byte>& object,
            std::istringstream& input)
{
    using namespace ra3::render;
    std::uint32_t a = 0;
    input >> a;
    if (operation == "wire") setWireframePending(object.data(), static_cast<std::uint8_t>(a));
    else if (operation == "clearfilter") clearFilter(object.data(), a);
    else if (operation == "filter") {
        std::uint32_t b = 0; input >> b; setFilter(object.data(), a, b);
    } else if (operation == "setlock") setCameraLockRaw(object.data(), static_cast<std::int32_t>(a));
    else if (operation == "clearshake") clearCameraShake(object.data());
    else if (operation == "shakeflag0") setCameraShakeFlag0(object.data(), static_cast<std::uint8_t>(a));
    else if (operation == "shakeflag1") setCameraShakeFlag1(object.data(), static_cast<std::uint8_t>(a));
    else if (operation == "shakeflag2") setCameraShakeFlag2(object.data(), static_cast<std::uint8_t>(a));
    else if (operation == "shakeflag3") setCameraShakeFlag3(object.data(), static_cast<std::uint8_t>(a));
    else if (operation == "translate") {
        std::uint32_t b = 0, c = 0; input >> b >> c;
        translateCameraSamples(object.data(), {bits<float>(a), bits<float>(b), bits<float>(c)});
    } else if (operation == "updatezoom") updateZoom(object.data());
    else if (operation == "updatepitch") updatePitch(object.data());
    else if (operation == "updateheight") updateHeight(object.data());
    else if (operation == "updateangle") updateAngle(object.data());
    else throw std::runtime_error("unknown mutation");
}

void command(const std::string& line)
{
    using namespace ra3::render;
    std::istringstream input(line);
    std::string operation;
    input >> operation;
    if (operation == "easeconfig") {
        std::uint32_t a = 0, b = 0, c = 0;
        input >> a >> b >> c;
        ParabolicEase ease{};
        configureEase(ease, bits<float>(a), bits<float>(b), bits<float>(c));
        std::cout << encode(&ease, sizeof(ease)) << '\n';
        return;
    }
    if (operation == "easeeval") {
        std::string encoded;
        std::uint32_t value = 0;
        input >> encoded >> value;
        const auto data = decode(encoded);
        if (data.size() != sizeof(ParabolicEase)) throw std::runtime_error("ease size");
        ParabolicEase ease{};
        std::memcpy(&ease, data.data(), sizeof(ease));
        std::cout << floatBits(evaluateEase(ease, bits<float>(value))) << '\n';
        return;
    }
    if (operation == "raindrop") {
        std::uint32_t x = 0, y = 0, z = 0, color = 0;
        input >> x >> y >> z >> color;
        const auto drop = makeRainDrop(bits<float>(x), bits<float>(y), bits<float>(z), color);
        std::cout << encode(&drop, sizeof(drop)) << '\n';
        return;
    }
    if (operation == "fld80") {
        std::uint32_t value = 0;
        input >> value;
        unsigned char extended[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            fld dword ptr [value]
            fstp tbyte ptr [extended]
        }
#endif
        std::cout << encode(extended, sizeof(extended)) << '\n';
        return;
    }
    if (operation == "rainindices") {
        std::size_t count = 0;
        input >> count;
        std::vector<std::uint16_t> indices(count * 6);
        fillRainIndices(indices.data(), count);
        std::cout << encode(indices.data(), indices.size() * sizeof(indices[0])) << '\n';
        return;
    }
    if (operation == "rainenable") {
        std::string encoded;
        unsigned enabled = 0;
        input >> encoded >> enabled;
        auto data = decode(encoded);
        if (data.size() % sizeof(RainSegment) != 0) {
            throw std::runtime_error("rain segment size");
        }
        auto* first = reinterpret_cast<RainSegment*>(data.data());
        setRainEnabled(first, first + data.size() / sizeof(RainSegment), enabled != 0);
        std::cout << encode(data.data(), data.size()) << '\n';
        return;
    }

    std::string encoded;
    input >> encoded;
    auto object = decode(encoded);
    if (operation == "scaledheight") {
        std::cout << floatBits(scaledHeight(object.data())) << '\n';
    } else if (operation == "guardbias") {
        std::cout << static_cast<std::byte*>(guardBandBias(object.data())) - object.data() << '\n';
    } else if (operation == "finished") {
        std::cout << cameraMovementFinished(object.data()) << '\n';
    } else if (operation == "lockequals") {
        std::int32_t value = 0; input >> value;
        std::cout << cameraLockEquals(object.data(), value) << '\n';
    } else if (operation == "getlock") {
        std::cout << cameraLockRaw(object.data()) << '\n';
    } else if (operation == "shakex") std::cout << floatBits(cameraShakeX(object.data())) << '\n';
    else if (operation == "shakey") std::cout << floatBits(cameraShakeY(object.data())) << '\n';
    else if (operation == "shakeintensity") std::cout << floatBits(cameraShakeIntensity(object.data())) << '\n';
    else if (operation == "shakeactive") {
        std::cout << static_cast<unsigned>(cameraShakeActive(object.data())) << '\n';
    }
    else {
        mutate(operation, object, input);
        std::cout << encode(object.data(), object.size()) << '\n';
    }
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") return 0;
        try {
            command(line);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
