#include <ra3/common/ByteString.h>
#include <ra3/common/Crc32.h>
#include <ra3/math/Math.h>
#include <ra3/sim/Random.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> fromHex(const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    if (text == "-") {
        return bytes;
    }
    bytes.reserve(text.size() / 2 + 1);
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

void printHex(const std::uint8_t* bytes, std::size_t size)
{
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << '\n';
}

const char* textPointer(const std::vector<std::uint8_t>& bytes, const std::string& encoded)
{
    return encoded == "-" ? nullptr : reinterpret_cast<const char*>(bytes.data());
}

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

float floatFromBits(const std::string& text)
{
    const std::uint32_t bits = number(text);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void printFloat(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::cout << bits << '\n';
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

#if defined(_MSC_VER) && defined(_M_IX86)
using ExtendedVectorPairFunction =
    float(__cdecl*)(const ra3::math::Vector3&, const ra3::math::Vector3&) noexcept;
using ExtendedVectorFunction =
    float(__cdecl*)(const ra3::math::Vector3*) noexcept;
using ExtendedOffsetDistanceFunction = float(__fastcall*)(
    const void*, void*, const ra3::math::Vector3*) noexcept;

ExtendedVectorPairFunction squaredDistanceExtended = &ra3::math::squaredDistance;
ExtendedVectorPairFunction distanceExtended = &ra3::math::distance;
ExtendedVectorFunction approximateLengthExtended = &ra3::math::approximateLengthExtended;
ExtendedOffsetDistanceFunction offsetDistanceExtended =
    &ra3::math::squaredDistanceFromOffset38Extended;

void printExtendedVectorPair(
    ExtendedVectorPairFunction function,
    const ra3::math::Vector3* a,
    const ra3::math::Vector3* b)
{
    std::uint8_t bytes[10]{};
    __asm {
        push b
        push a
        call function
        add esp, 8
        fstp tbyte ptr [bytes]
    }
    printHex(bytes, sizeof(bytes));
}

void printExtendedVector(
    ExtendedVectorFunction function, const ra3::math::Vector3* value)
{
    std::uint8_t bytes[10]{};
    __asm {
        push value
        call function
        add esp, 4
        fstp tbyte ptr [bytes]
    }
    printHex(bytes, sizeof(bytes));
}

using RandomRealFunction = float(__cdecl*)(ra3::sim::RandomState*, float, float) noexcept;
RandomRealFunction randomRealExtended = &ra3::sim::randomRealExtended;

// Prints the 80-bit ST0, its single-precision store and the post-state.
void printRandomRealExtended(ra3::sim::RandomState* state, float lower, float upper)
{
    std::uint8_t bytes[10]{};
    std::uint32_t rounded = 0;
    __asm {
        push upper
        push lower
        push state
        call randomRealExtended
        add esp, 12
        fst dword ptr [rounded]
        fstp tbyte ptr [bytes]
    }
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        std::cout << std::setw(2) << static_cast<unsigned>(byte);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << ' ' << rounded << ' ';
    printHex(reinterpret_cast<const std::uint8_t*>(state), sizeof(*state));
}

void printExtendedOffsetDistance(
    ExtendedOffsetDistanceFunction function,
    const void* object,
    const ra3::math::Vector3* point)
{
    std::uint8_t bytes[10]{};
    __asm {
        push point
        xor edx, edx
        mov ecx, object
        call function
        fstp tbyte ptr [bytes]
    }
    printHex(bytes, sizeof(bytes));
}
#endif

template <typename T>
T objectFromBytes(const std::vector<std::uint8_t>& bytes)
{
    T value{};
    if (bytes.size() != sizeof(T)) {
        throw std::runtime_error("incorrect object size");
    }
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

ra3::sim::RandomState randomState(const std::vector<std::uint8_t>& bytes)
{
    ra3::sim::RandomState state{};
    if (bytes.size() != sizeof(state)) {
        throw std::invalid_argument("random state must contain six dwords");
    }
    std::memcpy(&state, bytes.data(), sizeof(state));
    return state;
}

void printRandom(std::uint32_t result, const ra3::sim::RandomState& state)
{
    std::cout << result << ' ';
    printHex(reinterpret_cast<const std::uint8_t*>(&state), sizeof(state));
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string op;
        in >> op;
        if (op == "quit") {
            break;
        }

        std::string firstText;
        std::string secondText;
        std::string thirdText;
        in >> firstText >> secondText >> thirdText;
        const auto first = fromHex(firstText);
        const auto second = fromHex(secondText);
        const char* const firstPtr = textPointer(first, firstText);
        const char* const secondPtr = textPointer(second, secondText);

        if (op == "fpcw") {
            setFpuControlWord(static_cast<std::uint16_t>(number(firstText)));
            std::cout << "ok\n";
        } else if (op == "copylim") {
            const std::uint32_t count = number(secondText);
            std::vector<std::uint8_t> output(static_cast<std::size_t>(count) + 2, 0xA5);
            char* destination = firstText == "-" ? nullptr : reinterpret_cast<char*>(output.data());
            ra3::common::copyStringLimited(destination, firstPtr, count);
            printHex(output.data(), output.size());
        } else if (op == "appendlim") {
            const std::uint32_t count = number(thirdText);
            const std::size_t capacity = first.size() + count + 1;
            std::vector<std::uint8_t> output(capacity, 0xA5);
            std::copy(first.begin(), first.end(), output.begin());
            ra3::common::appendStringLimited(
                reinterpret_cast<char*>(output.data()), secondPtr, count);
            printHex(output.data(), output.size());
        } else if (op == "ncmp" || op == "nicmp") {
            const std::uint32_t count = number(thirdText);
            const int result = op == "ncmp"
                ? ra3::common::compareStringsLimited(firstPtr, secondPtr, count)
                : ra3::common::compareStringsCaseInsensitiveLimited(firstPtr, secondPtr, count);
            std::cout << result << '\n';
        } else if (op == "copy" || op == "lower") {
            std::vector<std::uint8_t> output(first.size() + 2, 0xA5);
            if (op == "copy") {
                ra3::common::copyString(reinterpret_cast<char*>(output.data()), firstPtr);
            } else {
                ra3::common::copyStringLowercase(reinterpret_cast<char*>(output.data()), firstPtr);
            }
            printHex(output.data(), output.size());
        } else if (op == "cmp") {
            std::cout << ra3::common::compareStrings(firstPtr, secondPtr) << '\n';
        } else if (op == "len") {
            std::cout << ra3::common::stringLength(firstPtr) << '\n';
        } else if (op == "rchr") {
            const auto* result = ra3::common::findLastCharacter(
                firstPtr, static_cast<std::uint8_t>(number(secondText)));
            std::cout << (result == nullptr ? -1 : static_cast<int>(result - firstPtr)) << '\n';
        } else if (op == "widen") {
            std::vector<std::uint16_t> output(first.size() + 1, 0xA5A5);
            ra3::common::widenBytes(output.data(), firstPtr);
            printHex(reinterpret_cast<const std::uint8_t*>(output.data()), output.size() * 2);
        } else if (op == "bytes") {
            std::cout << ra3::common::compareBytes(first.data(), second.data(), number(thirdText))
                      << '\n';
        } else if (op == "pcstr") {
            std::cout << ra3::common::compareLengthPrefixedToCString(
                firstPtr, static_cast<std::uint16_t>(number(thirdText)), secondPtr) << '\n';
        } else if (op == "pp") {
            std::string fourthText;
            in >> fourthText;
            std::cout << ra3::common::compareLengthPrefixed(
                firstPtr, static_cast<std::uint16_t>(number(thirdText)), secondPtr,
                static_cast<std::uint16_t>(number(fourthText))) << '\n';
        } else if (op == "fnv") {
            std::cout << ra3::common::hashLowercaseFnv1a(firstPtr) << '\n';
        } else if (op == "decode") {
            std::cout << ra3::common::decodeUtf8(first.data()) << '\n';
        } else if (op == "encode") {
            std::uint8_t output[4]{0xA5, 0xA5, 0xA5, 0xA5};
            ra3::common::encodeUtf8(output, number(firstText));
            printHex(output, sizeof(output));
        } else if (op == "decodeadv") {
            std::uint32_t codePoint = 0;
            const auto* end = ra3::common::decodeUtf8AndAdvance(first.data(), codePoint);
            std::cout << codePoint << ' ' << (end - first.data()) << '\n';
        } else if (op == "crc32") {
            std::cout << ra3::common::crc32CString(firstPtr) << '\n';
        } else if (op == "maximum" || op == "minimum") {
            const float a = floatFromBits(firstText);
            const float b = floatFromBits(secondText);
            printFloat(op == "maximum" ? ra3::math::maximum(a, b) : ra3::math::minimum(a, b));
        } else if (op == "clamp") {
            printFloat(ra3::math::clamp(
                floatFromBits(firstText), floatFromBits(secondText), floatFromBits(thirdText)));
        } else if (op == "equal3") {
            std::cout << ra3::math::exactlyEqual(
                objectFromBytes<ra3::math::Vector3>(first),
                objectFromBytes<ra3::math::Vector3>(second)) << '\n';
        } else if (op == "equal2range") {
            const auto* a = reinterpret_cast<const ra3::math::Vector2*>(first.data());
            const auto* b = reinterpret_cast<const ra3::math::Vector2*>(second.data());
            std::cout << ra3::math::exactlyEqualVector2Range(
                a, a + first.size() / sizeof(*a), b) << '\n';
        } else if (
            op == "distance2" || op == "distance" ||
            op == "distance2x87" || op == "distancex87") {
            const auto a = objectFromBytes<ra3::math::Vector3>(first);
            const auto b = objectFromBytes<ra3::math::Vector3>(second);
            if (op == "distance2") {
                printFloat(ra3::math::squaredDistance(a, b));
            } else if (op == "distance") {
                printFloat(ra3::math::distance(a, b));
#if defined(_MSC_VER) && defined(_M_IX86)
            } else {
                printExtendedVectorPair(
                    op == "distance2x87" ? squaredDistanceExtended : distanceExtended,
                    &a, &b);
#endif
            }
        } else if (op == "approximatelength") {
            printFloat(ra3::math::approximateLength(
                objectFromBytes<ra3::math::Vector3>(first)));
        } else if (op == "approximatelengthx87") {
#if defined(_MSC_VER) && defined(_M_IX86)
            const auto value = objectFromBytes<ra3::math::Vector3>(first);
            printExtendedVector(approximateLengthExtended, &value);
#endif
        } else if (op == "normalize2") {
            const auto input = objectFromBytes<ra3::math::Vector2>(first);
            ra3::math::Vector2 output{};
            ra3::math::normalize(input, output);
            printHex(reinterpret_cast<const std::uint8_t*>(&output), sizeof(output));
        } else if (op == "scale3") {
            const auto output = ra3::math::scaled(
                objectFromBytes<ra3::math::Vector3>(first), floatFromBits(secondText));
            printHex(reinterpret_cast<const std::uint8_t*>(&output), sizeof(output));
        } else if (op == "transform3") {
            const auto output = ra3::math::transformDirection(
                objectFromBytes<ra3::math::Matrix3x4>(first),
                objectFromBytes<ra3::math::Vector3>(second));
            printHex(reinterpret_cast<const std::uint8_t*>(&output), sizeof(output));
        } else if (op == "offsetdistance2") {
            printFloat(ra3::math::squaredDistanceFromOffset38(
                first.data(), objectFromBytes<ra3::math::Vector3>(second)));
        } else if (op == "offsetdistance2x87") {
#if defined(_MSC_VER) && defined(_M_IX86)
            const auto point = objectFromBytes<ra3::math::Vector3>(second);
            printExtendedOffsetDistance(offsetDistanceExtended, first.data(), &point);
#endif
        } else if (op == "clampoffset68") {
            std::vector<std::uint8_t> object(0x6C);
            std::memcpy(object.data() + 0x68, &first[0], sizeof(float));
            ra3::math::clampFloatAtOffset68(
                object.data(), floatFromBits(secondText), floatFromBits(thirdText));
            printHex(object.data() + 0x68, sizeof(float));
        } else if (op == "colorargb") {
            const auto output = ra3::math::colorFromArgb(number(firstText));
            printHex(reinterpret_cast<const std::uint8_t*>(&output), sizeof(output));
        } else if (op == "contains2") {
            std::cout << ra3::math::containsInclusive(
                objectFromBytes<ra3::math::Vector2>(first),
                objectFromBytes<ra3::math::Vector2>(second),
                objectFromBytes<ra3::math::Vector2>(fromHex(thirdText))) << '\n';
        } else if (op == "separate2") {
            auto a = objectFromBytes<ra3::math::Vector2>(first);
            auto b = objectFromBytes<ra3::math::Vector2>(second);
            ra3::math::separate(a, b, floatFromBits(thirdText));
            std::uint8_t output[sizeof(a) + sizeof(b)]{};
            std::memcpy(output, &a, sizeof(a));
            std::memcpy(output + sizeof(a), &b, sizeof(b));
            printHex(output, sizeof(output));
        } else if (op == "rngstep") {
            auto state = randomState(first);
            printRandom(ra3::sim::nextRandom(state), state);
        } else if (op == "rngseed") {
            ra3::sim::RandomState state{};
            ra3::sim::seedRandom(state, number(firstText));
            printHex(reinterpret_cast<const std::uint8_t*>(&state), sizeof(state));
        } else if (op == "rnglogic" || op == "rngclient") {
            auto state = randomState(first);
            const int low = std::stoi(secondText);
            const int high = std::stoi(thirdText);
            const int result = op == "rnglogic"
                ? ra3::sim::logicRandomInteger(state, low, high)
                : ra3::sim::clientRandomInteger(state, low, high);
            printRandom(static_cast<std::uint32_t>(result), state);
        } else if (op == "rngreal") {
            auto state = randomState(first);
            const float result =
                ra3::sim::randomReal(state, floatFromBits(secondText), floatFromBits(thirdText));
            std::uint32_t bits = 0;
            std::memcpy(&bits, &result, sizeof(bits));
            printRandom(bits, state);
        } else if (op == "rngrealx87") {
#if defined(_MSC_VER) && defined(_M_IX86)
            auto state = randomState(first);
            printRandomRealExtended(&state, floatFromBits(secondText), floatFromBits(thirdText));
#endif
        } else {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
