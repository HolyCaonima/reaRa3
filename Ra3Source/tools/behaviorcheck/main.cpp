#include <ra3/gamelogic/Behavior.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using ra3::gamelogic::ContainedObject;
using ra3::gamelogic::FireWeaponStateInput;
using ra3::gamelogic::GarrisonStatusInput;
using ra3::gamelogic::PhysicsContact;
using ra3::gamelogic::PhysicsRuntime;
using ra3::gamelogic::SpawnCrateState;
using ra3::gamelogic::SpawnCrateTiming;
using ra3::gamelogic::TimedCondition;
using ra3::gamelogic::Vector3;

constexpr std::size_t kBufferSize = 1024;

class Buffer {
public:
    alignas(16) std::array<std::uint8_t, kBufferSize> bytes{};

    std::uint32_t word(std::size_t index) const
    {
        std::uint32_t result = 0;
        std::memcpy(&result, bytes.data() + index * 4U, sizeof(result));
        return result;
    }

    void word(std::size_t index, std::uint32_t value)
    {
        std::memcpy(bytes.data() + index * 4U, &value, sizeof(value));
    }

    float real(std::size_t index) const
    {
        float result = 0.0F;
        std::memcpy(&result, bytes.data() + index * 4U, sizeof(result));
        return result;
    }

    void real(std::size_t index, float value)
    {
        std::memcpy(bytes.data() + index * 4U, &value, sizeof(value));
    }

    bool boolean(std::size_t index) const { return word(index) != 0; }

    template <typename T>
    T* at(std::size_t offset)
    {
        return reinterpret_cast<T*>(bytes.data() + offset);
    }

    template <typename T>
    const T* at(std::size_t offset) const
    {
        return reinterpret_cast<const T*>(bytes.data() + offset);
    }
};

Buffer fromHex(const std::string& text)
{
    if (text.size() != kBufferSize * 2U) {
        throw std::runtime_error("scenario size");
    }
    Buffer result{};
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        result.bytes[index] = static_cast<std::uint8_t>(
            std::stoul(text.substr(index * 2U, 2), nullptr, 16));
    }
    return result;
}

void printHex(const Buffer& value)
{
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (const std::uint8_t byte : value.bytes) {
        std::cout << std::setw(2) << static_cast<unsigned>(byte);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << '\n';
}

Vector3 vector(const Buffer& input, std::size_t index)
{
    return {input.real(index), input.real(index + 1U), input.real(index + 2U)};
}

void vector(Buffer& output, std::size_t index, const Vector3& value)
{
    output.real(index, value.x);
    output.real(index + 1U, value.y);
    output.real(index + 2U, value.z);
}

Buffer evaluate(std::uint32_t address, const Buffer& input)
{
    using namespace ra3::gamelogic;
    Buffer output = input;
    switch (address) {
    case 0x006B1540:
        output.word(0, static_cast<std::uint32_t>(findOpenGarrisonPoint(
            static_cast<std::int32_t>(input.word(0)), input.at<std::uint32_t>(0x100),
            static_cast<std::int32_t>(input.word(1)))));
        break;
    case 0x006B1670:
        output.word(0, static_cast<std::uint32_t>(chooseGarrisonDestination(
            input.boolean(0), input.boolean(1),
            static_cast<std::int32_t>(input.word(2)))));
        break;
    case 0x006B16E0:
        output.word(0, garrisonReassignmentActions(
            static_cast<std::int32_t>(input.word(0)),
            static_cast<std::int32_t>(input.word(1))));
        break;
    case 0x006B1900:
        output.word(0, preferGarrisonPoint(
            static_cast<std::int32_t>(input.word(0)),
            static_cast<std::int32_t>(input.word(1)),
            input.real(2), input.real(3)) ? 1U : 0U);
        break;
    case 0x006B1E90: {
        std::uint32_t destination = input.word(4);
        output.word(0, copyExitWaypointIfAvailable(
            input.boolean(0), input.boolean(1), input.word(2),
            input.word(3), destination) ? 1U : 0U);
        output.word(4, destination);
        break;
    }
    case 0x006B1F00:
        output.word(0, countMatchingOccupants(
            input.boolean(0), input.at<std::uint8_t>(0x100),
            input.word(1), input.word(2)));
        break;
    case 0x006B20E0:
        output.word(0, canTransferContainedObject(
            input.word(0), input.boolean(1), input.boolean(2),
            input.boolean(3), input.boolean(4), input.boolean(5)) ? 1U : 0U);
        break;
    case 0x006B21C0:
        output.word(0, containsObjectId(
            input.at<std::uint32_t>(0x100), input.word(0),
            input.word(1)) ? 1U : 0U);
        break;
    case 0x006B2220:
        output.word(0, hasOpenRiderSlotZero(
            input.boolean(0), input.at<std::uint8_t>(0x100),
            input.word(1)) ? 1U : 0U);
        break;
    case 0x006B2260:
        output.word(0, hasOpenRiderSlotOne(
            input.boolean(0), input.at<std::uint8_t>(0x100),
            input.word(1)) ? 1U : 0U);
        break;
    case 0x006B2320:
        output.word(0, static_cast<std::uint32_t>(nearestContainedObject2D(
            input.at<ContainedObject>(0x100), input.word(0),
            input.real(1), input.real(2))));
        break;
    case 0x006B23B0: {
        float scale = input.real(1);
        output.word(0, static_cast<std::uint32_t>(consumeScaledSpawnCount(
            static_cast<std::int32_t>(input.word(0)), scale)));
        output.real(1, scale);
        break;
    }
    case 0x006B2690: {
        GarrisonStatusInput status{};
        std::memcpy(status.healthy.data(), input.bytes.data() + 0x100, 36);
        std::memcpy(status.damaged.data(), input.bytes.data() + 0x124, 36);
        std::memcpy(status.reallyDamaged.data(), input.bytes.data() + 0x148, 36);
        status.healthyMatches = input.boolean(0);
        status.damagedMatches = input.boolean(1);
        status.reallyDamagedMatches = input.boolean(2);
        output.word(0, garrisonDamageStatusToSet(status));
        break;
    }
    case 0x006B27B0:
        output.word(0, clearGarrisonDamageStatusBits(input.word(0)));
        break;
    case 0x006B3120:
        output.word(0, remainingConditionFrames(
            input.at<TimedCondition>(0x100), input.word(0),
            input.word(1), input.word(2)));
        break;
    case 0x006B3160:
        output.word(0, nextConditionWakeFrame(
            input.at<TimedCondition>(0x100), input.word(0), input.word(1)));
        break;
    case 0x006B3270:
        vector(output, 0, scaleBalloonVelocity(vector(input, 0), input.real(3)));
        break;
    case 0x006B3660:
        output.word(0, specialAbilityDamageBucket(input.real(0)));
        break;
    case 0x006B3820: {
        const auto result = updateDisabledStatus(
            input.boolean(0), input.boolean(1), input.boolean(2),
            input.real(3), input.real(4));
        output.word(0, result.active ? 1U : 0U);
        output.word(1, result.pending ? 1U : 0U);
        output.word(2, result.statusSet ? 1U : 0U);
        output.word(3, result.dirty ? 1U : 0U);
        output.word(4, result.sleepFrames);
        break;
    }
    case 0x006B3980:
        output.word(0, specialAbilityActivationState(input.word(0)));
        break;
    case 0x006B39A0:
        vector(output, 0, approachPosition(
            vector(input, 0), vector(input, 3),
            input.real(6), input.real(7)));
        break;
    case 0x006B3F10:
        output.word(0, crossedTriggerRadius(
            vector(input, 0), vector(input, 3), input.real(6),
            input.boolean(16), input.boolean(17)) ? 1U : 0U);
        break;
    case 0x006B3DC0:
        output.word(0, specialAbilityUnpackState(input.word(0)));
        break;
    case 0x006B3FC0:
        output.real(0, minimumSpecialProgress(
            input.at<float>(0x100), input.word(0), input.real(1)));
        break;
    case 0x006B4070: {
        float base = input.real(1);
        broadcastSpecialProgress(
            output.at<float>(0x100), input.word(0), base, input.real(2));
        output.real(1, base);
        break;
    }
    case 0x006B40F0: {
        float base = input.real(1);
        resetSpecialProgress(output.at<float>(0x100), input.word(0), base);
        output.real(1, base);
        break;
    }
    case 0x006B44E0:
        output.word(0, withinAnySpecialPosition(
            input.at<Vector3>(0x100), input.word(0),
            vector(input, 1), input.real(4)) ? 1U : 0U);
        break;
    case 0x006B4720:
        output.word(0, requiredConditionSet(input.word(0), input.word(1)) ? 1U : 0U);
        break;
    case 0x006B48D0:
        output.word(0, requiredConditionSetUpdate(
            input.word(0), input.word(1)) ? 1U : 0U);
        break;
    case 0x006B4A40:
        output.word(0, specialPowerWithinRange(
            input.boolean(0), input.real(1), vector(input, 2),
            vector(input, 5), input.boolean(8)) ? 1U : 0U);
        break;
    case 0x006B4D40:
        output.word(0, weaponFireTargetAllowed(
            input.word(0), input.boolean(1)) ? 1U : 0U);
        break;
    case 0x006B4EB0: {
        Vector3 target = vector(input, 0);
        applyWeaponFireRadius(
            target, input.real(3), input.real(4),
            input.boolean(5), input.real(6));
        vector(output, 0, target);
        break;
    }
    case 0x006B51C0:
        output.word(0, clearDockObjectStatus(input.word(0)));
        break;
    case 0x006B5AF0:
        vector(output, 0, selectSourceDockPosition(
            vector(input, 0), vector(input, 3), vector(input, 6),
            input.boolean(9), input.boolean(10), vector(input, 11)));
        break;
    case 0x006B5BA0:
        vector(output, 0, selectTargetDockPosition(
            vector(input, 0), vector(input, 3), vector(input, 6)));
        break;
    case 0x006B5C20:
        vector(output, 0, selectApproachDockPosition(
            vector(input, 0), vector(input, 3), vector(input, 6)));
        break;
    case 0x006B5CA0: {
        std::uint32_t first = input.word(0);
        std::uint32_t second = input.word(1);
        markDockPairOccupied(first, second);
        output.word(0, first);
        output.word(1, second);
        break;
    }
    case 0x006B5AE0:
        setDockEnabled(output.bytes[0x66], input.boolean(0));
        break;
    case 0x006B6EC0: {
        Vector3 destination = vector(input, 0);
        std::uint32_t flags = input.word(3);
        std::uint32_t target = input.word(4);
        setSpecialDestination(destination, flags, target, vector(input, 5));
        vector(output, 0, destination);
        output.word(3, flags);
        output.word(4, target);
        break;
    }
    case 0x006B7340:
        output.word(0, canSelectAlternateTarget(
            input.boolean(0), input.boolean(1),
            input.boolean(2), input.boolean(3)) ? 1U : 0U);
        break;
    case 0x006B7850:
        output.real(0, bombardRadius(
            input.real(0), input.boolean(1), input.real(2)));
        break;
    case 0x006B7B00: {
        FireWeaponStateInput state{};
        std::memcpy(state.weaponState.data(), input.bytes.data(), 48);
        state.position = vector(input, 12);
        state.specialStatus = input.boolean(15);
        state.statusAlreadySet = input.boolean(16);
        const auto result = initializeFireWeaponState(state);
        std::memcpy(output.bytes.data(), result.weaponState.data(), 48);
        vector(output, 12, result.origin);
        output.word(15, result.phase);
        output.word(16, result.reserved);
        output.word(17, result.statusSet ? 1U : 0U);
        output.word(18, result.dirty ? 1U : 0U);
        break;
    }
    case 0x006B7D70: {
        std::uint32_t flags = input.word(0);
        invalidTargetsDeathFlags(flags);
        output.word(0, flags);
        break;
    }
    case 0x006B7D80: {
        std::uint32_t flags = input.word(0);
        invalidTargetsDeathDefaultFlags(flags);
        output.word(0, flags);
        break;
    }
    case 0x006B8480: {
        std::uint32_t flags = input.word(0);
        linearDamageFlags(flags);
        output.word(0, flags);
        break;
    }
    case 0x006B8CF0:
        vector(output, 0, physicsAnglesToVector(
            input.real(0), input.real(1), input.real(2)));
        break;
    case 0x006B8C70: {
        std::uint32_t flags = input.word(0);
        physicsBehaviorFlags(flags);
        output.word(0, flags);
        break;
    }
    case 0x006B8EC0: {
        PhysicsRuntime state{};
        std::memcpy(&state, input.bytes.data() + 0x100, sizeof(state));
        initializePhysicsRuntime(state, input.word(0), input.word(1), input.word(2));
        std::memcpy(output.bytes.data(), &state, sizeof(state));
        break;
    }
    case 0x006B91E0:
        output.word(0, hasMotiveTarget(
            input.boolean(0), input.word(1)) ? 1U : 0U);
        break;
    case 0x006B9510: {
        PhysicsContact firstValue{};
        firstValue.id = input.word(6);
        PhysicsContact* first = input.boolean(0) ? &firstValue : nullptr;
        PhysicsContact* last = first;
        PhysicsContact contact{};
        contact.id = input.word(1);
        std::uint32_t count = input.word(2);
        appendPhysicsContact(first, last, contact, count);
        output.word(0, first == &contact ? 1U : 0U);
        output.word(1, last == &contact ? 1U : 0U);
        output.word(2, count);
        output.word(3, contact.next);
        output.word(4, contact.previous);
        output.word(5, firstValue.next);
        break;
    }
    case 0x006B9580: {
        const std::uint32_t count = input.word(0);
        auto* contacts = output.at<PhysicsContact>(0x100);
        for (std::uint32_t index = 0; index < count; ++index) {
            contacts[index].next = index + 1U < count
                ? static_cast<std::uint32_t>(
                    reinterpret_cast<std::uintptr_t>(&contacts[index + 1U]))
                : 0U;
        }
        output.word(0, countMatchingContacts(
            count == 0 ? nullptr : contacts, input.at<std::uint32_t>(0x300),
            input.word(1)));
        break;
    }
    case 0x006B95D0: {
        std::array<std::uint32_t, 5> frames{};
        std::array<std::uint8_t, 5> enabled{};
        std::memcpy(frames.data(), input.bytes.data() + 0x100, sizeof(frames));
        std::memcpy(enabled.data(), input.bytes.data() + 0x120, enabled.size());
        output.word(0, enablePhysicsContact(
            frames, enabled, input.word(0), input.boolean(1),
            input.word(2)) ? 1U : 0U);
        std::memcpy(output.bytes.data() + 0x100, frames.data(), sizeof(frames));
        std::memcpy(output.bytes.data() + 0x120, enabled.data(), enabled.size());
        break;
    }
    case 0x006B96C0: {
        std::uint32_t frame = input.word(0);
        std::uint8_t state = static_cast<std::uint8_t>(input.word(1));
        output.word(0, expireDelayedWeapon(
            frame, state, input.word(2)) ? 1U : 0U);
        output.word(1, frame);
        output.word(2, state);
        break;
    }
    case 0x006B99C0: {
        std::uint32_t currentCount = input.word(1);
        std::uint32_t previousCount = input.word(2);
        output.word(0, removePhysicsOverlap(
            input.word(0), output.at<std::uint32_t>(0x100),
            output.at<std::uint32_t>(0x180), currentCount,
            output.at<std::uint32_t>(0x200),
            output.at<std::uint32_t>(0x280), previousCount) ? 1U : 0U);
        output.word(1, currentCount);
        output.word(2, previousCount);
        break;
    }
    case 0x006B9B70:
        output.word(0, needsAirborneStatus(
            input.real(0), input.word(1)) ? 1U : 0U);
        break;
    case 0x006B9BB0:
    {
        const auto result = clearPhysicsStatusesAndSet(
            input.word(0), input.word(1), input.word(2), input.boolean(3));
        output.word(0, result.statusA8);
        output.word(1, result.statusB8);
        output.word(2, result.conditionWasSet ? 1U : 0U);
        break;
    }
    case 0x006BA5D0: {
        SpawnCrateState state{};
        std::memcpy(&state, input.bytes.data() + 0x100, sizeof(state));
        initializeSpawnCrateState(
            state, input.word(0), input.word(1),
            SpawnCrateTiming{
                input.boolean(2), input.real(3),
                input.real(4), input.real(5)});
        std::memcpy(output.bytes.data(), &state, sizeof(state));
        break;
    }
    default:
        throw std::runtime_error("unknown address");
    }
    return output;
}

void mutate(Buffer& output)
{
    output.bytes[0] ^= 1U;
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
            std::istringstream stream(line);
            std::string operation;
            std::string addressText;
            std::string scenarioText;
            stream >> operation >> addressText >> scenarioText;
            Buffer result = evaluate(
                static_cast<std::uint32_t>(
                    std::stoul(addressText, nullptr, 16)),
                fromHex(scenarioText));
            if (operation == "mutate") {
                mutate(result);
            } else if (operation != "eval") {
                throw std::runtime_error("operation");
            }
            printHex(result);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
