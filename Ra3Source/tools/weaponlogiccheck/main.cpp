#include <ra3/gamelogic/WeaponLogic.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ra3::gamelogic::Matrix3x4;
using ra3::gamelogic::PhysicsOverlapNode;
using ra3::gamelogic::Sphere;

bool g_mutated = false;

template <typename T>
T fromBits(std::uint32_t bits)
{
    static_assert(sizeof(T) == sizeof(bits), "expected a four-byte type");
    T result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::uint32_t floatBits(float value)
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::vector<std::uint8_t> parseHex(const std::string& text)
{
    if ((text.size() & 1u) != 0u) {
        return {};
    }
    std::vector<std::uint8_t> result(text.size() / 2u);
    for (std::size_t index = 0; index != result.size(); ++index) {
        unsigned value = 0;
        std::istringstream stream(text.substr(index * 2u, 2u));
        stream >> std::hex >> value;
        if (!stream || value > 0xFFu) {
            return {};
        }
        result[index] = static_cast<std::uint8_t>(value);
    }
    return result;
}

std::string toHex(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index != size; ++index) {
        stream << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return stream.str();
}

std::string extendedHex(float value)
{
    std::array<std::uint8_t, 10> extended{};
#if defined(_MSC_VER) && defined(_M_IX86)
    auto* output = extended.data();
    __asm {
        mov eax, output
        fld value
        fstp tbyte ptr [eax]
    }
#else
    const long double converted = value;
    std::memcpy(extended.data(), &converted, std::min(extended.size(), sizeof(converted)));
#endif
    return toHex(extended.data(), extended.size());
}

template <typename T, std::size_t N>
bool copyArray(const std::vector<std::uint8_t>& bytes, std::array<T, N>& output)
{
    if (bytes.size() != sizeof(output)) {
        return false;
    }
    std::memcpy(output.data(), bytes.data(), bytes.size());
    return true;
}

template <typename T, std::size_t N>
bool copyArray(const std::vector<std::uint8_t>& bytes, T (&output)[N])
{
    if (bytes.size() != sizeof(output)) {
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    return true;
}

std::uint32_t maybeMutate(std::uint32_t value)
{
    return g_mutated ? value ^ 1u : value;
}

bool selfTest()
{
    std::size_t count = 0;
    const auto* metadata = ra3::gamelogic::moduleMetadata(count);
    if (metadata == nullptr || count != 30
        || ra3::gamelogic::findModuleMetadata(0x0069F850u) == nullptr
        || ra3::gamelogic::weaponUsesLineOfSight(1u << 17u) != 1u
        || ra3::gamelogic::objectStatusBit9(1u << 9u) != 1u
        || ra3::gamelogic::slavedConfigurationBit22(1u << 22u) != 1u) {
        return false;
    }
    ra3::gamelogic::ModuleKeyState state{0xA4u, 0x11223344u};
    const auto first = ra3::gamelogic::resolveModuleKey(metadata[0], state, 0x55667788u);
    const auto second = ra3::gamelogic::resolveModuleKey(metadata[0], state, 0xDEADBEEFu);
    return first.resolverCalled && first.value == 0x55667788u
        && state.guard == 0xA5u && state.cachedKey == 0x55667788u
        && !second.resolverCalled && second.value == 0x55667788u;
}

bool process(const std::string& line)
{
    std::istringstream input(line);
    std::string command;
    input >> command;
    if (command == "quit") {
        return false;
    }
    if (command == "mutate") {
        unsigned enabled = 0;
        input >> enabled;
        g_mutated = enabled != 0;
        std::cout << "ok\n";
        return true;
    }
    if (command == "fpcw") {
        unsigned value = 0;
        input >> value;
#if defined(_MSC_VER) && defined(_M_IX86)
        const auto controlWord = static_cast<std::uint16_t>(value);
        __asm {
            fldcw controlWord
        }
#else
        (void)value;
#endif
        std::cout << "ok\n";
        return true;
    }
    if (command == "metadata_name") {
        std::size_t index = 0;
        input >> index;
        std::size_t count = 0;
        const auto* entries = ra3::gamelogic::moduleMetadata(count);
        if (!input || index >= count) {
            std::cout << "error\n";
            return true;
        }
        std::cout << maybeMutate(ra3::gamelogic::moduleNameAddress(entries[index])) << '\n';
        return true;
    }
    if (command == "metadata_key") {
        std::size_t index = 0;
        std::uint32_t guard = 0;
        std::uint32_t cached = 0;
        std::uint32_t resolver = 0;
        input >> index >> guard >> cached >> resolver;
        std::size_t count = 0;
        const auto* entries = ra3::gamelogic::moduleMetadata(count);
        if (!input || index >= count) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::ModuleKeyState state{guard, cached};
        auto result = ra3::gamelogic::resolveModuleKey(entries[index], state, resolver);
        result.value = maybeMutate(result.value);
        std::cout << result.value << ' ' << state.guard << ' ' << state.cachedKey
                  << ' ' << (result.resolverCalled ? 1 : 0) << '\n';
        return true;
    }
    if (command == "target_player") {
        std::uint32_t templateObject = 0;
        std::uint32_t flags = 0;
        std::uint32_t sourceObject = 0;
        std::uint32_t sourcePlayer = 0;
        std::uint32_t candidatePlayer = 0;
        input >> templateObject >> flags >> sourceObject >> sourcePlayer >> candidatePlayer;
        std::cout << maybeMutate(ra3::gamelogic::rejectDifferentControllingPlayer(
            templateObject, flags, sourceObject, sourcePlayer, candidatePlayer)) << '\n';
        return true;
    }
    if (command == "kind_mask") {
        std::string exclusionText;
        std::string sourceText;
        std::string requirementText;
        std::string candidateText;
        input >> exclusionText >> sourceText >> requirementText >> candidateText;
        std::array<std::uint32_t, 7> exclusions{};
        std::array<std::uint32_t, 7> source{};
        std::array<std::uint32_t, 7> requirements{};
        std::array<std::uint32_t, 7> candidate{};
        if (!copyArray(parseHex(exclusionText), exclusions)
            || !copyArray(parseHex(sourceText), source)
            || !copyArray(parseHex(requirementText), requirements)
            || !copyArray(parseHex(candidateText), candidate)) {
            std::cout << "error\n";
            return true;
        }
        std::cout << maybeMutate(ra3::gamelogic::weaponKindMaskMatches(
            exclusions.data(), source.data(), requirements.data(), candidate.data())) << '\n';
        return true;
    }
    if (command == "los") {
        std::uint32_t flags = 0;
        input >> flags;
        std::cout << maybeMutate(ra3::gamelogic::weaponUsesLineOfSight(flags)) << '\n';
        return true;
    }
    if (command == "charge") {
        std::uint32_t current = 0;
        std::uint32_t start = 0;
        std::uint32_t frameBits = 0;
        input >> current >> start >> frameBits;
        float result = ra3::gamelogic::chargeWeaponProgress(
            current, start, fromBits<float>(frameBits));
        if (g_mutated) {
            result = result == 0.0f ? 1.0f : 0.0f;
        }
        std::cout << floatBits(result) << ' ' << extendedHex(result) << '\n';
        return true;
    }
    if (command == "cluster") {
        std::uint32_t flags = 0;
        std::string idsText;
        std::uint32_t idsAddress = 0;
        std::uint32_t target = 0;
        unsigned hasPosition = 0;
        unsigned allowed = 0;
        input >> flags >> idsText >> idsAddress >> target >> hasPosition >> allowed;
        const auto bytes = parseHex(idsText);
        if ((bytes.size() & 3u) != 0u) {
            std::cout << "error\n";
            return true;
        }
        std::vector<std::uint32_t> ids(bytes.size() / 4u);
        std::memcpy(ids.data(), bytes.data(), bytes.size());
        const auto result = ra3::gamelogic::clusterBombAcceptsTarget(
            flags, ids.data(), ids.size(), idsAddress, target,
            hasPosition != 0, allowed != 0);
        std::cout << maybeMutate(result) << '\n';
        return true;
    }
    if (command == "transform") {
        unsigned enabled = 0;
        std::string pointsText;
        std::string matrixText;
        std::string outputText;
        input >> enabled >> pointsText >> matrixText >> outputText;
        std::array<float, 6> points{};
        Matrix3x4 matrix{};
        std::array<float, 6> output{};
        if (!copyArray(parseHex(pointsText), points)
            || !copyArray(parseHex(matrixText), matrix.value)
            || !copyArray(parseHex(outputText), output)) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::transformDestructiblePoints(
            enabled != 0, points.data(), matrix, output.data());
        if (g_mutated) {
            reinterpret_cast<std::uint8_t*>(output.data())[0] ^= 1u;
        }
        std::cout << toHex(output.data(), sizeof(output)) << '\n';
        return true;
    }
    if (command == "transform_alias") {
        std::string pointsText;
        std::string matrixText;
        input >> pointsText >> matrixText;
        std::array<float, 6> points{};
        Matrix3x4 matrix{};
        if (!copyArray(parseHex(pointsText), points)
            || !copyArray(parseHex(matrixText), matrix.value)) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::transformDestructiblePoints(
            true, points.data(), matrix, points.data());
        if (g_mutated) {
            reinterpret_cast<std::uint8_t*>(points.data())[0] ^= 1u;
        }
        std::cout << toHex(points.data(), sizeof(points)) << '\n';
        return true;
    }
    if (command == "freefall") {
        std::string objectsText;
        std::string idsText;
        std::uint32_t noMatchValue = 0;
        std::uint32_t candidate = 0;
        input >> objectsText >> idsText >> noMatchValue >> candidate;
        const auto objectBytes = parseHex(objectsText);
        const auto idBytes = parseHex(idsText);
        if (objectBytes.size() != idBytes.size() || (idBytes.size() & 3u) != 0u) {
            std::cout << "error\n";
            return true;
        }
        std::vector<std::uint32_t> objects(objectBytes.size() / 4u);
        std::vector<std::uint32_t> ids(idBytes.size() / 4u);
        std::memcpy(objects.data(), objectBytes.data(), objectBytes.size());
        std::memcpy(ids.data(), idBytes.data(), idBytes.size());
        std::cout << maybeMutate(ra3::gamelogic::freefallContainsTarget(
            objects.data(), ids.data(), ids.size(), noMatchValue, candidate)) << '\n';
        return true;
    }
    if (command == "lifetime_secondary") {
        unsigned minimum = 0;
        unsigned maximum = 0;
        std::uint32_t delay = 0;
        std::uint32_t tick = 0;
        unsigned hasLimit = 0;
        std::uint32_t limit = 0;
        std::uint32_t current = 0;
        std::uint32_t finalFrame = 0;
        std::uint32_t fps = 0;
        std::uint32_t speed = 0;
        input >> minimum >> maximum >> delay >> tick >> hasLimit >> limit
              >> current >> finalFrame >> fps >> speed;
        const ra3::gamelogic::LifetimeConfig config{
            minimum != 0, maximum != 0, fromBits<float>(delay), fromBits<float>(tick),
            hasLimit != 0, limit};
        std::cout << maybeMutate(ra3::gamelogic::lifetimeSecondaryDeadline(
            config, current, finalFrame, fromBits<float>(fps), fromBits<float>(speed)))
                  << '\n';
        return true;
    }
    if (command == "lifetime_start") {
        std::uint32_t current = 0;
        std::uint32_t sampled = 0;
        std::uint32_t secondary = 0;
        input >> current >> sampled >> secondary;
        std::cout << maybeMutate(
            ra3::gamelogic::lifetimeStart(current, sampled, secondary)) << '\n';
        return true;
    }
    if (command == "lifetime_pause") {
        unsigned pausedValue = 0;
        std::uint32_t current = 0;
        input >> pausedValue >> current;
        auto paused = static_cast<std::uint8_t>(pausedValue);
        const auto result = ra3::gamelogic::lifetimePause(paused, current);
        std::cout << maybeMutate(result) << ' ' << static_cast<unsigned>(paused) << '\n';
        return true;
    }
    if (command == "lifetime_resume") {
        unsigned pausedValue = 0;
        std::uint32_t current = 0;
        std::uint32_t finalFrame = 0;
        std::uint32_t secondary = 0;
        input >> pausedValue >> current >> finalFrame >> secondary;
        auto paused = static_cast<std::uint8_t>(pausedValue);
        const auto result =
            ra3::gamelogic::lifetimeResume(paused, current, finalFrame, secondary);
        std::cout << maybeMutate(result) << ' ' << static_cast<unsigned>(paused) << '\n';
        return true;
    }
    if (command == "lose_health") {
        std::uint32_t accumulated = 0;
        std::uint32_t perPulse = 0;
        std::uint32_t requested = 0;
        input >> accumulated >> perPulse >> requested;
        std::cout << maybeMutate(ra3::gamelogic::loseHealthPulseCount(
            fromBits<float>(accumulated), fromBits<float>(perPulse), requested)) << '\n';
        return true;
    }
    if (command == "collision_set" || command == "collision_clear") {
        unsigned flagValue = 0;
        std::uint32_t objectMask = 0;
        std::uint32_t acceptedMask = 0;
        input >> flagValue >> objectMask >> acceptedMask;
        auto flag = static_cast<std::uint8_t>(flagValue);
        if (command == "collision_set") {
            ra3::gamelogic::physicsSetCollisionFlag(flag, objectMask, acceptedMask);
        } else {
            ra3::gamelogic::physicsClearCollisionFlag(flag, objectMask, acceptedMask);
        }
        std::cout << maybeMutate(static_cast<std::uint32_t>(flag)) << '\n';
        return true;
    }
    if (command == "capture_transforms") {
        std::string objectText;
        std::string contactText;
        input >> objectText >> contactText;
        Matrix3x4 object{};
        Matrix3x4 contact{};
        Matrix3x4 savedObject{};
        Matrix3x4 savedContact{};
        std::uint8_t valid = 0;
        if (!copyArray(parseHex(objectText), object.value)
            || !copyArray(parseHex(contactText), contact.value)) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::physicsCaptureTransforms(
            object, contact, savedObject, savedContact, valid);
        if (g_mutated) {
            reinterpret_cast<std::uint8_t*>(savedObject.value)[0] ^= 1u;
        }
        std::cout << toHex(savedObject.value, sizeof(savedObject.value)) << ' '
                  << toHex(savedContact.value, sizeof(savedContact.value)) << ' '
                  << static_cast<unsigned>(valid) << '\n';
        return true;
    }
    if (command == "scale_group") {
        std::int32_t count = 0;
        std::int32_t source = 0;
        input >> count >> source;
        float inverse = 0;
        float scaled = 0;
        ra3::gamelogic::physicsScaleForGroup(count, source, inverse, scaled);
        std::cout << maybeMutate(floatBits(inverse)) << ' ' << floatBits(scaled) << '\n';
        return true;
    }
    if (command == "status9") {
        std::uint32_t status = 0;
        input >> status;
        std::cout << maybeMutate(ra3::gamelogic::objectStatusBit9(status)) << '\n';
        return true;
    }
    if (command == "copy_player") {
        std::uint32_t id = 0;
        std::string initialText;
        input >> id >> initialText;
        auto physics = parseHex(initialText);
        if (physics.size() != 0x130u) {
            std::cout << "error\n";
            return true;
        }
        std::array<std::uint8_t, 0x41C> object{};
        std::array<std::uint8_t, 0x24> controller{};
        const void* controllerPointer = controller.data();
        const void* objectPointer = object.data();
        std::memcpy(controller.data() + 0x20, &id, sizeof(id));
        std::memcpy(object.data() + 0x418, &controllerPointer, sizeof(controllerPointer));
        std::memcpy(physics.data() + 0x08, &objectPointer, sizeof(objectPointer));
        ra3::gamelogic::physicsCopyControllingPlayerId(physics.data());
        if (g_mutated) {
            physics[0x12C] ^= 1u;
        }
        std::cout << toHex(physics.data(), physics.size()) << '\n';
        return true;
    }
    if (command == "count_overlaps") {
        std::uint32_t owner = 0;
        std::string ownersText;
        std::string ignoredText;
        input >> owner >> ownersText >> ignoredText;
        const auto ownerBytes = parseHex(ownersText);
        const auto ignoredBytes = parseHex(ignoredText);
        if ((ownerBytes.size() & 3u) != 0u
            || ignoredBytes.size() != ownerBytes.size() / 4u) {
            std::cout << "error\n";
            return true;
        }
        std::vector<PhysicsOverlapNode> nodes(ignoredBytes.size());
        for (std::size_t index = 0; index != nodes.size(); ++index) {
            std::memcpy(&nodes[index].ownerValue, ownerBytes.data() + index * 4u, 4u);
            nodes[index].ignored = ignoredBytes[index];
            nodes[index].next = index + 1u == nodes.size() ? nullptr : &nodes[index + 1u];
        }
        const auto result = ra3::gamelogic::physicsCountOverlaps(
            nodes.empty() ? nullptr : nodes.data(), owner);
        std::cout << maybeMutate(static_cast<std::uint32_t>(result)) << '\n';
        return true;
    }
    if (command == "controller_height") {
        unsigned object = 0;
        unsigned controller = 0;
        std::uint32_t objectValue = 0;
        std::uint32_t heightObjectValue = 0;
        std::uint32_t height = 0;
        input >> object >> controller >> objectValue >> heightObjectValue >> height;
        std::cout << maybeMutate(ra3::gamelogic::physicsHasControllerAtHeight(
            objectValue, object != 0, controller != 0,
            heightObjectValue, fromBits<float>(height))) << '\n';
        return true;
    }
    if (command == "power_wake") {
        unsigned powered = 0;
        std::uint32_t frame = 0;
        input >> powered >> frame;
        std::cout << maybeMutate(
            ra3::gamelogic::powerUpdateWakeFrame(powered != 0, frame)) << '\n';
        return true;
    }
    if (command == "laser") {
        std::uint32_t delay = 0;
        std::uint32_t fps = 0;
        std::uint32_t speed = 0;
        std::uint32_t current = 0;
        unsigned activeValue = 0;
        unsigned finishedValue = 0;
        input >> delay >> fps >> speed >> current >> activeValue >> finishedValue;
        auto active = static_cast<std::uint8_t>(activeValue);
        auto finished = static_cast<std::uint8_t>(finishedValue);
        const auto result = ra3::gamelogic::laserScatterInitialize(
            fromBits<float>(delay), fromBits<float>(fps), fromBits<float>(speed),
            current, active, finished);
        std::cout << maybeMutate(result) << ' ' << static_cast<unsigned>(active)
                  << ' ' << static_cast<unsigned>(finished) << '\n';
        return true;
    }
    if (command == "shield") {
        std::string sphereText;
        std::string startText;
        std::string endText;
        std::string outputText;
        std::uint32_t startAddress = 0;
        std::uint32_t outputAddress = 0;
        input >> sphereText >> startText >> endText >> outputText
              >> startAddress >> outputAddress;
        Sphere sphere{};
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        std::array<float, 3> output{};
        const auto sphereBytes = parseHex(sphereText);
        if (sphereBytes.size() != sizeof(sphere)
            || !copyArray(parseHex(startText), start)
            || !copyArray(parseHex(endText), end)
            || !copyArray(parseHex(outputText), output)) {
            std::cout << "error\n";
            return true;
        }
        std::memcpy(&sphere, sphereBytes.data(), sphereBytes.size());
        auto result = ra3::gamelogic::shieldSegmentIntersection(
            sphere, start.data(), end.data(), output.data(),
            startAddress, outputAddress);
        if (g_mutated) {
            output[0] = output[0] + 1.0f;
        }
        std::cout << maybeMutate(result) << ' '
                  << toHex(output.data(), sizeof(output)) << '\n';
        return true;
    }
    if (command == "shield_alias_start") {
        std::string sphereText;
        std::string startText;
        std::string endText;
        std::uint32_t address = 0;
        input >> sphereText >> startText >> endText >> address;
        Sphere sphere{};
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        const auto sphereBytes = parseHex(sphereText);
        if (sphereBytes.size() != sizeof(sphere)
            || !copyArray(parseHex(startText), start)
            || !copyArray(parseHex(endText), end)) {
            std::cout << "error\n";
            return true;
        }
        std::memcpy(&sphere, sphereBytes.data(), sphereBytes.size());
        auto result = ra3::gamelogic::shieldSegmentIntersection(
            sphere, start.data(), end.data(), start.data(), address, address);
        if (g_mutated) {
            start[0] = start[0] + 1.0f;
        }
        std::cout << maybeMutate(result) << ' '
                  << toHex(start.data(), sizeof(start)) << '\n';
        return true;
    }
    if (command == "clear_links") {
        std::string objectText;
        input >> objectText;
        auto object = parseHex(objectText);
        if (object.size() != 0x12F8u) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::clearObjectSlaveLinks(object.data());
        if (g_mutated) {
            object[0x34C] ^= 1u;
        }
        std::cout << toHex(object.data(), object.size()) << '\n';
        return true;
    }
    if (command == "pinned") {
        std::string masterText;
        std::string offsetText;
        std::string outputText;
        unsigned hasOffset = 0;
        input >> masterText >> offsetText >> outputText >> hasOffset;
        std::array<float, 3> master{};
        std::array<float, 3> offset{};
        std::array<float, 3> output{};
        if (!copyArray(parseHex(masterText), master)
            || !copyArray(parseHex(offsetText), offset)
            || !copyArray(parseHex(outputText), output)) {
            std::cout << "error\n";
            return true;
        }
        ra3::gamelogic::slavedPinnedPosition(
            master.data(), hasOffset != 0 ? offset.data() : nullptr, output.data());
        if (g_mutated) {
            reinterpret_cast<std::uint8_t*>(output.data())[0] ^= 1u;
        }
        std::cout << toHex(output.data(), sizeof(output)) << '\n';
        return true;
    }
    if (command == "pinned_alias_master"
        || command == "pinned_alias_offset") {
        std::string masterText;
        std::string offsetText;
        input >> masterText >> offsetText;
        std::array<float, 3> master{};
        std::array<float, 3> offset{};
        if (!copyArray(parseHex(masterText), master)
            || !copyArray(parseHex(offsetText), offset)) {
            std::cout << "error\n";
            return true;
        }
        float* output = command == "pinned_alias_master"
            ? master.data()
            : offset.data();
        ra3::gamelogic::slavedPinnedPosition(
            master.data(), offset.data(), output);
        if (g_mutated) {
            reinterpret_cast<std::uint8_t*>(output)[0] ^= 1u;
        }
        std::cout << toHex(output, sizeof(master)) << '\n';
        return true;
    }
    if (command == "guard_area") {
        std::string pointsText;
        std::string positionText;
        std::uint32_t radius = 0;
        std::uint32_t address = 0;
        input >> pointsText >> positionText >> radius >> address;
        const auto pointBytes = parseHex(pointsText);
        std::array<float, 3> position{};
        if ((pointBytes.size() % 12u) != 0u
            || !copyArray(parseHex(positionText), position)) {
            std::cout << "error\n";
            return true;
        }
        std::vector<float> points(pointBytes.size() / 4u);
        std::memcpy(points.data(), pointBytes.data(), pointBytes.size());
        std::cout << maybeMutate(ra3::gamelogic::slavedPointOutsideGuardArea(
            points.data(), points.size() / 3u, position.data(),
            fromBits<float>(radius), address)) << '\n';
        return true;
    }
    if (command == "slaved_state") {
        std::uint32_t configuration = 0;
        std::uint32_t statusA = 0;
        std::uint32_t statusB = 0;
        std::uint32_t statusC = 0;
        input >> configuration >> statusA >> statusB >> statusC;
        std::cout << maybeMutate(ra3::gamelogic::slavedStateAllowed(
            configuration, statusA, statusB, statusC)) << '\n';
        return true;
    }
    if (command == "config22") {
        std::uint32_t configuration = 0;
        input >> configuration;
        std::cout << maybeMutate(
            ra3::gamelogic::slavedConfigurationBit22(configuration)) << '\n';
        return true;
    }
    if (command == "rider") {
        std::uint32_t objectValue = 0;
        unsigned hasModule = 0;
        std::uint32_t configurationObjectValue = 0;
        std::uint32_t configuration = 0;
        unsigned configuredValue = 0;
        input >> objectValue >> hasModule >> configurationObjectValue
              >> configuration >> configuredValue;
        std::cout << maybeMutate(ra3::gamelogic::slavedHasRider(
            objectValue, hasModule != 0, configurationObjectValue, configuration,
            static_cast<std::uint8_t>(configuredValue))) << '\n';
        return true;
    }
    if (command == "copy_flags") {
        std::uint32_t flags = 0;
        std::uint32_t output = 0;
        input >> flags >> output;
        ra3::gamelogic::slavedCopyTemplateFlags(flags, output);
        std::cout << maybeMutate(output) << '\n';
        return true;
    }
    std::cout << "error\n";
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
        if (!selfTest()) {
            std::cerr << "weaponlogiccheck self-test failed\n";
            return 1;
        }
        std::cout << "weaponlogiccheck self-test passed\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--mutated") {
        g_mutated = true;
    } else if (argc != 1) {
        std::cerr << "usage: ra3_weaponlogiccheck [--self-test|--mutated]\n";
        return 2;
    }
    std::string line;
    while (std::getline(std::cin, line) && process(line)) {
        std::cout.flush();
    }
    return 0;
}
