#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::ui {

struct Point {
    std::int32_t x;
    std::int32_t y;
};

struct Rect {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct Pair {
    std::uint32_t first;
    std::uint32_t second;
};

struct PointerEvent {
    std::int32_t x;
    std::int32_t y;
    std::uint32_t value;
    std::uint32_t timestamp;
};

struct TypedEvent {
    std::uint32_t tag;
    const void* payload;
};

struct TextSlice {
    const void* sourceHandle;
    std::uint32_t start;
    std::uint32_t count;
};

enum class CallbackOp : std::uint32_t {
    queryGuard,
    applyActive,
    queryCondition,
    applyCondition,
    measureTextIndex,
    cancelTimerToken,
    timerExpired,
    startTimerToken,
    releaseOwned,
    setPosition,
    setExtent,
    setClip,
    preprocessInput,
    suppressRelease,
    dispatchMessage,
    dispatchHeld,
    delayedUpdate,
    queryControllerActive,
    queryControllerBlocked,
    applyControllerActive,
    queryChildReady,
    applyControllerReady,
    resolveBitName,
    notifyPrimaryBit,
    notifySecondaryBit,
    hitTestCaret,
    coalescedPointer,
    fallbackPointer,
};

enum class PayloadAlias : std::uint32_t {
    none,
    input,
    localCopy,
};

using CallbackFunction = std::uint32_t (*)(
    void* context, CallbackOp operation, const std::uint32_t* arguments,
    std::size_t argumentCount);

struct Callbacks {
    void* context{};
    CallbackFunction function{};

    std::uint32_t invoke(
        CallbackOp operation, const std::uint32_t* arguments = nullptr,
        std::size_t argumentCount = 0) const noexcept;
};

class LayoutView {
public:
    explicit LayoutView(void* memory) noexcept;

    void* data() const noexcept;
    std::uint8_t byte(std::size_t offset) const noexcept;
    void setByte(std::size_t offset, std::uint8_t value) const noexcept;
    std::uint32_t word(std::size_t offset) const noexcept;
    void setWord(std::size_t offset, std::uint32_t value) const noexcept;

private:
    std::uint8_t* memory_;
};

class InputRouterView {
public:
    explicit InputRouterView(void* memory) noexcept;

    bool hasParent() const noexcept;
    std::int32_t capturedSlot() const noexcept;
    void setCapturedSlot(std::int32_t slot) const noexcept;
    std::uint32_t slotWord(std::uint32_t slot, std::uint32_t word) const noexcept;
    void setSlotWord(std::uint32_t slot, std::uint32_t word, std::uint32_t value) const noexcept;
    bool slotActive(std::uint32_t slot) const noexcept;
    void setSlotActive(std::uint32_t slot, bool active) const noexcept;
    bool slotAlternate(std::uint32_t slot) const noexcept;
    void setSlotAlternate(std::uint32_t slot, bool alternate) const noexcept;

private:
    LayoutView layout_;
};

class HitRegionView {
public:
    explicit HitRegionView(void* memory) noexcept;

    Point* begin() const noexcept;
    Point* end() const noexcept;
    void setPoints(Point* begin, Point* end, Point* capacity) const noexcept;
    bool boundsValid() const noexcept;
    void setBoundsValid(bool valid) const noexcept;
    Rect bounds() const noexcept;
    void setBounds(const Rect& bounds) const noexcept;
    void* data() const noexcept;

private:
    LayoutView layout_;
};

class TextEditView {
public:
    explicit TextEditView(void* memory) noexcept;

    const char16_t* text() const noexcept;
    std::uint32_t caret() const noexcept;
    void setCaret(std::uint32_t value) const noexcept;
    std::uint32_t anchor() const noexcept;
    void setAnchor(std::uint32_t value) const noexcept;
    void setSelection(std::uint32_t begin, std::uint32_t end) const noexcept;
    std::uint32_t selectionMode() const noexcept;
    void setSelectionMode(std::uint32_t value) const noexcept;
    std::uint8_t flags() const noexcept;
    bool mouseSelectionActive() const noexcept;
    void setMouseSelectionActive(bool value) const noexcept;
    void* data() const noexcept;

private:
    LayoutView layout_;
};

// ra3_1.0.game 0x00A70060, 0x00A700D0, 0x00A70110.
std::uint32_t setControlActive(
    LayoutView control, bool active, const Callbacks& callbacks) noexcept;
std::uint32_t propagateEnabledState(
    LayoutView control, const Callbacks& callbacks) noexcept;
std::uint32_t propagateFocusState(
    LayoutView control, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A701A0.
using TextMeasureFunction = bool (*)(
    void* context, std::uint32_t candidate, std::uint32_t& measured) noexcept;
std::uint32_t fitTextIndex(
    std::uint32_t candidate, void* context, TextMeasureFunction measure) noexcept;
std::uint32_t fitTextIndex(
    LayoutView control, std::uint32_t candidate, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A70220, 0x00A70290.
void tickUiTimer(LayoutView timer, std::uint32_t now, const Callbacks& callbacks) noexcept;
void startUiTimer(LayoutView timer, std::uint32_t now, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A704B0, 0x00A70500, 0x00A70520, 0x00A70540.
void clearPendingAction(LayoutView owner, const Callbacks& callbacks) noexcept;
void setControlPosition(
    LayoutView control, std::uint32_t position, const Callbacks& callbacks) noexcept;
void setControlExtent(
    LayoutView control, std::uint32_t first, std::uint32_t second,
    const Callbacks& callbacks) noexcept;
void setControlClip(
    LayoutView control, std::uint32_t clip, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A705A0, 0x00A70670, 0x00A70700.
std::uint32_t controllerUiObject(LayoutView controller) noexcept;
std::uint32_t controllerInputTable(LayoutView controller) noexcept;
std::uint32_t countControlFlags(const std::uint32_t words[2]) noexcept;

// ra3_1.0.game 0x00A71200, 0x00A712C0, 0x00A71340.
void dispatchInputRelease(
    InputRouterView router, std::uint32_t slot, const std::uint32_t payload[4],
    const Callbacks& callbacks) noexcept;
std::uint32_t routeIdleInput(
    InputRouterView router, const std::uint32_t payload[4],
    const Callbacks& callbacks) noexcept;
std::uint32_t cancelInputCapture(
    InputRouterView router, std::uint32_t reason, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A6FFE0, 0x00A71B20, 0x00A71B40, 0x00A71B70.
void updateHitRegionBounds(HitRegionView region) noexcept;
void initializeHitRegion(HitRegionView region) noexcept;
Rect copyHitRegionBounds(HitRegionView region) noexcept;
bool hitRegionContains(HitRegionView region, const Point& point) noexcept;

// ra3_1.0.game 0x00A71E80.
void moveTextCaretLeft(TextEditView edit, std::uint32_t keyModifiers) noexcept;

// ra3_1.0.game 0x00A72380.
void synchronizeControllerState(LayoutView controller, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A72660.
using ControlBitResolver = std::uint32_t (*)(
    void* context, std::uint32_t nameToken, bool& success) noexcept;
bool setNamedControlBit(
    std::uint32_t* words, std::uint32_t nameToken, void* context,
    ControlBitResolver resolve) noexcept;

// ra3_1.0.game 0x00A72D30, 0x00A72DC0, 0x00A72E20.
void deactivateEventGate(LayoutView gate, const Callbacks& callbacks) noexcept;
void dispatchActiveInputSlots(
    InputRouterView router, const bool blocked[3], const Pair& pointer,
    const Callbacks& callbacks) noexcept;
std::uint32_t routeTypedInputEvent(
    InputRouterView router, const TypedEvent& event, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A730B0, 0x00A73130.
void setPrimaryControlBit(
    LayoutView control, std::uint32_t index, bool value, const Callbacks& callbacks) noexcept;
void setSecondaryControlBit(
    LayoutView control, std::uint32_t index, bool value, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A73590, 0x00A735F0, 0x00A73640.
TextSlice makeTextSlice(
    const void* sourceHandle, std::int32_t start, std::int32_t count) noexcept;
TextSlice makeTextPrefix(const void* sourceHandle, std::int32_t count) noexcept;
TextSlice makeTextSuffix(const void* sourceHandle, std::int32_t count) noexcept;

std::uint32_t dispatchUiMessage107(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage108(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage109(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10A(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10B(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10C(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10D(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10E(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage10F(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage110(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage111(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage112(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage113(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage114(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;
std::uint32_t dispatchUiMessage115(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A74700, 0x00A748F0.
std::uint32_t routeNearPointerEvent(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept;
void activateEventGate(LayoutView gate, const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A74E70, 0x00A74EC0.
void beginTextSelection(
    TextEditView edit, std::int32_t x, std::int32_t y,
    const Callbacks& callbacks) noexcept;
void updateTextSelectionFromPoint(
    TextEditView edit, Point& lastPoint, std::int32_t x, std::int32_t y,
    const Callbacks& callbacks) noexcept;

// ra3_1.0.game 0x00A75570, 0x00A759F0, 0x00A75A60.
std::uint32_t forwardUiMessage106(
    const std::uint32_t payload[3], bool hasSink, const Callbacks& callbacks) noexcept;
std::uint32_t routePrimaryDoubleClick(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept;
std::uint32_t routeSecondaryDoubleClick(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept;

}  // namespace ra3::ui
