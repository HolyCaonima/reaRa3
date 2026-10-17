#include "support/TestHarness.h"

#include <ra3/ui/UiControl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace {

std::uint32_t pointerWord(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

struct MessageCapture {
    std::array<std::uint32_t, 8> arguments{};
    std::size_t count{};

    static std::uint32_t callback(
        void* context, ra3::ui::CallbackOp operation,
        const std::uint32_t* arguments, std::size_t argumentCount)
    {
        auto& capture = *static_cast<MessageCapture*>(context);
        if (operation == ra3::ui::CallbackOp::dispatchMessage) {
            capture.count = argumentCount;
            std::copy(arguments, arguments + argumentCount, capture.arguments.begin());
        }
        return 0x12345678u;
    }
};

}  // namespace

RA3_TEST(uiHitRegionMaintainsBoundsAndUsesInclusiveEdges)
{
    std::array<std::uint8_t, 0x24> storage{};
    ra3::ui::HitRegionView region(storage.data());
    ra3::ui::initializeHitRegion(region);
    const ra3::ui::Point points[]{{0, 0}, {20, 0}, {20, 10}, {0, 10}};
    region.setPoints(
        const_cast<ra3::ui::Point*>(points),
        const_cast<ra3::ui::Point*>(points) + 4,
        const_cast<ra3::ui::Point*>(points) + 4);
    region.setBoundsValid(false);

    const auto bounds = ra3::ui::copyHitRegionBounds(region);
    RA3_CHECK_EQ(bounds.left, 0);
    RA3_CHECK_EQ(bounds.right, 20);
    RA3_CHECK(ra3::ui::hitRegionContains(region, ra3::ui::Point{10, 5}));
    RA3_CHECK(!ra3::ui::hitRegionContains(region, ra3::ui::Point{30, 5}));
}

RA3_TEST(uiTextCaretAndSlicesPreserveGameLayoutSemantics)
{
    std::array<std::uint8_t, 0x38> editStorage{};
    const char16_t text[] = u"one  two";
    std::uint32_t textPointer = pointerWord(text);
    std::memcpy(editStorage.data() + 8, &textPointer, sizeof(textPointer));
    const std::uint32_t caret = 8;
    std::memcpy(editStorage.data() + 0x10, &caret, sizeof(caret));
    ra3::ui::moveTextCaretLeft(ra3::ui::TextEditView(editStorage.data()), 4);
    std::uint32_t result = 0;
    std::memcpy(&result, editStorage.data() + 0x10, sizeof(result));
    RA3_CHECK_EQ(result, 5u);

    struct TextStorage {
        std::uint16_t length;
        std::uint16_t capacity;
        char16_t value[8];
    } source{6, 8, u"abcdef"};
    std::uint32_t handle = pointerWord(source.value);
    const auto slice = ra3::ui::makeTextSlice(&handle, -3, 8);
    RA3_CHECK_EQ(slice.sourceHandle, &handle);
    RA3_CHECK_EQ(slice.start, 0u);
    RA3_CHECK_EQ(slice.count, 5u);
}

RA3_TEST(uiTypedMessageEncoderCopiesPayloadAndMarksAlias)
{
    MessageCapture capture;
    const ra3::ui::Callbacks callbacks{&capture, &MessageCapture::callback};
    const ra3::ui::Pair first{1, 2};
    const ra3::ui::Pair second{4, 5};
    RA3_CHECK_EQ(
        ra3::ui::dispatchUiMessage109(first, 3, second, callbacks),
        0x12345678u);
    RA3_CHECK_EQ(capture.count, 8u);
    RA3_CHECK_EQ(capture.arguments[0], 0x00E2A109u);
    RA3_CHECK_EQ(capture.arguments[1], 5u);
    RA3_CHECK_EQ(
        capture.arguments[2],
        static_cast<std::uint32_t>(ra3::ui::PayloadAlias::localCopy));
    RA3_CHECK_EQ(capture.arguments[7], 5u);
}
