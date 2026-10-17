#include <ra3/common/StateMachine.h>

#include <cstring>

namespace ra3::common {
namespace {

std::uint32_t read32(const void* base, std::uint32_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const char*>(base) + offset, 4);
    return value;
}

void write32(void* base, std::uint32_t offset, std::uint32_t value) noexcept
{
    std::memcpy(static_cast<char*>(base) + offset, &value, 4);
}

std::uint8_t read8(const void* base, std::uint32_t offset) noexcept
{
    return static_cast<const std::uint8_t*>(base)[offset];
}

void write8(void* base, std::uint32_t offset, std::uint8_t value) noexcept
{
    static_cast<std::uint8_t*>(base)[offset] = value;
}

void* asPointer(std::uint32_t address) noexcept
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));
}

std::uint32_t asAddress(const void* pointer) noexcept
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t withLowByte(std::uint32_t value, std::uint32_t low) noexcept
{
    return (value & 0xFFFFFF00u) | (low & 0xFFu);
}

// Signed quotient of a 32-bit byte distance by 12, matching
// `imul 0x2AAAAAAB; sar edx, 1; shr eax, 31; add eax, edx`.
std::int32_t signedCount12(std::uint32_t end, std::uint32_t begin) noexcept
{
    const auto bytes = static_cast<std::int32_t>(end - begin);
    const auto product = static_cast<std::int64_t>(bytes) *
                         static_cast<std::int64_t>(static_cast<std::int32_t>(0x2AAAAAABu));
    auto high = static_cast<std::int32_t>(product >> 32);
    high >>= 1;
    const auto unsignedHigh = static_cast<std::uint32_t>(high);
    return static_cast<std::int32_t>(unsignedHigh + (unsignedHigh >> 31));
}

void copyVec3(void* destination, std::uint32_t destinationOffset, const void* source,
              std::uint32_t sourceOffset) noexcept
{
    write32(destination, destinationOffset, read32(source, sourceOffset));
    write32(destination, destinationOffset + 4, read32(source, sourceOffset + 4));
    write32(destination, destinationOffset + 8, read32(source, sourceOffset + 8));
}

std::uint32_t depthEnter(std::uint32_t* depth) noexcept
{
    const std::uint32_t next = *depth + 1u;
    *depth = next;
    return next;
}

class RestoreByte {
public:
    RestoreByte(void* object, std::uint32_t offset, std::uint8_t replacement) noexcept
        : object_(object), offset_(offset), saved_(read8(object, offset))
    {
        write8(object_, offset_, replacement);
    }

    ~RestoreByte() { write8(object_, offset_, saved_); }

    RestoreByte(const RestoreByte&) = delete;
    RestoreByte& operator=(const RestoreByte&) = delete;

private:
    void* object_;
    std::uint32_t offset_;
    std::uint8_t saved_;
};

}  // namespace

std::uint32_t lockMachine(void* self, std::uint32_t incoming) noexcept
{
    write8(self, 0x78, 1);
    return incoming;
}

std::uint32_t unlockMachine(void* self, std::uint32_t incoming) noexcept
{
    write8(self, 0x78, 0);
    return incoming;
}

std::uint32_t storeGoalScalar(void* self, std::uint32_t incoming, std::uint32_t bits) noexcept
{
    if (read8(self, 0x78) == 0) {
        write32(self, 0x54, bits);
    }
    return incoming;
}

std::uint32_t storeGoalScalarMutant(void* self, std::uint32_t incoming, std::uint32_t bits) noexcept
{
    // Inverts the lock test of 0x005AA280. Not a shipped body.
    if (read8(self, 0x78) != 0) {
        write32(self, 0x54, bits);
    }
    return incoming;
}

std::uint32_t storeGoalDword(void* self, std::uint32_t value) noexcept
{
    write32(self, 0x70, value);
    return value;
}

std::uint32_t setGoalObject(void* self, std::uint32_t incoming, std::uint32_t object) noexcept
{
    if (read8(self, 0x78) != 0) {
        return incoming;
    }
    if (object == 0) {
        write32(self, 0x28, 0);
        return 0;
    }
    const std::uint32_t id = read32(asPointer(object), 0xDC);
    write32(self, 0x28, id);
    return id;
}

std::uint32_t setGoalPosition(void* self, std::uint32_t incoming, std::uint32_t position,
                              std::uint32_t scalarBits) noexcept
{
    if (read8(self, 0x78) != 0) {
        return incoming;
    }
    std::uint32_t result = 0;
    if (position != 0) {
        copyVec3(self, 0x2C, asPointer(position), 0);
        copyVec3(self, 0x48, asPointer(position), 0);
        write8(self, 0x44, 0);
        result = read32(asPointer(position), 8);
    }
    write32(self, 0x54, scalarBits);
    return result;
}

std::uint32_t resetGoalPosition(void* self, std::uint32_t incoming, std::uint32_t position) noexcept
{
    if (read8(self, 0x78) != 0) {
        return incoming;
    }
    std::uint32_t result = 0;
    if (position != 0) {
        copyVec3(self, 0x2C, asPointer(position), 0);
        copyVec3(self, 0x48, asPointer(position), 0);
        write8(self, 0x44, 0);
        result = read32(asPointer(position), 8);
    }
    write32(self, 0x54, kDefaultGoalScalarBits);
    return result;
}

std::uint32_t copyGoalFields(void* self, std::uint32_t source) noexcept
{
    const auto* from = asPointer(source);
    if (read8(self, 0x78) == 0) {
        // `lea eax, [source+0x2C]; test eax, eax` is false only for source == -0x2C.
        if (source + 0x2C != 0) {
            copyVec3(self, 0x2C, from, 0x2C);
            copyVec3(self, 0x48, from, 0x2C);
            write8(self, 0x44, 0);
        }
        // The constant store is then replaced by fstp of source+0x54. For the
        // finite values the oracle uses, that round-trip keeps the bits.
        write32(self, 0x54, read32(from, 0x54));
    }
    write8(self, 0x7C, read8(from, 0x7C));
    write8(self, 0x44, read8(from, 0x44));
    copyVec3(self, 0x48, from, 0x48);
    return read32(from, 0x50);
}

std::uint32_t pathPointAt(const void* self, std::int32_t index) noexcept
{
    if (index < 0) {
        return 0;
    }
    const std::uint32_t begin = read32(self, 0x58);
    const std::uint32_t end = read32(self, 0x5C);
    const std::int32_t count = signedCount12(end, begin);
    if (index >= count) {
        return 0;
    }
    return begin + static_cast<std::uint32_t>(index) * 12u;
}

std::uint32_t currentStateId(const void* self) noexcept
{
    const std::uint32_t state = read32(self, 4);
    if (state == 0) {
        return kInvalidStateId;
    }
    return read32(asPointer(state), 4);
}

std::uint32_t isInIdleState(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t state = read32(self, 4);
    if (state == 0) {
        return withLowByte(incoming, 0);
    }
    const std::uint32_t result = hooks.callState(hooks.context, state, 0x28, 0);
    return withLowByte(result, (result & 0xFFu) != 0 ? 1u : 0u);
}

std::uint32_t forwardStateSlot48(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t state = read32(self, 4);
    if (state == 0) {
        return incoming;
    }
    return hooks.callState(hooks.context, state, 0x48, 0);
}

std::uint32_t callWhileUnlocked(void* self, StateMachineHooks& hooks) noexcept
{
    write8(self, 0x78, 0);
    const std::uint32_t result =
        hooks.callMachine(hooks.context, asAddress(self), 0x34, 0);
    write8(self, 0x78, 1);
    return result;
}

std::uint32_t notifyStateThenMachine(void* self, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t state = read32(self, 4);
    if (state != 0) {
        hooks.callState(hooks.context, state, 0x3C, 0);
    }
    return hooks.callMachine(hooks.context, asAddress(self), 0x38, 0);
}

std::uint32_t stateMapContains(void* self, std::uint32_t incoming, std::uint32_t key,
                               StateMachineHooks& hooks) noexcept
{
    // map::find returns the address of its iterator slot. `mov al, dl` then
    // keeps that address and replaces only the low byte with 0 or 1. The
    // boolean is the semantic result; the oracle checks the native residue
    // against the slot address the shipped call returns.
    (void)incoming;
    const std::uint32_t found = hooks.findState(hooks.context, asAddress(self), key);
    return found != 0 ? 1u : 0u;
}

std::uint32_t internalGetState(void* self, std::uint32_t key, StateMachineHooks& hooks) noexcept
{
    return hooks.findState(hooks.context, asAddress(self), key);
}

std::uint32_t scanConditions(void* state, std::uint32_t* outStatus, std::uint32_t incoming,
                             StateMachineHooks& hooks) noexcept
{
    std::uint32_t count = read32(state, 0x14);
    if (count == 0) {
        return incoming;
    }
    std::uint32_t record = read32(state, 0x10);
    std::uint32_t result = incoming;
    while (count != 0) {
        const std::uint32_t function = read32(asPointer(record), 0);
        const std::uint32_t toState = read32(asPointer(record), 4);
        const std::uint32_t userData = read32(asPointer(record), 8);
        result = hooks.callCondition(hooks.context, function, asAddress(state), userData);
        if ((result & 0xFFu) != 0) {
            if (toState == kExitMachineSuccess) {
                *outStatus = kStateSuccess;
                return result;
            }
            if (toState == kExitMachineFailure) {
                *outStatus = kStateFailure;
                // `mov eax, out; mov [eax], 0xFFFFFFFE` returns the out pointer.
                return asAddress(outStatus);
            }
            result = internalSetState(asPointer(read32(state, 0x18)), toState, hooks);
            *outStatus = result;
            return result;
        }
        record += 0x10;
        count -= 1u;
    }
    return result;
}

std::uint32_t applyTransitionId(void* state, std::uint32_t transitionId,
                                StateMachineHooks& hooks) noexcept
{
    auto* machine = asPointer(read32(state, 0x18));
    if (transitionId == kExitMachineSuccess) {
        internalSetState(machine, kMachineDoneStateId, hooks);
        return kStateSuccess;
    }
    if (transitionId == kExitMachineFailure) {
        internalSetState(machine, kMachineDoneStateId, hooks);
        return kStateFailure;
    }
    if (transitionId == kSpecialStateId) {
        return hooks.callMachine(hooks.context, asAddress(machine), 0x18, 0);
    }
    return internalSetState(machine, transitionId, hooks);
}

std::uint32_t checkTransitions(void* state, std::uint32_t status, StateMachineHooks& hooks) noexcept
{
    if (depthEnter(hooks.transitionDepth) >= kStateRecursionLimit) {
        *hooks.transitionDepth -= 1u;
        return kStateFailure;
    }
    std::uint32_t result = 0;
    if (status == kStateFailure) {
        result = applyTransitionId(state, read32(state, 0x0C), hooks);
    } else if (status == kStateSuccess) {
        result = applyTransitionId(state, read32(state, 0x08), hooks);
    } else if (status != 0) {
        result = 0;
    } else {
        std::uint32_t local = 0;
        scanConditions(state, &local, 0, hooks);
        result = local;
    }
    *hooks.transitionDepth -= 1u;
    return result;
}

std::uint32_t checkSleepTransitions(void* state, std::uint32_t frames,
                                    StateMachineHooks& hooks) noexcept
{
    if (depthEnter(hooks.sleepDepth) >= kStateRecursionLimit) {
        *hooks.sleepDepth -= 1u;
        return kStateFailure;
    }
    std::uint32_t slot = frames;
    scanConditions(state, &slot, 0, hooks);
    *hooks.sleepDepth -= 1u;
    return slot;
}

std::uint32_t internalSetState(void* self, std::uint32_t stateId, StateMachineHooks& hooks) noexcept
{
    write32(self, 0x74, 0);
    std::uint32_t next = 0;
    if (stateId == kMachineDoneStateId) {
        next = 0;
    } else if (stateId == kInvalidStateId && read32(self, 0x68) == kInvalidStateId) {
        return kStateFailure;
    } else {
        next = hooks.findState(hooks.context, asAddress(self), stateId);
    }

    const std::uint32_t previous = read32(self, 4);
    if (previous != 0) {
        hooks.callState(hooks.context, previous, 0x18, 0);
    }
    write32(self, 4, next);
    if (next == 0) {
        return 0;
    }

    const std::uint32_t entered = hooks.callState(hooks.context, next, 0x10, 0);
    const std::uint32_t current = read32(self, 4);
    if (current == 0) {
        return kStateFailure;
    }
    if (current != next || static_cast<std::int32_t>(entered) <= 0) {
        const std::uint32_t status = current == next ? entered : 0u;
        return checkTransitions(asPointer(current), status, hooks);
    }

    const std::uint32_t sleepUntil = entered + hooks.frame;
    write32(self, 0x74, sleepUntil);
    return checkSleepTransitions(asPointer(current), entered, hooks);
}

std::uint32_t setState(void* self, std::uint32_t stateId, StateMachineHooks& hooks) noexcept
{
    if (read8(self, 0x78) != 0) {
        return 0;
    }
    return internalSetState(self, stateId, hooks);
}

std::uint32_t initDefaultState(void* self, StateMachineHooks& hooks) noexcept
{
    if (read8(self, 0x7A) != 0) {
        return kStateFailure;
    }
    write8(self, 0x7A, 1);
    return internalSetState(self, read32(self, 0x68), hooks);
}

std::uint32_t updateStateMachine(void* self, StateMachineHooks& hooks) noexcept
{
    RestoreByte reentry(self, 0x79, 1);
    const std::uint32_t frame = hooks.frame;
    const std::uint32_t sleep = read32(self, 0x74);
    if (sleep != 0 && frame < sleep) {
        const std::uint32_t state = read32(self, 4);
        if (state == 0) {
            return kStateFailure;
        }
        return checkSleepTransitions(asPointer(state), sleep - frame, hooks);
    }

    write32(self, 0x74, 0);
    const std::uint32_t state = read32(self, 4);
    if (state == 0) {
        return kStateFailure;
    }
    const std::uint32_t updated = hooks.callState(hooks.context, state, 0x14, 0);
    const std::uint32_t current = read32(self, 4);
    if (current == 0) {
        return kStateFailure;
    }
    if (current != state || static_cast<std::int32_t>(updated) <= 0) {
        return checkTransitions(asPointer(current), current == state ? updated : 0u, hooks);
    }
    write32(self, 0x74, frame + updated);
    return checkSleepTransitions(asPointer(current), updated, hooks);
}

std::uint32_t failureStateExit(void* state, std::uint32_t argument, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t machine = read32(state, 0x18);
    const std::uint32_t owner = machine == 0 ? 0u : read32(asPointer(machine), 0x24);
    if (owner == 0) {
        return 0;
    }
    if ((argument & 0xFFu) != 0) {
        const std::uint32_t probed = hooks.callExternal(hooks.context, kExtOwnerProbe, owner, 0);
        if (probed != 0) {
            return probed;
        }
    }
    return hooks.callExternal(hooks.context, kExtOwnerTail, owner, kStateSuccess);
}

std::uint32_t goalQuery(void* self, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t mode = read32(self, 0x6C);
    if (mode == 0) {
        return hooks.callExternal(hooks.context, kExtOwnerQuery, read32(self, 0x24), 0);
    }
    if (mode == 1) {
        const std::uint32_t goalId = read32(self, 0x28);
        if (goalId == 0) {
            return 0;
        }
        hooks.callExternal(hooks.context, kExtGoalLookupBegin, goalId, 0);
        const std::uint32_t value = hooks.callExternal(hooks.context, kExtGoalLookup, goalId, 0);
        hooks.callExternal(hooks.context, kExtGoalLookupEnd, 1, 0);
        return value;
    }
    const std::uint32_t asserted =
        hooks.callExternal(hooks.context, kExtAssert, 0x00BE90A8u, 1);
    if ((asserted & 0xFFu) != 0) {
        return 0;
    }
    const std::uint32_t gate = hooks.callExternal(hooks.context, kExtDebugGate, 0, 0);
    if ((gate & 0xFFu) != 0) {
        return 0;
    }
    // Four calls: debug vtable +0x78, +0x3C, +0x3C, +0x50. Each returns 0 here;
    // the shipped bytes keep the debug object in EAX and still return 0.
    hooks.callExternal(hooks.context, kExtDebugChain, 0, 0);
    hooks.callExternal(hooks.context, kExtDebugChain, 0, 0);
    hooks.callExternal(hooks.context, kExtDebugChain, 0, 0);
    hooks.callExternal(hooks.context, kExtDebugChain, 0, 0);
    return 0;
}

std::uint32_t goalObjectLive(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept
{
    if (read32(self, 0x28) == 0) {
        return withLowByte(incoming, 0);
    }
    const std::uint32_t object = goalQuery(self, hooks);
    if (object == 0) {
        return 1u;
    }
    return withLowByte(object, read8(asPointer(object), 0x471) & 1u);
}

std::uint32_t publishGoalPosition(void* self, std::uint32_t out, StateMachineHooks& hooks) noexcept
{
    const std::uint32_t owner = read32(self, 0x24);
    const std::uint32_t unit = read32(asPointer(owner), 0x374);
    const std::uint32_t goal = goalQuery(self, hooks);
    const std::uint32_t moving = hooks.callExternal(hooks.context, kExtMoving, owner, 0);
    const bool stopped = (moving & 0xFFu) == 0;
    if (goal != 0) {
        copyVec3(asPointer(out), 0, asPointer(goal), 0x38);
        if (unit != 0) {
            const std::uint32_t goalObjectId = read32(asPointer(goal), 0xDC);
            const std::uint32_t unitGoal = hooks.callExternal(hooks.context, kExtUnitGoal, unit, 0);
            if (goalObjectId == unitGoal) {
                return out;
            }
        }
    } else {
        copyVec3(asPointer(out), 0, self, 0x2C);
    }
    if (!stopped) {
        return out;
    }
    const std::uint32_t kind = hooks.callExternal(hooks.context, kExtUnitKind, unit, 0);
    if (kind == 1u) {
        hooks.callExternal(hooks.context, kExtMoveProbe, out, 0);
        // The byte compared after the probe is the low byte of the out pointer.
        if ((out & 0xFFu) == 0u) {
            return out;
        }
    }
    const std::uint32_t canPublish =
        hooks.callExternal(hooks.context, kExtCanPublish, owner, asAddress(self));
    if ((canPublish & 0xFFu) == 0) {
        return out;
    }
    const std::uint32_t already =
        hooks.callExternal(hooks.context, kExtAlreadyThere, unit, out);
    if ((already & 0xFFu) != 0) {
        return out;
    }
    hooks.callExternal(hooks.context, kExtPublish, owner, out);
    return out;
}

std::uint32_t assignGoalPath(void* self, std::uint32_t source, StateMachineHooks& hooks) noexcept
{
    return hooks.callExternal(hooks.context, kExtPathAssign, asAddress(self) + 0x58u, source);
}

std::uint32_t xferGoalPath(void* self, std::uint32_t xfer, std::uint32_t vector,
                           StateMachineHooks& hooks) noexcept
{
    (void)self;
    auto* xferObject = asPointer(xfer);
    auto* records = asPointer(vector);
    hooks.callExternal(hooks.context, kExtXferOpen, xfer, 0);
    std::uint32_t count = static_cast<std::uint32_t>(
        signedCount12(read32(records, 4), read32(records, 0)));
    hooks.callExternal(hooks.context, kExtXferName, xfer, 0x00BDF38Cu);
    count = hooks.callExternal(hooks.context, kExtXferCount, xfer, count);
    if ((read8(xferObject, 4) & 1u) != 0) {
        std::uint32_t cursor = read32(records, 0);
        const std::uint32_t end = read32(records, 4);
        while (cursor != end) {
            hooks.callExternal(hooks.context, kExtXferElement, xfer, cursor);
            cursor += 12u;
        }
        return xfer;
    }
    if (read32(records, 0) != read32(records, 4)) {
        hooks.callExternal(hooks.context, kExtXferClear, xfer, 0x00BDF36Cu);
    }
    hooks.callExternal(hooks.context, kExtXferReserve, vector, count);
    if (count == 0) {
        return xfer;
    }
    for (std::uint32_t remaining = count; remaining != 0; remaining -= 1u) {
        if (read8(xferObject, 8) != 0) {
            break;
        }
        const std::uint32_t end = read32(records, 4);
        const std::uint32_t cap = read32(records, 8);
        if (end < cap) {
            write32(records, 4, end + 12u);
            std::memset(asPointer(end), 0, 12);
        } else {
            hooks.callExternal(hooks.context, kExtXferGrow, vector, end);
        }
        hooks.callExternal(hooks.context, kExtXferElement, xfer, read32(records, 4) - 12u);
    }
    return xfer;
}

}  // namespace ra3::common
