#include "ra3/gamelogic/AIStateViews.h"

#include <cmath>
#include <cstring>

namespace ra3::gamelogic {
namespace {

template <typename T>
T load(const std::byte* data, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

template <typename T>
void store(std::byte* data, std::size_t offset, T value) noexcept
{
    std::memcpy(data + offset, &value, sizeof(value));
}

std::byte* bytes(void* value) noexcept
{
    return static_cast<std::byte*>(value);
}

const std::byte* bytes(const void* value) noexcept
{
    return static_cast<const std::byte*>(value);
}

std::byte* pointer(const std::byte* data, std::size_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(load<std::uintptr_t>(data, offset));
}

std::uint32_t pointerBits(const void* value) noexcept
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
}

std::uint32_t byteInRegister(const std::byte* source, std::size_t offset) noexcept
{
    return (pointerBits(source) & 0xffffff00U) | load<std::uint8_t>(source, offset);
}

std::int32_t x87ProductToLowI32(float first, float second, float third) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    std::uint16_t control{};
    std::uint16_t productControl{};
    std::uint16_t truncateControl{};
    std::int64_t wide{};
    __asm {
        fnstcw control
        mov ax, control
        and ax, 0fcffh
        or ax, 0300h
        mov productControl, ax
        or ax, 0c00h
        mov truncateControl, ax
        fldcw productControl
        fld first
        fmul second
        fmul third
        fldcw truncateControl
        fistp qword ptr wide
        fldcw control
    }
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(wide));
#else
    const auto product = static_cast<long double>(first) *
                         static_cast<long double>(second) *
                         static_cast<long double>(third);
    constexpr auto minimum = -9223372036854775808.0L;
    constexpr auto limit = 9223372036854775808.0L;
    if (!std::isfinite(product) || product < minimum || product >= limit) {
        return 0;
    }
    const auto wide = static_cast<std::int64_t>(product);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(wide));
#endif
}

const std::byte* adjustedModuleData(const std::byte* adjusted) noexcept
{
    return pointer(adjusted - 0x518, 0);
}

std::byte* adjustedOwner(const std::byte* adjusted) noexcept
{
    return pointer(adjusted - 0x514, 0);
}

std::uint32_t stateId(const std::byte* state) noexcept
{
    return state == nullptr ? 999999U : load<std::uint32_t>(state, 0x04);
}

std::uint32_t dispatchCall(
    const GameDispatch& dispatch, void* target, std::uint32_t slot,
    const std::uint32_t* arguments = nullptr, std::size_t argumentCount = 0) noexcept
{
    if (dispatch.invoke == nullptr) {
        return 0;
    }
    return dispatch.invoke(dispatch.context, target, slot, arguments, argumentCount);
}

}  // namespace

StateView::StateView(void* storage) noexcept : data_(bytes(storage)) {}
StateView::StateView(const void* storage) noexcept : data_(const_cast<std::byte*>(bytes(storage))) {}

void* StateView::machine() const noexcept
{
    return pointer(data_, 0x18);
}

std::uint32_t StateView::value20() const noexcept
{
    return load<std::uint32_t>(data_, 0x20);
}

void StateView::setValue20(std::uint32_t value) noexcept
{
    store(data_, 0x20, value);
}

StateResult StateView::enterDozerPrimaryIdle(std::uint32_t frame) noexcept
{
    store(data_, 0x20, frame);
    store(data_, 0x28, std::uint8_t{0});
    return StateResult::continueState;
}

void StateView::exitDozerPrimaryIdle() noexcept
{
    if (load<std::uint8_t>(data_, 0x28) != 0) {
        store(data_, 0x24, std::uint32_t{0xffffffffU});
        store(data_, 0x28, std::uint8_t{0});
    }
}

StateResult StateView::enterDozerAction(
    std::uint32_t frame, const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    auto* update = pointer(owner, 0x374);
    auto* dozer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1bc)));
    if (dozer == nullptr) {
        return StateResult::failure;
    }
    store(data_, 0x24, frame);
    const auto task = load<std::uint32_t>(data_, 0x20);
    if (task == 0 || task == 1) {
        const std::uint32_t argument = 0;
        dispatchCall(dispatch, dozer, 0x48, &argument, 1);
    }
    return StateResult::continueState;
}

void StateView::exitDozerAction(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    auto* update = pointer(owner, 0x374);
    if (update == nullptr) {
        return;
    }
    auto* dozer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1bc)));
    if (dozer != nullptr) {
        dispatchCall(dispatch, dozer, 0x58);
    }
}

StateResult StateView::enterNestedDozerAction(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    auto* update = pointer(owner, 0x374);
    if (update == nullptr) {
        return StateResult::failure;
    }
    auto* dozer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1bc)));
    const std::uint32_t task = load<std::uint32_t>(data_, 0x20);
    dispatchCall(dispatch, dozer, 0x28, &task, 1);
    dispatchCall(dispatch, pointer(data_, 0x24), 0x18);
    return StateResult::continueState;
}

void StateView::exitNestedDozerAction(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    auto* update = pointer(owner, 0x374);
    if (update == nullptr) {
        return;
    }
    auto* dozer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1bc)));
    const std::uint32_t invalidTask = 0xffffffffU;
    dispatchCall(dispatch, dozer, 0x28, &invalidTask, 1);
    dispatchCall(dispatch, dozer, 0x58);
}

StateResult StateView::enterSupplyBusy(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    if (owner != nullptr) {
        auto* update = pointer(owner, 0x374);
        if (update != nullptr) {
            auto* supply = reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1c4)));
            if (supply != nullptr) {
                const std::uint32_t value = 0;
                dispatchCall(dispatch, supply, 0x5c, &value, 1);
            }
        }
    }
    return StateResult::continueState;
}

StateResult StateView::enterSupplyIdle(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    if (owner != nullptr) {
        auto* update = pointer(owner, 0x374);
        const auto worker = dispatchCall(dispatch, update, 0x288);
        if (worker == 0) {
            return StateResult::continueState;
        }
    }
    return StateResult::success;
}

StateResult StateView::enterSupplyDocking(const GameDispatch& dispatch) noexcept
{
    const auto* stateMachine = pointer(data_, 0x18);
    auto* owner = pointer(stateMachine, 0x24);
    auto* update = pointer(owner, 0x374);
    auto* supply = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(dispatchCall(dispatch, update, 0x1c4)));
    if (supply == nullptr) {
        return StateResult::failure;
    }
    const std::uint32_t value = 0;
    dispatchCall(dispatch, supply, 0x54, &value, 1);
    return StateResult::continueState;
}

DozerAIView::DozerAIView(void* adjustedStorage) noexcept : data_(bytes(adjustedStorage)) {}
DozerAIView::DozerAIView(const void* adjustedStorage) noexcept
    : data_(const_cast<std::byte*>(bytes(adjustedStorage))) {}

std::uint32_t DozerAIView::hasTask(std::uint32_t task) const noexcept
{
    const auto target = load<std::uint32_t>(data_, 0x04 + std::size_t(task) * 8U);
    return (task & 0xffffff00U) | std::uint32_t(target != 0);
}

std::uint32_t DozerAIView::hasAnyTask(const GameDispatch& dispatch) const noexcept
{
    std::uint32_t result = 0;
    for (std::uint32_t task = 0; task < 3; ++task) {
        result = dispatchCall(dispatch, data_, 0x18, &task, 1);
        if ((result & 0xffU) != 0) {
            return (result & 0xffffff00U) | 1U;
        }
    }
    return result;
}

std::uint32_t DozerAIView::taskTarget(std::uint32_t task) const noexcept
{
    return load<std::uint32_t>(data_, 0x04 + std::size_t(task) * 8U);
}

std::int32_t DozerAIView::mostRecentTask(const GameDispatch& dispatch) const noexcept
{
    std::uint32_t newest = 0;
    std::int32_t result = -1;
    for (std::uint32_t task = 0; task < 3; ++task) {
        const auto timestamp = load<std::uint32_t>(data_, 0x08 + std::size_t(task) * 8U);
        if ((dispatchCall(dispatch, data_, 0x18, &task, 1) & 0xffU) != 0 &&
            newest < timestamp) {
            newest = timestamp;
            result = static_cast<std::int32_t>(task);
        }
    }
    return result;
}

const void* DozerAIView::dockPoint(std::uint32_t task, std::uint32_t point) const noexcept
{
    if (task >= 3 || point >= 3) {
        return nullptr;
    }
    const auto index = point + task * 3U;
    const auto offset = 0x2cU + std::size_t(index) * 0x10U;
    if (load<std::uint8_t>(data_, offset) == 0) {
        return nullptr;
    }
    return data_ + offset + 4U;
}

float DozerAIView::repairHealthPerSecond() const noexcept
{
    return load<float>(adjustedModuleData(data_), 0x4c);
}

float DozerAIView::boredTime() const noexcept
{
    return load<float>(adjustedModuleData(data_), 0x50);
}

SupplyTruckAIView::SupplyTruckAIView(void* adjustedStorage) noexcept
    : data_(bytes(adjustedStorage)) {}
SupplyTruckAIView::SupplyTruckAIView(const void* adjustedStorage) noexcept
    : data_(const_cast<std::byte*>(bytes(adjustedStorage))) {}

std::uint32_t SupplyTruckAIView::isCurrentlyFerryingSupplies() const noexcept
{
    const auto* machine = pointer(data_, 0x04);
    if (machine == nullptr) {
        return 0;
    }
    const auto* current = pointer(machine, 0x04);
    if (current == nullptr) {
        return 0;
    }
    const auto id = load<std::uint32_t>(current, 0x04);
    switch (id) {
    case 2:
    case 4:
    case 5:
        return (id & 0xffffff00U) | 1U;
    default:
        return id & 0xffffff00U;
    }
}

std::uint32_t SupplyTruckAIView::loseOneBox(const SupplyEvents& events) noexcept
{
    auto upper = pointerBits(data_) & 0xffffff00U;
    const auto current = load<std::uint32_t>(data_, 0x40);
    if (current == 0) {
        return upper;
    }
    const auto next = current - 1U;
    store(data_, 0x40, next);
    auto* owner = adjustedOwner(data_);
    if (owner != nullptr && load<std::uintptr_t>(owner, 0xf4) != 0 &&
        events.drawableSupplyChanged != nullptr) {
        const auto maximum = load<std::int32_t>(adjustedModuleData(data_), 0x4c);
        upper = events.drawableSupplyChanged(
            events.context, owner, maximum, static_cast<std::int32_t>(next)) &
            0xffffff00U;
    }
    return upper | 1U;
}

std::uint32_t SupplyTruckAIView::forceWanting() const noexcept
{
    const auto* moduleData = adjustedModuleData(data_);
    return byteInRegister(moduleData, 0x6f);
}

std::uint32_t SupplyTruckAIView::forceBusy() const noexcept
{
    const auto* moduleData = adjustedModuleData(data_);
    return byteInRegister(moduleData, 0x70);
}

void SupplyTruckAIView::copyPosition24(void* output) const noexcept
{
    auto* destination = bytes(output);
    store(destination, 0, load<std::uint32_t>(data_, 0x24));
    store(destination, 4, load<std::uint32_t>(data_, 0x28));
    store(destination, 8, load<std::uint32_t>(data_, 0x2c));
}

float SupplyTruckAIView::warehouseScanDistance() const noexcept
{
    return load<float>(adjustedModuleData(data_), 0x5c);
}

std::uint32_t SupplyTruckAIView::dataFlag6c() const noexcept
{
    const auto* moduleData = adjustedModuleData(data_);
    return byteInRegister(moduleData, 0x6c);
}

std::uint32_t SupplyTruckAIView::dataFlag6d() const noexcept
{
    const auto* moduleData = adjustedModuleData(data_);
    return byteInRegister(moduleData, 0x6d);
}

std::uint32_t SupplyTruckAIView::dataFlag6e() const noexcept
{
    const auto* moduleData = adjustedModuleData(data_);
    return byteInRegister(moduleData, 0x6e);
}

float SupplyTruckAIView::dataFloat60() const noexcept
{
    return load<float>(adjustedModuleData(data_), 0x60);
}

std::int32_t SupplyTruckAIView::centerDelay(const LogicTiming& timing) const noexcept
{
    return ai_port::scaledFrames(load<float>(adjustedModuleData(data_), 0x64), timing);
}

std::int32_t SupplyTruckAIView::warehouseDelay(const LogicTiming& timing) const noexcept
{
    return ai_port::scaledFrames(load<float>(adjustedModuleData(data_), 0x68), timing);
}

UnitAIView::UnitAIView(void* storage) noexcept : data_(bytes(storage)) {}
UnitAIView::UnitAIView(const void* storage) noexcept
    : data_(const_cast<std::byte*>(bytes(storage))) {}

void UnitAIView::setValue0144(std::uint32_t value) noexcept
{
    store(data_, 0x144, value);
}

void UnitAIView::resetStateFlags() noexcept
{
    store(data_, 0x430, std::uint8_t{1});
#if defined(RA3_AISTATE_ORACLE_MUTANT)
    store(data_, 0x431, std::uint8_t{1});
#else
    store(data_, 0x431, std::uint8_t{0});
#endif
    store(data_, 0x144, std::uint32_t{0});
}

std::uint32_t UnitAIView::nestedFlag1e() const noexcept
{
    const auto* nested = pointer(data_, 0x47c);
    if (nested == nullptr) {
        return 1;
    }
    const auto* value = pointer(nested, 0x14);
    return byteInRegister(value, 0x1e);
}

void UnitAIView::setGoalPosition(const void* position) noexcept
{
    const auto* source = bytes(position);
    store(data_, 0x21c, std::uint32_t{2});
    store(data_, 0x220, load<std::uint32_t>(source, 0));
    store(data_, 0x224, load<std::uint32_t>(source, 4));
    store(data_, 0x228, load<std::uint32_t>(source, 8));
}

void UnitAIView::clearGoal() noexcept
{
    store(data_, 0x21c, std::uint32_t{0});
}

std::uint32_t UnitAIView::selectedWeaponUsable() const noexcept
{
    const auto* weapon = pointer(data_, 0x214);
    if (weapon == nullptr) {
        return 0;
    }
    const auto* level1 = pointer(weapon, 0x04);
    const auto* level2 = pointer(level1, 0x04);
    const auto status = load<std::uint32_t>(level2, 0x64);
    const auto upper = pointerBits(weapon) & 0xffffff00U;
    if ((status == 2 || status == 3) &&
        0.0F < load<float>(weapon, 0x54) + load<float>(weapon, 0x44)) {
        return upper | 1U;
    }
    return upper | std::uint32_t(status == 4);
}

void UnitAIView::clearPendingObjectIds() noexcept
{
    store(data_, 0x49c, std::uint32_t{0xffffffffU});
    store(data_, 0x4fc, std::uint32_t{0});
}

std::uint32_t UnitAIView::objectMatchesAttackIds(const void* object) const noexcept
{
    const auto* machine = pointer(data_, 0x3c);
    const auto* queued = pointer(machine, 0x88);
    const auto upperSource = pointerBits(queued);
    bool attack = queued != nullptr && load<std::uint32_t>(queued, 0x04) == 0x1a;
    if (!attack) {
        const auto* current = pointer(machine, 0x04);
        attack = current != nullptr && load<std::uint32_t>(current, 0x04) == 0x1a;
    }
    if (attack) {
        const auto id = load<std::uint32_t>(bytes(object), 0xdc);
        attack = id == load<std::uint32_t>(data_, 0x1e8) ||
                 id == load<std::uint32_t>(data_, 0x1ec);
    }
    return (upperSource & 0xffffff00U) | std::uint32_t(attack);
}

std::uint32_t UnitAIView::currentStateIs69Or6a() const noexcept
{
    const auto* machine = pointer(data_, 0x3c);
    const auto* queued = pointer(machine, 0x88);
    const auto* current = pointer(machine, 0x04);
    const auto queuedId = stateId(queued);
    const auto currentId = stateId(current);
    if (queuedId == 0x69 || currentId == 0x69 || queuedId == 0x6a) {
        return (pointerBits(machine) & 0xffffff00U) | 1U;
    }
    return (pointerBits(current) & 0xffffff00U) | std::uint32_t(currentId == 0x6a);
}

std::uint32_t UnitAIView::value0250() const noexcept
{
    return load<std::uint32_t>(data_, 0x250);
}

std::uint32_t UnitAIView::currentOrQueuedStateId() const noexcept
{
    const auto* machine = pointer(data_, 0x3c);
    if (machine == nullptr) {
        return 0x70;
    }
    const auto* queued = pointer(machine, 0x88);
    if (queued != nullptr) {
        const auto queuedId = load<std::uint32_t>(queued, 0x04);
        if (queuedId != 999999U) {
            return queuedId;
        }
    }
    const auto* current = pointer(machine, 0x04);
    return stateId(current);
}

std::uint32_t UnitAIView::hasQueuedState() const noexcept
{
    const auto* machine = pointer(data_, 0x3c);
    return std::uint32_t(machine != nullptr && pointer(machine, 0x88) != nullptr &&
                         load<std::int32_t>(machine, 0x8c) == -1);
}

std::uint32_t UnitAIView::currentStateId() const noexcept
{
    const auto* machine = pointer(data_, 0x3c);
    return stateId(pointer(machine, 0x04));
}

void* UnitAIView::currentStateConditionBlock() const noexcept
{
    const auto machine = load<std::uintptr_t>(data_, 0x3c);
    return reinterpret_cast<void*>(machine + 0x2cU);
}

void* UnitAIView::currentStateTransitionBlock() const noexcept
{
    const auto machine = load<std::uintptr_t>(data_, 0x3c);
    return reinterpret_cast<void*>(machine + 0x58U);
}

void UnitAIView::setDesiredSpeedBits(std::uint32_t bits) noexcept
{
    store(data_, 0x188, bits);
}

std::int32_t UnitAIView::scaledModuleTime(const LogicTiming& timing) const noexcept
{
    if (timing.gameLogic == nullptr) {
        return 0;
    }
    const auto* moduleData = pointer(data_, 0x04);
    return x87ProductToLowI32(
        timing.timeScale, timing.framesPerSecond, load<float>(moduleData, 0x30));
}

void UnitAIView::clearCommandLatch(const GameDispatch& dispatch) noexcept
{
    const std::uint32_t reason = 4;
    dispatchCall(dispatch, data_, 0x5c, &reason, 1);
    store(data_, 0x30, std::uint32_t{0});
}

std::int32_t UnitAIView::randomizedDelay(const LogicTiming& timing) const noexcept
{
    const auto* moduleData = pointer(data_, 0x04);
    const auto base = ai_port::scaledFrames(load<float>(moduleData, 0x28), timing);
    const auto* owner = pointer(data_, 0x08);
    const auto ownerId = load<std::uint32_t>(owner, 0xdc);
    const auto result = static_cast<std::uint32_t>(base) +
        ownerId % (timing.integerFramesPerSecond >> 1U);
    return static_cast<std::int32_t>(result);
}

WeaponStateView::WeaponStateView(void* storage) noexcept : data_(bytes(storage)) {}
WeaponStateView::WeaponStateView(const void* storage) noexcept
    : data_(const_cast<std::byte*>(bytes(storage))) {}

StateResult WeaponStateView::updateReady(const WeaponEvents& events) const noexcept
{
    const auto* weapon = pointer(data_, 0x20);
    if (load<std::uint8_t>(weapon, 0x60) != 0) {
        return StateResult::success;
    }
    if (events.readyStateNotified != nullptr) {
        const auto* machine = pointer(data_, 0x18);
        events.readyStateNotified(events.context, load<std::uint32_t>(machine, 0x84));
    }
    return StateResult::continueState;
}

StateResult WeaponStateView::updateCoolDown(const LogicTiming& timing) noexcept
{
    const auto finish = load<std::uint32_t>(data_, 0x28) + load<std::uint32_t>(data_, 0x24);
    if (timing.frame < finish) {
        return StateResult::continueState;
    }
    const auto* weapon = pointer(data_, 0x20);
    const auto* nested1 = pointer(weapon, 0x04);
    const auto* nested2 = pointer(nested1, 0x04);
    const auto mode = load<std::uint32_t>(nested2, 0xb0);
    if (load<std::uint8_t>(weapon, 0x60) == 0) {
        store(data_, 0x08, std::uint32_t{0});
    } else if (mode != 0 && mode != 3 && mode != 4) {
        store(data_, 0x08, std::uint32_t{2});
    } else {
        store(data_, 0x08, std::uint32_t{1});
    }
    return StateResult::success;
}

StateResult WeaponStateView::updateReload(const LogicTiming& timing) const noexcept
{
    const auto finish = load<std::uint32_t>(data_, 0x28) + load<std::uint32_t>(data_, 0x24);
    return timing.frame < finish ? StateResult::continueState : StateResult::success;
}

namespace ai_port {

std::int32_t scaledFrames(float value, const LogicTiming& timing) noexcept
{
    return timing.gameLogic == nullptr
        ? x87ProductToLowI32(timing.framesPerSecond, value, 1.0F)
        : x87ProductToLowI32(timing.timeScale, timing.framesPerSecond, value);
}

std::int32_t oneSecondFrames(const LogicTiming& timing) noexcept
{
    return timing.gameLogic == nullptr
        ? x87ProductToLowI32(timing.framesPerSecond, 1.0F, 1.0F)
        : x87ProductToLowI32(timing.timeScale, timing.framesPerSecond, 1.0F);
}

}  // namespace ai_port
}  // namespace ra3::gamelogic
