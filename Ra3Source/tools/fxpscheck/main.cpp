#include <ra3/gameclient/ParticleModules.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

float floatValue(const std::string& text)
{
    const std::uint32_t bits = number(text);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::vector<std::uint8_t> fromHex(const std::string& text)
{
    if ((text.size() & 1U) != 0U) {
        throw std::runtime_error("odd hex string");
    }
    std::vector<std::uint8_t> result(text.size() / 2U);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(
            std::stoul(text.substr(index * 2U, 2), nullptr, 16));
    }
    return result;
}

ra3::sim::RandomState randomState(const std::string& text)
{
    const auto bytes = fromHex(text);
    if (bytes.size() != sizeof(ra3::sim::RandomState)) {
        throw std::runtime_error("random state size");
    }
    ra3::sim::RandomState state{};
    std::memcpy(&state, bytes.data(), bytes.size());
    return state;
}

void printBytes(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
}

void printVector(
    const ra3::gameclient::Vector3& vector, const ra3::sim::RandomState& state)
{
    printBytes(&vector, sizeof(vector));
    std::cout << ' ';
    printBytes(&state, sizeof(state));
    std::cout << '\n';
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

ra3::sim::RandomVariable variable(
    const std::string& type, const std::string& low, const std::string& high)
{
    return {
        static_cast<std::int32_t>(number(type)),
        floatValue(low),
        floatValue(high)};
}

void run(const std::vector<std::string>& words)
{
    using namespace ra3::gameclient;
    if (words.empty()) {
        throw std::runtime_error("empty command");
    }
    if (words[0] == "fpcw") {
        setFpuControlWord(static_cast<std::uint16_t>(number(words.at(1))));
        std::cout << "ok\n";
        return;
    }
    if (words[0] == "unit") {
        auto state = randomState(words.at(1));
        printVector(randomUnitVector(state), state);
        return;
    }
    if (words[0] == "box") {
        auto state = randomState(words.at(1));
        BoxEmissionVolumeData data{};
        data.hollow = static_cast<std::uint8_t>(number(words.at(2)));
        data.halfSize = {floatValue(words.at(3)), floatValue(words.at(4)),
                         floatValue(words.at(5))};
        const VolumeModuleView module{0, 0, &data};
        printVector(emitBoxPoint(module, state), state);
        return;
    }
    if (words[0] == "cylinder") {
        auto state = randomState(words.at(1));
        CylinderEmissionVolumeData data{};
        data.hollow = static_cast<std::uint8_t>(number(words.at(2)));
        const float currentRadius = floatValue(words.at(3));
        data.length = floatValue(words.at(4));
        data.offset = {floatValue(words.at(5)), floatValue(words.at(6)),
                       floatValue(words.at(7))};
        const CylinderVolumeModuleView module{{0, 0, &data}, currentRadius};
        printVector(emitCylinderPoint(module, state), state);
        return;
    }
    if (words[0] == "line") {
        auto state = randomState(words.at(1));
        LineEmissionVolumeData data{};
        data.start = {floatValue(words.at(2)), floatValue(words.at(3)),
                      floatValue(words.at(4))};
        data.end = {floatValue(words.at(5)), floatValue(words.at(6)),
                    floatValue(words.at(7))};
        const VolumeModuleView module{0, 0, &data};
        printVector(emitLinePoint(module, state), state);
        return;
    }
    if (words[0] == "sphere") {
        auto state = randomState(words.at(1));
        SphereEmissionVolumeData data{};
        data.hollow = static_cast<std::uint8_t>(number(words.at(2)));
        data.radius = floatValue(words.at(3));
        const VolumeModuleView module{0, 0, &data};
        printVector(emitSpherePoint(module, state), state);
        return;
    }
    if (words[0] == "scaleunit") {
        auto state = randomState(words.at(1));
        printVector(scaleRandomUnitVector(floatValue(words.at(2)), state), state);
        return;
    }
    if (words[0] == "variableunit") {
        auto state = randomState(words.at(1));
        const auto radius = variable(words.at(2), words.at(3), words.at(4));
        printVector(randomVariableUnitVector(radius, state), state);
        return;
    }
    if (words[0] == "variableparts") {
        auto state = randomState(words.at(1));
        const auto radius = variable(words.at(2), words.at(3), words.at(4));
        const float scale = ra3::sim::clientRandomVariableValue(radius, state);
        const auto unit = randomUnitVector(state);
        printBytes(&scale, sizeof(scale));
        std::cout << ' ';
        printVector(unit, state);
        return;
    }
    if (words[0] == "cylvelocity") {
        auto state = randomState(words.at(1));
        const auto radial = variable(words.at(2), words.at(3), words.at(4));
        const auto normal = variable(words.at(5), words.at(6), words.at(7));
        printVector(cylindricalVelocity(radial, normal, state), state);
        return;
    }
    if (words[0] == "pointvelocity") {
        auto state = randomState(words.at(1));
        const auto bytes = fromHex(words.at(2));
        if (bytes.size() != 0x2CU) {
            throw std::runtime_error("point velocity data size");
        }
        std::array<std::uint8_t, 0x2C> data{};
        std::memcpy(data.data(), bytes.data(), data.size());
        const VolumeModuleView module{0, 0, data.data()};
        printVector(pointVelocity(module, state), state);
        return;
    }
    if (words[0] == "runtimeinit") {
        const auto source = fromHex(words.at(2));
        if (source.size() != 0x20U) {
            throw std::runtime_error("runtime source size");
        }
        EmitterModuleRuntimeData destination{};
        const auto kind = number(words.at(1));
        if (kind == 5) {
            initializeCylindricalVelocityRuntime(destination, source.data());
        } else if (kind == 1) {
            initializeLineVolumeRuntime(destination, source.data());
        } else if (kind == 2) {
            initializeSplineVolumeRuntime(destination, source.data());
        } else {
            throw std::runtime_error("runtime kind");
        }
        printBytes(&destination, sizeof(destination));
        std::cout << '\n';
        return;
    }
    if (words[0] == "alphainit") {
        const auto source = fromHex(words.at(1));
        if (source.size() != 0x98U) {
            throw std::runtime_error("alpha source size");
        }
        std::array<std::uint8_t, 0x98> destination{};
        initializeDefaultAlphaTemplate(destination.data(), source.data());
        printBytes(destination.data(), destination.size());
        std::cout << '\n';
        return;
    }
    if (words[0] == "boxinit") {
        const auto source = fromHex(words.at(1));
        if (source.size() != 0x24U) {
            throw std::runtime_error("box source size");
        }
        BoxVolumeRuntimeData destination{};
        initializeBoxVolumeRuntime(destination, source.data());
        printBytes(&destination, sizeof(destination));
        std::cout << '\n';
        return;
    }
    if (words[0] == "boxdims") {
        const auto source = fromHex(words.at(1));
        if (source.size() != 0x24U) {
            throw std::runtime_error("box data size");
        }
        const VolumeModuleView module{0, 0, source.data()};
        float output[2]{};
        boxVolumeDebugDimensions(module, output);
        printBytes(output, sizeof(output));
        std::cout << '\n';
        return;
    }
    if (words[0] == "colorrange") {
        std::array<float, 6> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] = floatValue(words.at(index + 1));
        }
        const Vector3 candidate{
            floatValue(words.at(7)), floatValue(words.at(8)), floatValue(words.at(9))};
        const bool result =
            colorWithinRange(values.data(), candidate, floatValue(words.at(10)));
        std::cout << (result ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "pointinit") {
        const auto source = fromHex(words.at(1));
        if (source.size() != 0x28U) {
            throw std::runtime_error("point source size");
        }
        PointVelocityTemplateData destination{};
        initializePointVelocityTemplate(destination, source.data());
        printBytes(&destination, sizeof(destination));
        std::cout << '\n';
        return;
    }
    if (words[0] == "updateptr") {
        std::array<std::uint8_t, 0x80> data{};
        VolumeModuleView module{0, 0, data.data()};
        const auto offset = number(words.at(1));
        std::uint8_t* pointer = offset == 0x44
            ? defaultUpdateLifetimeData(module)
            : (offset == 0x50 ? defaultUpdateSizeData(module)
                              : defaultUpdateRotationData(module));
        std::cout << (pointer - data.data()) << '\n';
        return;
    }
    if (words[0] == "shuffle") {
        auto state = randomState(words.at(1));
        std::vector<std::uint32_t> values;
        values.reserve(words.size() - 2U);
        for (std::size_t index = 2; index < words.size(); ++index) {
            values.push_back(number(words[index]));
        }
        shuffleDwords(values.data(), values.data() + values.size(), state);
        if (values.empty()) {
            std::cout << '.';
        } else {
            printBytes(values.data(), values.size() * sizeof(std::uint32_t));
        }
        std::cout << ' ';
        printBytes(&state, sizeof(state));
        std::cout << '\n';
        return;
    }
    if (words[0] == "gpu") {
        const auto bytes = fromHex(words.at(1));
        if (bytes.size() != sizeof(GpuDrawDataView)) {
            throw std::runtime_error("gpu data size");
        }
        GpuDrawDataView data{};
        std::memcpy(&data, bytes.data(), bytes.size());
        const GpuDrawModuleView module{0, 0, &data};
        std::array<std::uint32_t, 7> scalar{
            gpuFramesPerRow(module), gpuTotalFrames(module),
            0, gpuSingleRow(module), gpuSortParticles(module),
            gpuMiscWord(module), gpuEnableSoftParticles(module)};
        const float speed = gpuSpeedMultiplier(module);
        std::memcpy(&scalar[2], &speed, sizeof(scalar[2]));
        const auto dataPointer = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&data));
        if ((scalar[6] & 0xFFFFFF00U) != (dataPointer & 0xFFFFFF00U)) {
            throw std::runtime_error("gpu AL return convention");
        }
        scalar[6] &= 0xFFU;
        float misc[4]{};
        gpuMiscValues(module, misc);
        printBytes(scalar.data(), scalar.size() * sizeof(scalar[0]));
        printBytes(misc, sizeof(misc));
        std::cout << '\n';
        return;
    }
    if (words[0] == "names") {
        std::cout << boxVolumeTypeName() << '|'
                  << cylindricalVelocityTypeName() << '|'
                  << lineVolumeTypeName() << '|'
                  << pointVolumeTypeName() << '|'
                  << splineVolumeTypeName() << '|'
                  << defaultUpdateTypeName() << '|'
                  << defaultUpdateParticleTypeName() << '|'
                  << gpuDrawTypeName() << '|'
                  << windTypeName() << '|'
                  << defaultAlphaTemplateTypeName() << '|'
                  << windTemplateTypeName() << '|'
                  << defaultAlphaParticleTypeName() << '|'
                  << defaultColorParticleTypeName() << '\n';
        return;
    }
    throw std::runtime_error("unknown command");
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
            std::istringstream input(line);
            std::vector<std::string> words;
            for (std::string word; input >> word;) {
                words.push_back(std::move(word));
            }
            run(words);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
