#include <ra3/ui/UiControl.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace ra3::ui {
namespace {

constexpr std::uint32_t tagCancel = 0x00E2A102u;
constexpr std::uint32_t tagActivate = 0x00E2A103u;
constexpr std::uint32_t tagForward106 = 0x00E2A106u;
constexpr std::uint32_t tagPointer = 0x00DEBD52u;
constexpr std::array<std::uint32_t, 3> pressTags{
    0x00DEBD46u, 0x00DEBD4Au, 0x00DEBD4Eu};
constexpr std::array<std::uint32_t, 3> holdTags{
    0x00DEBD45u, 0x00DEBD49u, 0x00DEBD4Du};
constexpr std::array<std::uint32_t, 3> releaseTags{
    0x00DEBD47u, 0x00DEBD4Bu, 0x00DEBD4Fu};
constexpr std::array<std::uint32_t, 3> normalReleaseTags{
    0x00E2A107u, 0x00E2A10Cu, 0x00E2A111u};
constexpr std::array<std::uint32_t, 3> alternateReleaseTags{
    0x00E2A108u, 0x00E2A10Du, 0x00E2A112u};
constexpr std::array<std::uint32_t, 3> capturedReleaseTags{
    0x00E2A10Au, 0x00E2A10Fu, 0x00E2A114u};

std::uint32_t pointerWord(const void* pointer) noexcept
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T>
T* pointerFromWord(std::uint32_t value) noexcept
{
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(value));
}

std::uint32_t dispatchMessage(
    const Callbacks& callbacks, std::uint32_t tag, const std::uint32_t* payload,
    std::size_t count, PayloadAlias alias) noexcept
{
    std::array<std::uint32_t, 8> arguments{};
    arguments[0] = tag;
    arguments[1] = static_cast<std::uint32_t>(count);
    arguments[2] = static_cast<std::uint32_t>(alias);
    for (std::size_t i = 0; i < count; ++i) {
        arguments[i + 3] = payload[i];
    }
    return callbacks.invoke(CallbackOp::dispatchMessage, arguments.data(), count + 3);
}

std::uint32_t dispatchShortMessage(
    const Callbacks& callbacks, std::uint32_t tag, const Pair& pair,
    std::uint32_t value) noexcept
{
    const std::uint32_t payload[]{pair.first, pair.second, value};
    return dispatchMessage(callbacks, tag, payload, 3, PayloadAlias::localCopy);
}

std::uint32_t dispatchLongMessage(
    const Callbacks& callbacks, std::uint32_t tag, const Pair& pair,
    std::uint32_t value, const Pair& second) noexcept
{
    const std::uint32_t payload[]{
        pair.first, pair.second, value, second.first, second.second};
    return dispatchMessage(callbacks, tag, payload, 5, PayloadAlias::localCopy);
}

void updateBounds(HitRegionView region) noexcept
{
    Point* const begin = region.begin();
    Point* const end = region.end();
    if (begin == nullptr || begin == end) {
        region.setBounds(Rect{});
        region.setBoundsValid(true);
        return;
    }
    Rect bounds{begin->x, begin->y, begin->x, begin->y};
    for (const Point* point = begin + 1; point != end; ++point) {
        bounds.left = std::min(bounds.left, point->x);
        bounds.top = std::min(bounds.top, point->y);
        bounds.right = std::max(bounds.right, point->x);
        bounds.bottom = std::max(bounds.bottom, point->y);
    }
    region.setBounds(bounds);
    region.setBoundsValid(true);
}

bool edgeCrossesRay(const Point& query, const Point& first, const Point& second) noexcept
{
    if (first.y == second.y) {
        return false;
    }
    const bool straddles =
        first.y < query.y ? (second.y >= query.y && first.y < query.y)
                          : second.y < query.y;
    if (!straddles || (first.x < query.x && second.x < query.x)) {
        return false;
    }
    const std::uint32_t dx =
        static_cast<std::uint32_t>(second.x) - static_cast<std::uint32_t>(first.x);
    const std::uint32_t dy =
        static_cast<std::uint32_t>(query.y) - static_cast<std::uint32_t>(first.y);
    const std::int32_t product = static_cast<std::int32_t>(dx * dy);
    const float numerator = static_cast<float>(product);
    const std::int32_t denominator = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(second.y) - static_cast<std::uint32_t>(first.y));
    const float intersection =
        numerator / static_cast<float>(denominator) + static_cast<float>(first.x);
    return intersection >= static_cast<float>(query.x);
}

std::uint16_t textLength(const void* sourceHandle) noexcept
{
    if (sourceHandle == nullptr) {
        return 0;
    }
    std::uint32_t textValue = 0;
    std::memcpy(&textValue, sourceHandle, sizeof(textValue));
    if (textValue == 0) {
        return 0;
    }
    const auto* text = pointerFromWord<const std::uint8_t>(textValue);
    std::uint16_t length = 0;
    std::memcpy(&length, text - 4, sizeof(length));
    return length;
}

std::uint32_t clampSignedIndex(std::uint32_t value, std::uint32_t length) noexcept
{
    if (static_cast<std::int32_t>(value) < 0) {
        return 0;
    }
    return value > length ? length : value;
}

bool deltaLess(std::int32_t first, std::int32_t second, std::int32_t limit) noexcept
{
    std::uint32_t difference =
        static_cast<std::uint32_t>(first) - static_cast<std::uint32_t>(second);
    if (static_cast<std::int32_t>(difference) < 0) {
        difference = 0u - difference;
    }
    return static_cast<std::int32_t>(difference) < limit;
}

bool nestedSinkPresent(LayoutView router) noexcept
{
    const std::uint32_t ownerValue = router.word(4);
    if (ownerValue == 0) {
        return false;
    }
    LayoutView owner(pointerFromWord<void>(ownerValue));
    return owner.word(4) != 0;
}

void applyHitCaret(TextEditView edit, std::uint32_t caret) noexcept
{
    if (edit.selectionMode() != 0) {
        const std::uint32_t anchor = edit.anchor();
        edit.setSelection(std::min(caret, anchor), std::max(caret, anchor));
    }
    edit.setCaret(caret);
}

std::uint32_t routeDoubleClick(
    LayoutView router, const PointerEvent& event, bool requiredMode,
    std::uint32_t fallbackTag, const Callbacks& callbacks) noexcept
{
    const bool mode = router.byte(8) != 0;
    const std::uint32_t elapsed = event.timestamp - router.word(0x18);
    if (mode == requiredMode
        && deltaLess(event.x, static_cast<std::int32_t>(router.word(0x0C)), 5)
        && deltaLess(event.y, static_cast<std::int32_t>(router.word(0x10)), 5)
        && elapsed < 500u) {
        const std::uint32_t arguments[]{
            static_cast<std::uint32_t>(event.x), static_cast<std::uint32_t>(event.y),
            event.value, event.timestamp};
        callbacks.invoke(CallbackOp::coalescedPointer, arguments, 4);
        return 0;
    }
    const std::uint32_t arguments[]{
        fallbackTag, static_cast<std::uint32_t>(event.x), static_cast<std::uint32_t>(event.y),
        event.value, event.timestamp};
    return callbacks.invoke(CallbackOp::fallbackPointer, arguments, 5);
}

void setControlBit(
    LayoutView control, std::size_t offset, CallbackOp operation,
    std::uint32_t index, bool value, const Callbacks& callbacks) noexcept
{
    const std::uint32_t mask = 1u << (index & 31u);
    const std::size_t wordOffset = offset + static_cast<std::size_t>(index >> 5u) * 4u;
    const std::uint32_t oldValue = control.word(wordOffset);
    if (((oldValue & mask) != 0) == value) {
        return;
    }
    const std::uint32_t newValue = value ? oldValue | mask : oldValue & ~mask;
    control.setWord(wordOffset, newValue);
    const std::uint32_t arguments[]{
        index, value ? 1u : 0u,
        control.word(0x11C + static_cast<std::size_t>(index) * 4u)};
    callbacks.invoke(operation, arguments, 3);
}

struct FitMeasureContext {
    const Callbacks* callbacks;
};

bool measureCandidate(
    void* const opaque, const std::uint32_t candidate, std::uint32_t& measured) noexcept
{
    const auto* const context = static_cast<const FitMeasureContext*>(opaque);
    measured = candidate;
    return (context->callbacks->invoke(CallbackOp::measureTextIndex, &candidate, 1)
            & 0xFFu)
        != 0;
}

}  // namespace

std::uint32_t Callbacks::invoke(
    CallbackOp operation, const std::uint32_t* arguments,
    std::size_t argumentCount) const noexcept
{
    return function == nullptr ? 0 : function(context, operation, arguments, argumentCount);
}

LayoutView::LayoutView(void* memory) noexcept
    : memory_(static_cast<std::uint8_t*>(memory))
{
}

void* LayoutView::data() const noexcept
{
    return memory_;
}

std::uint8_t LayoutView::byte(std::size_t offset) const noexcept
{
    return memory_[offset];
}

void LayoutView::setByte(std::size_t offset, std::uint8_t value) const noexcept
{
    memory_[offset] = value;
}

std::uint32_t LayoutView::word(std::size_t offset) const noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, memory_ + offset, sizeof(value));
    return value;
}

void LayoutView::setWord(std::size_t offset, std::uint32_t value) const noexcept
{
    std::memcpy(memory_ + offset, &value, sizeof(value));
}

InputRouterView::InputRouterView(void* memory) noexcept
    : layout_(memory)
{
}

bool InputRouterView::hasParent() const noexcept
{
    return layout_.word(4) != 0;
}

std::int32_t InputRouterView::capturedSlot() const noexcept
{
    return static_cast<std::int32_t>(layout_.word(8));
}

void InputRouterView::setCapturedSlot(std::int32_t slot) const noexcept
{
    layout_.setWord(8, static_cast<std::uint32_t>(slot));
}

std::uint32_t InputRouterView::slotWord(
    std::uint32_t slot, std::uint32_t wordIndex) const noexcept
{
    return layout_.word(0x0C + static_cast<std::size_t>(slot) * 0x14u
                        + static_cast<std::size_t>(wordIndex) * 4u);
}

void InputRouterView::setSlotWord(
    std::uint32_t slot, std::uint32_t wordIndex, std::uint32_t value) const noexcept
{
    layout_.setWord(0x0C + static_cast<std::size_t>(slot) * 0x14u
                    + static_cast<std::size_t>(wordIndex) * 4u, value);
}

bool InputRouterView::slotActive(std::uint32_t slot) const noexcept
{
    return layout_.byte(0x1C + static_cast<std::size_t>(slot) * 0x14u) != 0;
}

void InputRouterView::setSlotActive(std::uint32_t slot, bool active) const noexcept
{
    layout_.setByte(
        0x1C + static_cast<std::size_t>(slot) * 0x14u,
        static_cast<std::uint8_t>(active));
}

bool InputRouterView::slotAlternate(std::uint32_t slot) const noexcept
{
    return layout_.byte(0x1D + static_cast<std::size_t>(slot) * 0x14u) != 0;
}

void InputRouterView::setSlotAlternate(std::uint32_t slot, bool alternate) const noexcept
{
    layout_.setByte(
        0x1D + static_cast<std::size_t>(slot) * 0x14u,
        static_cast<std::uint8_t>(alternate));
}

HitRegionView::HitRegionView(void* memory) noexcept
    : layout_(memory)
{
}

Point* HitRegionView::begin() const noexcept
{
    return pointerFromWord<Point>(layout_.word(0));
}

Point* HitRegionView::end() const noexcept
{
    return pointerFromWord<Point>(layout_.word(4));
}

void HitRegionView::setPoints(Point* beginValue, Point* endValue, Point* capacity) const noexcept
{
    layout_.setWord(0, pointerWord(beginValue));
    layout_.setWord(4, pointerWord(endValue));
    layout_.setWord(8, pointerWord(capacity));
}

bool HitRegionView::boundsValid() const noexcept
{
    return layout_.byte(0x20) != 0;
}

void HitRegionView::setBoundsValid(bool valid) const noexcept
{
    layout_.setByte(0x20, static_cast<std::uint8_t>(valid));
}

Rect HitRegionView::bounds() const noexcept
{
    return Rect{
        static_cast<std::int32_t>(layout_.word(0x10)),
        static_cast<std::int32_t>(layout_.word(0x14)),
        static_cast<std::int32_t>(layout_.word(0x18)),
        static_cast<std::int32_t>(layout_.word(0x1C))};
}

void HitRegionView::setBounds(const Rect& value) const noexcept
{
    layout_.setWord(0x10, static_cast<std::uint32_t>(value.left));
    layout_.setWord(0x14, static_cast<std::uint32_t>(value.top));
    layout_.setWord(0x18, static_cast<std::uint32_t>(value.right));
    layout_.setWord(0x1C, static_cast<std::uint32_t>(value.bottom));
}

void* HitRegionView::data() const noexcept
{
    return layout_.data();
}

TextEditView::TextEditView(void* memory) noexcept
    : layout_(memory)
{
}

const char16_t* TextEditView::text() const noexcept
{
    return pointerFromWord<const char16_t>(layout_.word(8));
}

std::uint32_t TextEditView::caret() const noexcept
{
    return layout_.word(0x10);
}

void TextEditView::setCaret(std::uint32_t value) const noexcept
{
    layout_.setWord(0x10, value);
}

std::uint32_t TextEditView::anchor() const noexcept
{
    return layout_.word(0x14);
}

void TextEditView::setAnchor(std::uint32_t value) const noexcept
{
    layout_.setWord(0x14, value);
}

void TextEditView::setSelection(std::uint32_t begin, std::uint32_t end) const noexcept
{
    layout_.setWord(0x18, begin);
    layout_.setWord(0x1C, end);
}

std::uint32_t TextEditView::selectionMode() const noexcept
{
    return layout_.word(0x20);
}

void TextEditView::setSelectionMode(std::uint32_t value) const noexcept
{
    layout_.setWord(0x20, value);
}

std::uint8_t TextEditView::flags() const noexcept
{
    return layout_.byte(0x28);
}

bool TextEditView::mouseSelectionActive() const noexcept
{
    return layout_.byte(0x35) != 0;
}

void TextEditView::setMouseSelectionActive(bool value) const noexcept
{
    layout_.setByte(0x35, static_cast<std::uint8_t>(value));
}

void* TextEditView::data() const noexcept
{
    return layout_.data();
}

std::uint32_t setControlActive(
    LayoutView control, bool active, const Callbacks& callbacks) noexcept
{
    if ((control.byte(0x14) != 0) == active) {
        return static_cast<std::uint32_t>(active);
    }
    control.setByte(0x14, static_cast<std::uint8_t>(active));
    bool applied = false;
    if (active) {
        const bool guardPresent = control.word(0x0C) != 0;
        applied = !guardPresent || (callbacks.invoke(CallbackOp::queryGuard) & 0xFFu) == 0;
    }
    const std::uint32_t argument = applied ? 1u : 0u;
    return callbacks.invoke(CallbackOp::applyActive, &argument, 1);
}

std::uint32_t propagateEnabledState(
    LayoutView control, const Callbacks& callbacks) noexcept
{
    const bool value = control.byte(0x10) != 0
        && (callbacks.invoke(CallbackOp::queryCondition) & 0xFFu) == 0;
    const std::uint32_t argument = value ? 1u : 0u;
    return callbacks.invoke(CallbackOp::applyCondition, &argument, 1);
}

std::uint32_t propagateFocusState(
    LayoutView, const Callbacks& callbacks) noexcept
{
    const std::uint32_t argument =
        callbacks.invoke(CallbackOp::queryCondition) & 0xFFu;
    return callbacks.invoke(CallbackOp::applyCondition, &argument, 1);
}

std::uint32_t fitTextIndex(
    const std::uint32_t candidate, void* const context,
    const TextMeasureFunction measure) noexcept
{
    std::uint32_t measured = 0;
    return measure != nullptr && measure(context, candidate, measured) ? measured : 0;
}

std::uint32_t fitTextIndex(
    LayoutView, const std::uint32_t candidate, const Callbacks& callbacks) noexcept
{
    FitMeasureContext context{&callbacks};
    return fitTextIndex(candidate, &context, &measureCandidate);
}

void tickUiTimer(
    LayoutView timer, std::uint32_t now, const Callbacks& callbacks) noexcept
{
    if (timer.byte(0x14) == 0 || timer.byte(0x15) != 0) {
        return;
    }
    const std::uint32_t elapsed = now - timer.word(0x10);
    const std::uint32_t remaining = timer.word(0x0C);
    if (elapsed < remaining) {
        timer.setWord(0x10, now);
        timer.setWord(0x0C, remaining - elapsed);
        return;
    }
    const std::uint32_t token = timer.word(8);
    callbacks.invoke(CallbackOp::cancelTimerToken, &token, 1);
    timer.setWord(0x0C, 0);
    timer.setByte(0x15, 1);
    callbacks.invoke(CallbackOp::timerExpired);
    timer.setWord(0x10, now);
}

void startUiTimer(
    LayoutView timer, std::uint32_t now, const Callbacks& callbacks) noexcept
{
    timer.setByte(0x14, 1);
    timer.setWord(0x10, now);
    const std::uint32_t token = timer.word(8);
    callbacks.invoke(CallbackOp::startTimerToken, &token, 1);
}

void clearPendingAction(LayoutView owner, const Callbacks& callbacks) noexcept
{
    const std::uint32_t actionValue = owner.word(0x1C);
    if (actionValue == 0) {
        return;
    }
    LayoutView action(pointerFromWord<void>(actionValue));
    action.setByte(8, 0);
    const std::uint32_t owned = action.word(0x10);
    action.setWord(0x10, 0);
    if (owned != 0) {
        const std::uint32_t deletingDestructor = 1;
        callbacks.invoke(CallbackOp::releaseOwned, &deletingDestructor, 1);
    }
}

void setControlPosition(
    LayoutView control, std::uint32_t position, const Callbacks& callbacks) noexcept
{
    callbacks.invoke(CallbackOp::setPosition, &position, 1);
    control.setByte(0x0C, 1);
}

void setControlExtent(
    LayoutView control, std::uint32_t first, std::uint32_t second,
    const Callbacks& callbacks) noexcept
{
    const std::uint32_t arguments[]{first, second};
    callbacks.invoke(CallbackOp::setExtent, arguments, 2);
    control.setByte(0x0C, 1);
}

void setControlClip(
    LayoutView control, std::uint32_t clip, const Callbacks& callbacks) noexcept
{
    callbacks.invoke(CallbackOp::setClip, &clip, 1);
    control.setByte(0x0D, 1);
}

std::uint32_t controllerUiObject(LayoutView controller) noexcept
{
    LayoutView owner(pointerFromWord<void>(controller.word(4)));
    return owner.word(0x21C);
}

std::uint32_t controllerInputTable(LayoutView controller) noexcept
{
    LayoutView owner(pointerFromWord<void>(controller.word(4)));
    return owner.word(4) + 0x2D0u;
}

std::uint32_t countControlFlags(const std::uint32_t words[2]) noexcept
{
    std::uint32_t result = 0;
    for (std::uint32_t value : {words[0], words[1]}) {
        value -= (value >> 1u) & 0x55555555u;
        value = ((value >> 2u) & 0x33333333u) + (value & 0x33333333u);
        result += ((((value >> 4u) + value) & 0x0F0F0F0Fu) * 0x01010101u) >> 24u;
    }
    return result;
}

void dispatchInputRelease(
    InputRouterView router, std::uint32_t slot, const std::uint32_t payload[4],
    const Callbacks& callbacks) noexcept
{
    callbacks.invoke(CallbackOp::preprocessInput, payload, 4);
    if (!router.slotActive(slot)) {
        return;
    }
    router.setSlotActive(slot, false);
    if (router.capturedSlot() == static_cast<std::int32_t>(slot)) {
        router.setCapturedSlot(-1);
        if (router.hasParent()) {
            const Pair pair{payload[0], payload[1]};
            dispatchShortMessage(callbacks, capturedReleaseTags[slot], pair, payload[2]);
        }
        return;
    }
    std::uint32_t suppressArguments[]{slot, payload[0], payload[1], payload[3]};
    if ((callbacks.invoke(CallbackOp::suppressRelease, suppressArguments, 4) & 0xFFu) != 0
        || !router.hasParent()) {
        return;
    }
    const Pair pair{
        router.slotWord(slot, 0),
        router.slotWord(slot, 1)};
    dispatchShortMessage(
        callbacks,
        router.slotAlternate(slot) ? alternateReleaseTags[slot] : normalReleaseTags[slot],
        pair, router.slotWord(slot, 2));
}

std::uint32_t routeIdleInput(
    InputRouterView router, const std::uint32_t payload[4],
    const Callbacks& callbacks) noexcept
{
    callbacks.invoke(CallbackOp::preprocessInput, payload, 4);
    if (router.capturedSlot() >= 0) {
        return 0;
    }
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (router.slotActive(slot)) {
            return 0;
        }
    }
    if (!router.hasParent()) {
        return 1;
    }
    return dispatchMessage(callbacks, tagPointer, payload, 4, PayloadAlias::input);
}

std::uint32_t cancelInputCapture(
    InputRouterView router, std::uint32_t reason, const Callbacks& callbacks) noexcept
{
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        router.setSlotActive(slot, false);
    }
    router.setCapturedSlot(-1);
    if (!router.hasParent()) {
        return 1;
    }
    return dispatchMessage(callbacks, tagCancel, &reason, 1, PayloadAlias::input);
}

void updateHitRegionBounds(HitRegionView region) noexcept
{
    updateBounds(region);
}

void initializeHitRegion(HitRegionView region) noexcept
{
    region.setPoints(nullptr, nullptr, nullptr);
    region.setBoundsValid(true);
    region.setBounds(Rect{});
}

Rect copyHitRegionBounds(HitRegionView region) noexcept
{
    if (!region.boundsValid()) {
        updateHitRegionBounds(region);
    }
    return region.bounds();
}

bool hitRegionContains(HitRegionView region, const Point& point) noexcept
{
    Point* const begin = region.begin();
    Point* const end = region.end();
    if (begin == nullptr || end == nullptr || end - begin < 3) {
        return false;
    }
    const Rect bounds = copyHitRegionBounds(region);
    if (point.x < bounds.left || point.y < bounds.top
        || point.x > bounds.right || point.y > bounds.bottom) {
        return false;
    }
    bool inside = false;
    Point previous = *begin;
    for (const Point* current = begin + 1; current != end; ++current) {
        if (edgeCrossesRay(point, previous, *current)) {
            inside = !inside;
        }
        previous = *current;
    }
    if (edgeCrossesRay(point, previous, *begin)) {
        inside = !inside;
    }
    return inside;
}

void moveTextCaretLeft(TextEditView edit, std::uint32_t keyModifiers) noexcept
{
    std::uint32_t caret = edit.caret();
    if ((keyModifiers & 0x430u) == 0) {
        edit.setSelectionMode(0);
        edit.setAnchor(0);
        edit.setSelection(0, 0);
    }
    if (caret == 0) {
        return;
    }
    if ((edit.flags() & 1u) == 0 && (keyModifiers & 0x0Cu) != 0) {
        --caret;
        const char16_t* const text = edit.text();
        while (caret != 0 && text != nullptr && text[caret] == u' ') {
            --caret;
        }
        while (caret != 0 && (text == nullptr || text[caret] != u' ')) {
            --caret;
        }
        if (caret != 0 && text != nullptr && text[caret] == u' ') {
            ++caret;
        }
    } else {
        --caret;
    }
    if (edit.selectionMode() != 0) {
        const std::uint32_t anchor = edit.anchor();
        edit.setSelection(std::min(caret, anchor), std::max(caret, anchor));
    }
    edit.setCaret(caret);
}

void synchronizeControllerState(
    LayoutView controller, const Callbacks& callbacks) noexcept
{
    callbacks.invoke(CallbackOp::delayedUpdate);
    const std::uint8_t activeByte = static_cast<std::uint8_t>(
        callbacks.invoke(CallbackOp::queryControllerActive) & 0xFFu);
    const bool active = activeByte != 0;
    if (controller.byte(0x14) != activeByte) {
        controller.setByte(0x14, activeByte);
        const bool blocked = active && controller.word(0x0C) != 0
            && (callbacks.invoke(CallbackOp::queryControllerBlocked) & 0xFFu) != 0;
        const std::uint32_t value = active && !blocked ? 1u : 0u;
        callbacks.invoke(CallbackOp::applyControllerActive, &value, 1);
    }
    const bool childReady =
        active && (callbacks.invoke(CallbackOp::queryChildReady) & 0xFFu) != 0;
    const std::uint32_t childValue = childReady ? 1u : 0u;
    callbacks.invoke(CallbackOp::applyControllerReady, &childValue, 1);
}

bool setNamedControlBit(
    std::uint32_t* const words, const std::uint32_t nameToken,
    void* const context, const ControlBitResolver resolve) noexcept
{
    bool success = false;
    const std::uint32_t index =
        resolve == nullptr ? std::numeric_limits<std::uint32_t>::max()
                           : resolve(context, nameToken, success);
    if (!success || static_cast<std::int32_t>(index) < 0) {
        return false;
    }
    words[index >> 5u] |= 1u << (index & 31u);
    return true;
}

void deactivateEventGate(LayoutView gate, const Callbacks& callbacks) noexcept
{
    if (gate.byte(8) == 0) {
        return;
    }
    gate.setByte(8, 0);
    if (gate.word(4) != 0) {
        const std::uint32_t value = 0;
        dispatchMessage(callbacks, tagCancel, &value, 1, PayloadAlias::localCopy);
    }
}

void dispatchActiveInputSlots(
    InputRouterView router, const bool blocked[3], const Pair& pointer,
    const Callbacks& callbacks) noexcept
{
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (router.slotActive(slot) && !blocked[slot]) {
            const std::uint32_t payload[]{
                pointer.first, pointer.second,
                router.slotWord(slot, 2), router.slotWord(slot, 3)};
            dispatchInputRelease(router, slot, payload, callbacks);
        }
    }
}

std::uint32_t routeTypedInputEvent(
    InputRouterView router, const TypedEvent& event, const Callbacks& callbacks) noexcept
{
    const std::uint32_t payloadValue = pointerWord(event.payload);
    if (event.tag == tagCancel && payloadValue != 0) {
        return cancelInputCapture(router, payloadValue, callbacks);
    }
    if (event.tag == tagPointer && event.payload != nullptr) {
        return routeIdleInput(
            router, static_cast<const std::uint32_t*>(event.payload), callbacks);
    }
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        if (event.tag == pressTags[slot] && event.payload != nullptr) {
            const auto* payload = static_cast<const std::uint32_t*>(event.payload);
            if (router.capturedSlot() == static_cast<std::int32_t>(slot)) {
                router.setCapturedSlot(-1);
            }
            for (std::uint32_t word = 0; word < 4; ++word) {
                router.setSlotWord(slot, word, payload[word]);
            }
            router.setSlotActive(slot, true);
            router.setSlotAlternate(slot, false);
            return 0;
        }
        if (event.tag == releaseTags[slot] && event.payload != nullptr) {
            dispatchInputRelease(
                router, slot, static_cast<const std::uint32_t*>(event.payload), callbacks);
            return 0;
        }
        if (event.tag == holdTags[slot] && event.payload != nullptr) {
            const auto* payload = static_cast<const std::uint32_t*>(event.payload);
            const std::uint32_t arguments[]{
                slot, payload[0], payload[1], payload[2], payload[3]};
            callbacks.invoke(CallbackOp::dispatchHeld, arguments, 5);
            return 0;
        }
    }
    if (!router.hasParent()) {
        return 1;
    }
    const std::uint32_t payload[]{payloadValue};
    return dispatchMessage(callbacks, event.tag, payload, 1, PayloadAlias::input);
}

void setPrimaryControlBit(
    LayoutView control, std::uint32_t index, bool value,
    const Callbacks& callbacks) noexcept
{
    setControlBit(control, 0x10C, CallbackOp::notifyPrimaryBit, index, value, callbacks);
}

void setSecondaryControlBit(
    LayoutView control, std::uint32_t index, bool value,
    const Callbacks& callbacks) noexcept
{
    setControlBit(control, 0x114, CallbackOp::notifySecondaryBit, index, value, callbacks);
}

TextSlice makeTextSlice(
    const void* sourceHandle, std::int32_t startValue, std::int32_t countValue) noexcept
{
    const std::uint32_t length = textLength(sourceHandle);
    const std::uint32_t rawStart = static_cast<std::uint32_t>(startValue);
    const std::uint32_t rawEnd = rawStart + static_cast<std::uint32_t>(countValue);
    const std::uint32_t start = clampSignedIndex(rawStart, length);
    if (static_cast<std::int32_t>(rawEnd) < static_cast<std::int32_t>(start)) {
        return TextSlice{sourceHandle, start, 0};
    }
    const std::uint32_t end = std::min(rawEnd, length);
    return TextSlice{sourceHandle, start, end - start};
}

TextSlice makeTextPrefix(const void* sourceHandle, std::int32_t count) noexcept
{
    return makeTextSlice(sourceHandle, 0, count);
}

TextSlice makeTextSuffix(const void* sourceHandle, std::int32_t count) noexcept
{
    const std::uint32_t length = textLength(sourceHandle);
    const std::uint32_t start =
        length - static_cast<std::uint32_t>(count);
    return makeTextSlice(sourceHandle, static_cast<std::int32_t>(start), count);
}

std::uint32_t dispatchUiMessage107(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A107u, pair, value);
}

std::uint32_t dispatchUiMessage108(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A108u, pair, value);
}

std::uint32_t dispatchUiMessage109(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept
{
    return dispatchLongMessage(callbacks, 0x00E2A109u, pair, value, second);
}

std::uint32_t dispatchUiMessage10A(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A10Au, pair, value);
}

std::uint32_t dispatchUiMessage10B(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A10Bu, pair, value);
}

std::uint32_t dispatchUiMessage10C(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A10Cu, pair, value);
}

std::uint32_t dispatchUiMessage10D(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A10Du, pair, value);
}

std::uint32_t dispatchUiMessage10E(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept
{
    return dispatchLongMessage(callbacks, 0x00E2A10Eu, pair, value, second);
}

std::uint32_t dispatchUiMessage10F(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A10Fu, pair, value);
}

std::uint32_t dispatchUiMessage110(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A110u, pair, value);
}

std::uint32_t dispatchUiMessage111(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A111u, pair, value);
}

std::uint32_t dispatchUiMessage112(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A112u, pair, value);
}

std::uint32_t dispatchUiMessage113(
    const Pair& pair, std::uint32_t value, const Pair& second,
    const Callbacks& callbacks) noexcept
{
    return dispatchLongMessage(callbacks, 0x00E2A113u, pair, value, second);
}

std::uint32_t dispatchUiMessage114(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A114u, pair, value);
}

std::uint32_t dispatchUiMessage115(
    const Pair& pair, std::uint32_t value, const Callbacks& callbacks) noexcept
{
    return dispatchShortMessage(callbacks, 0x00E2A115u, pair, value);
}

std::uint32_t routeNearPointerEvent(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept
{
    if (!deltaLess(event.x, static_cast<std::int32_t>(router.word(0x0C)), 5)
        || !deltaLess(event.y, static_cast<std::int32_t>(router.word(0x10)), 5)) {
        const std::uint32_t arguments[]{
            tagPointer, static_cast<std::uint32_t>(event.x),
            static_cast<std::uint32_t>(event.y), event.value, event.timestamp};
        return callbacks.invoke(CallbackOp::fallbackPointer, arguments, 5);
    }
    if (!nestedSinkPresent(router)) {
        return 1;
    }
    const std::uint32_t payload[]{
        static_cast<std::uint32_t>(event.x), static_cast<std::uint32_t>(event.y),
        event.value, event.timestamp};
    return dispatchMessage(callbacks, tagPointer, payload, 4, PayloadAlias::input);
}

void activateEventGate(LayoutView gate, const Callbacks& callbacks) noexcept
{
    if (gate.byte(8) != 0) {
        return;
    }
    gate.setByte(8, 1);
    if (gate.word(4) != 0) {
        const std::uint32_t value = 0;
        dispatchMessage(callbacks, tagActivate, &value, 1, PayloadAlias::localCopy);
    }
}

void beginTextSelection(
    TextEditView edit, std::int32_t x, std::int32_t y,
    const Callbacks& callbacks) noexcept
{
    edit.setSelectionMode(0);
    edit.setAnchor(0);
    edit.setSelection(0, 0);
    const std::uint32_t arguments[]{
        static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
    const std::uint32_t caret =
        callbacks.invoke(CallbackOp::hitTestCaret, arguments, 2);
    if (caret != std::numeric_limits<std::uint32_t>::max()) {
        edit.setCaret(caret);
        edit.setSelectionMode(2);
        edit.setAnchor(caret);
        edit.setSelection(caret, caret);
    }
    edit.setMouseSelectionActive(true);
}

void updateTextSelectionFromPoint(
    TextEditView edit, Point& lastPoint, std::int32_t x, std::int32_t y,
    const Callbacks& callbacks) noexcept
{
    if (lastPoint.x == x && lastPoint.y == y) {
        return;
    }
    if (edit.mouseSelectionActive()) {
        const std::uint32_t arguments[]{
            static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
        const std::uint32_t caret =
            callbacks.invoke(CallbackOp::hitTestCaret, arguments, 2);
        if (caret != std::numeric_limits<std::uint32_t>::max()) {
            applyHitCaret(edit, caret);
        }
    }
    lastPoint = Point{x, y};
}

std::uint32_t forwardUiMessage106(
    const std::uint32_t payload[3], bool hasSink, const Callbacks& callbacks) noexcept
{
    if (hasSink) {
        dispatchMessage(callbacks, tagForward106, payload, 3, PayloadAlias::localCopy);
    }
    return 0;
}

std::uint32_t routePrimaryDoubleClick(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept
{
    return routeDoubleClick(router, event, false, 0x00DEBD46u, callbacks);
}

std::uint32_t routeSecondaryDoubleClick(
    LayoutView router, const PointerEvent& event, const Callbacks& callbacks) noexcept
{
    return routeDoubleClick(router, event, true, 0x00DEBD4Eu, callbacks);
}

}  // namespace ra3::ui
