#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

enum class StateResult : std::int32_t {
    failure = -2,
    success = -1,
    continueState = 0,
};

// Values that the original code reads from TheGameLogic and GlobalData.
// Keeping them explicit lets offline users exercise the recovered layouts
// without pretending that game-owned singletons are available.
struct LogicTiming {
    const void* gameLogic{};
    std::uint32_t frame{};
    float timeScale{1.0F};
    float framesPerSecond{30.0F};
    std::uint32_t integerFramesPerSecond{30};
};

struct SupplyEvents {
    void* context{};
    std::uint32_t (*drawableSupplyChanged)(
        void* context, void* owner, std::int32_t maximum, std::int32_t current) noexcept{};
};

struct WeaponEvents {
    void* context{};
    void (*readyStateNotified)(void* context, std::uint32_t value) noexcept{};
};

struct GameDispatch {
    void* context{};
    std::uint32_t (*invoke)(
        void* context, void* target, std::uint32_t vtableOffset,
        const std::uint32_t* arguments, std::size_t argumentCount) noexcept{};
};

// A State-derived object. This is a non-owning view: no constructor,
// destructor, allocator, or ownership claim is made for game memory.
class StateView {
public:
    explicit StateView(void* storage) noexcept;
    explicit StateView(const void* storage) noexcept;

    [[nodiscard]] void* machine() const noexcept;
    [[nodiscard]] std::uint32_t value20() const noexcept;
    void setValue20(std::uint32_t value) noexcept;

    // DozerPrimaryIdleState, 0x00681BB0/0x00681BD0.
    StateResult enterDozerPrimaryIdle(std::uint32_t frame) noexcept;
    void exitDozerPrimaryIdle() noexcept;
    [[nodiscard]] StateResult enterDozerAction(
        std::uint32_t frame, const GameDispatch& dispatch) noexcept;        // 0x006819B0
    void exitDozerAction(const GameDispatch& dispatch) noexcept;           // 0x00681A00
    [[nodiscard]] StateResult enterNestedDozerAction(
        const GameDispatch& dispatch) noexcept;                            // 0x00681C80
    void exitNestedDozerAction(const GameDispatch& dispatch) noexcept;     // 0x00681CC0
    [[nodiscard]] StateResult enterSupplyBusy(
        const GameDispatch& dispatch) noexcept;                            // 0x00682310
    [[nodiscard]] StateResult enterSupplyIdle(
        const GameDispatch& dispatch) noexcept;                            // 0x00682350
    [[nodiscard]] StateResult enterSupplyDocking(
        const GameDispatch& dispatch) noexcept;                            // 0x006823C0

    static constexpr std::size_t minimumRecoveredSize = 0x29;

private:
    std::byte* data_;
};

// Secondary DozerAIInterface subobject. In RA3 this interface is +0x518 from
// the update-module base; negative offsets below are therefore intentional.
class DozerAIView {
public:
    explicit DozerAIView(void* adjustedStorage) noexcept;
    explicit DozerAIView(const void* adjustedStorage) noexcept;

    [[nodiscard]] std::uint32_t hasTask(std::uint32_t task) const noexcept; // 0x00681E80
    [[nodiscard]] std::uint32_t hasAnyTask(const GameDispatch& dispatch) const noexcept;
    [[nodiscard]] std::uint32_t taskTarget(std::uint32_t task) const noexcept; // 0x00681EC0
    [[nodiscard]] std::int32_t mostRecentTask(const GameDispatch& dispatch) const noexcept;
    [[nodiscard]] const void* dockPoint(
        std::uint32_t task, std::uint32_t point) const noexcept;           // 0x00681F60
    [[nodiscard]] float repairHealthPerSecond() const noexcept;            // 0x00681FA0
    [[nodiscard]] float boredTime() const noexcept;                        // 0x00681FB0

private:
    std::byte* data_;
};

// Secondary SupplyTruckAIInterface subobject (+0x518 from the module base).
class SupplyTruckAIView {
public:
    explicit SupplyTruckAIView(void* adjustedStorage) noexcept;
    explicit SupplyTruckAIView(const void* adjustedStorage) noexcept;

    [[nodiscard]] std::uint32_t isCurrentlyFerryingSupplies() const noexcept; // 0x00682090
    [[nodiscard]] std::uint32_t loseOneBox(const SupplyEvents& events = {}) noexcept; // 0x006820D0
    [[nodiscard]] std::uint32_t forceWanting() const noexcept;              // 0x00682170
    [[nodiscard]] std::uint32_t forceBusy() const noexcept;                 // 0x00682180
    void copyPosition24(void* output) const noexcept;                       // 0x00682190
    [[nodiscard]] float warehouseScanDistance() const noexcept;            // 0x006821B0
    [[nodiscard]] std::uint32_t dataFlag6c() const noexcept;                // 0x006821C0
    [[nodiscard]] std::uint32_t dataFlag6d() const noexcept;                // 0x006821D0
    [[nodiscard]] std::uint32_t dataFlag6e() const noexcept;                // 0x006821E0
    [[nodiscard]] float dataFloat60() const noexcept;                       // 0x006821F0
    [[nodiscard]] std::int32_t centerDelay(const LogicTiming& timing) const noexcept; // 0x00682200
    [[nodiscard]] std::int32_t warehouseDelay(const LogicTiming& timing) const noexcept; // 0x00682280

private:
    std::byte* data_;
};

// Common AIUpdateInterface storage used directly by the AIStates.cpp cluster.
class UnitAIView {
public:
    explicit UnitAIView(void* storage) noexcept;
    explicit UnitAIView(const void* storage) noexcept;

    void setValue0144(std::uint32_t value) noexcept;                        // 0x00682D30
    void resetStateFlags() noexcept;                                       // 0x00682D70
    [[nodiscard]] std::uint32_t nestedFlag1e() const noexcept;             // 0x00682DF0
    void setGoalPosition(const void* position) noexcept;                    // 0x00682E40
    void clearGoal() noexcept;                                             // 0x00682E70
    [[nodiscard]] std::uint32_t selectedWeaponUsable() const noexcept;     // 0x00682E80
    void clearPendingObjectIds() noexcept;                                 // 0x00682F00
    [[nodiscard]] std::uint32_t objectMatchesAttackIds(const void* object) const noexcept; // 0x00682FE0
    [[nodiscard]] std::uint32_t currentStateIs69Or6a() const noexcept;     // 0x00683030
    [[nodiscard]] std::uint32_t value0250() const noexcept;                // 0x006830F0
    [[nodiscard]] std::uint32_t currentOrQueuedStateId() const noexcept;    // 0x00683100
    [[nodiscard]] std::uint32_t hasQueuedState() const noexcept;           // 0x00683320
    [[nodiscard]] std::uint32_t currentStateId() const noexcept;           // 0x00683520
    [[nodiscard]] void* currentStateConditionBlock() const noexcept;       // 0x00683570
    [[nodiscard]] void* currentStateTransitionBlock() const noexcept;      // 0x00683580
    void setDesiredSpeedBits(std::uint32_t bits) noexcept;                 // 0x00683590
    [[nodiscard]] std::int32_t scaledModuleTime(const LogicTiming& timing) const noexcept; // 0x006835B0
    void clearCommandLatch(const GameDispatch& dispatch) noexcept;
    [[nodiscard]] std::int32_t randomizedDelay(const LogicTiming& timing) const noexcept; // 0x00683670

    static constexpr std::size_t minimumRecoveredSize = 0x504;

private:
    std::byte* data_;
};

class WeaponStateView {
public:
    explicit WeaponStateView(void* storage) noexcept;
    explicit WeaponStateView(const void* storage) noexcept;

    [[nodiscard]] StateResult updateReady(const WeaponEvents& events = {}) const noexcept; // 0x00685CC0
    [[nodiscard]] StateResult updateCoolDown(const LogicTiming& timing) noexcept; // 0x00685D20
    [[nodiscard]] StateResult updateReload(const LogicTiming& timing) const noexcept; // 0x00685DA0

private:
    std::byte* data_;
};

namespace ai_port {

[[nodiscard]] std::int32_t scaledFrames(float value, const LogicTiming& timing) noexcept;
[[nodiscard]] std::int32_t oneSecondFrames(const LogicTiming& timing) noexcept; // 0x00682890

}  // namespace ai_port

}  // namespace ra3::gamelogic
