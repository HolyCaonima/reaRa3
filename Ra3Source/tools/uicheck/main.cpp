#include <ra3/ui/UiControl.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ra3::ui::CallbackOp;

struct Recorder {
    static constexpr std::size_t resultCount = 32;

    std::array<std::uint32_t, resultCount> results{};
    std::vector<std::uint32_t> trace;

    static std::uint32_t invoke(
        void* context, CallbackOp operation, const std::uint32_t* arguments,
        std::size_t argumentCount)
    {
        auto& self = *static_cast<Recorder*>(context);
        self.trace.push_back(static_cast<std::uint32_t>(operation));
        self.trace.push_back(static_cast<std::uint32_t>(argumentCount));
        if (argumentCount != 0) {
            self.trace.insert(self.trace.end(), arguments, arguments + argumentCount);
        }
        return self.results[static_cast<std::size_t>(operation)];
    }

    static bool measure(
        void* context, std::uint32_t candidate, std::uint32_t& measured) noexcept
    {
        auto& self = *static_cast<Recorder*>(context);
        measured = self.results.back();
        return (invoke(
                    &self, CallbackOp::measureTextIndex, &candidate, 1)
                & 0xFFu)
            != 0;
    }

    static std::uint32_t resolveBit(
        void* context, std::uint32_t nameToken, bool& success) noexcept
    {
        auto& self = *static_cast<Recorder*>(context);
        success = (self.results[self.results.size() - 2] & 1u) != 0;
        return invoke(
            &self, CallbackOp::resolveBitName, &nameToken, 1);
    }

    ra3::ui::Callbacks callbacks()
    {
        return ra3::ui::Callbacks{this, &Recorder::invoke};
    }
};

std::vector<std::uint8_t> decode(const std::string& text)
{
    std::vector<std::uint8_t> result;
    if (text == ".") {
        return result;
    }
    if ((text.size() & 1u) != 0) {
        throw std::runtime_error("odd hex input");
    }
    result.reserve(text.size() / 2u);
    for (std::size_t i = 0; i < text.size(); i += 2u) {
        result.push_back(
            static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return result;
}

void encode(const void* memory, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    if (size == 0) {
        std::cout << '.';
        return;
    }
    std::cout << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    std::cout << std::dec;
}

template <typename T>
T read(const std::vector<std::uint8_t>& bytes, std::size_t offset = 0)
{
    T value{};
    if (offset + sizeof(value) > bytes.size()) {
        throw std::runtime_error("short input");
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

template <typename T>
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, T value)
{
    if (offset + sizeof(value) > bytes.size()) {
        throw std::runtime_error("short output");
    }
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint32_t pointerWord(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t argument(std::istringstream& input)
{
    std::string token;
    input >> token;
    return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
}

void configureResults(Recorder& recorder, const std::vector<std::uint8_t>& bytes)
{
    const std::size_t count =
        std::min(bytes.size() / sizeof(std::uint32_t), recorder.results.size());
    std::memcpy(recorder.results.data(), bytes.data(), count * sizeof(std::uint32_t));
}

std::uint32_t dispatchMessageOperation(
    std::uint32_t address, const std::vector<std::uint8_t>& blob,
    const std::vector<std::uint8_t>& auxiliary, std::uint32_t value,
    const ra3::ui::Callbacks& callbacks)
{
    const auto pair = read<ra3::ui::Pair>(blob);
    switch (address) {
    case 0x00A740A0: return ra3::ui::dispatchUiMessage107(pair, value, callbacks);
    case 0x00A740E0: return ra3::ui::dispatchUiMessage108(pair, value, callbacks);
    case 0x00A74120:
        return ra3::ui::dispatchUiMessage109(
            pair, value, read<ra3::ui::Pair>(auxiliary), callbacks);
    case 0x00A74170: return ra3::ui::dispatchUiMessage10A(pair, value, callbacks);
    case 0x00A741B0: return ra3::ui::dispatchUiMessage10B(pair, value, callbacks);
    case 0x00A741F0: return ra3::ui::dispatchUiMessage10C(pair, value, callbacks);
    case 0x00A74230: return ra3::ui::dispatchUiMessage10D(pair, value, callbacks);
    case 0x00A74270:
        return ra3::ui::dispatchUiMessage10E(
            pair, value, read<ra3::ui::Pair>(auxiliary), callbacks);
    case 0x00A742C0: return ra3::ui::dispatchUiMessage10F(pair, value, callbacks);
    case 0x00A74300: return ra3::ui::dispatchUiMessage110(pair, value, callbacks);
    case 0x00A74340: return ra3::ui::dispatchUiMessage111(pair, value, callbacks);
    case 0x00A74380: return ra3::ui::dispatchUiMessage112(pair, value, callbacks);
    case 0x00A743C0:
        return ra3::ui::dispatchUiMessage113(
            pair, value, read<ra3::ui::Pair>(auxiliary), callbacks);
    case 0x00A74410: return ra3::ui::dispatchUiMessage114(pair, value, callbacks);
    case 0x00A74450: return ra3::ui::dispatchUiMessage115(pair, value, callbacks);
    default: throw std::runtime_error("not a message operation");
    }
}

void run(const std::string& line)
{
    std::istringstream input(line);
    std::string addressText;
    std::string blobText;
    std::string auxiliaryText;
    std::string resultsText;
    input >> addressText >> blobText >> auxiliaryText >> resultsText;
    if (resultsText.empty()) {
        throw std::runtime_error("short command");
    }
    const auto address = static_cast<std::uint32_t>(
        std::stoul(addressText, nullptr, 16));
    auto blob = decode(blobText);
    auto auxiliary = decode(auxiliaryText);
    Recorder recorder;
    configureResults(recorder, decode(resultsText));
    const auto callbacks = recorder.callbacks();
    std::uint32_t result = 0;
    std::vector<std::uint8_t> output;

    switch (address) {
    case 0x00A6FFE0:
        {
            auto* const points =
                reinterpret_cast<ra3::ui::Point*>(auxiliary.data());
            const std::size_t pointCount =
                auxiliary.size() / sizeof(ra3::ui::Point);
            auto region = ra3::ui::HitRegionView(blob.data());
            region.setPoints(points, points + pointCount, points + pointCount);
            ra3::ui::updateHitRegionBounds(region);
            write(blob, 0, auxiliary.empty() ? 0u : 1u);
            write(blob, 4, auxiliary.empty()
                ? 0u
                : 1u
                    + static_cast<std::uint32_t>(
                        pointCount * sizeof(ra3::ui::Point)));
            write(blob, 8, auxiliary.empty()
                ? 0u
                : 1u
                    + static_cast<std::uint32_t>(
                        pointCount * sizeof(ra3::ui::Point)));
        }
        break;
    case 0x00A70060:
        result = ra3::ui::setControlActive(
            ra3::ui::LayoutView(blob.data()), argument(input) != 0, callbacks);
        break;
    case 0x00A700D0:
        result = ra3::ui::propagateEnabledState(
            ra3::ui::LayoutView(blob.data()), callbacks);
        break;
    case 0x00A70110:
        result = ra3::ui::propagateFocusState(
            ra3::ui::LayoutView(blob.data()), callbacks);
        break;
    case 0x00A701A0:
        result = ra3::ui::fitTextIndex(
            argument(input), &recorder, &Recorder::measure);
        break;
    case 0x00A70220:
        ra3::ui::tickUiTimer(
            ra3::ui::LayoutView(blob.data()), argument(input), callbacks);
        break;
    case 0x00A70290:
        ra3::ui::startUiTimer(
            ra3::ui::LayoutView(blob.data()), argument(input), callbacks);
        break;
    case 0x00A704B0:
        write(blob, 0x1C, pointerWord(auxiliary.data()));
        ra3::ui::clearPendingAction(ra3::ui::LayoutView(blob.data()), callbacks);
        write(blob, 0x1C, auxiliary.empty() ? 0u : 1u);
        break;
    case 0x00A70500:
        ra3::ui::setControlPosition(
            ra3::ui::LayoutView(blob.data()), argument(input), callbacks);
        break;
    case 0x00A70520:
        {
            const std::uint32_t first = argument(input);
            ra3::ui::setControlExtent(
                ra3::ui::LayoutView(blob.data()), first, argument(input), callbacks);
        }
        break;
    case 0x00A70540:
        ra3::ui::setControlClip(
            ra3::ui::LayoutView(blob.data()), argument(input), callbacks);
        break;
    case 0x00A705A0:
        write(blob, 4, pointerWord(auxiliary.data()));
        result = ra3::ui::controllerUiObject(ra3::ui::LayoutView(blob.data()));
        write(blob, 4, auxiliary.empty() ? 0u : 1u);
        break;
    case 0x00A70670:
        write(blob, 4, pointerWord(auxiliary.data()));
        result = ra3::ui::controllerInputTable(ra3::ui::LayoutView(blob.data()));
        write(blob, 4, auxiliary.empty() ? 0u : 1u);
        break;
    case 0x00A70700:
        result = ra3::ui::countControlFlags(
            reinterpret_cast<const std::uint32_t*>(blob.data()));
        break;
    case 0x00A71200:
        {
            const std::uint32_t slot = argument(input);
            ra3::ui::dispatchInputRelease(
                ra3::ui::InputRouterView(blob.data()), slot,
                reinterpret_cast<const std::uint32_t*>(auxiliary.data()), callbacks);
        }
        break;
    case 0x00A712C0:
        result = ra3::ui::routeIdleInput(
            ra3::ui::InputRouterView(blob.data()),
            reinterpret_cast<const std::uint32_t*>(auxiliary.data()), callbacks);
        break;
    case 0x00A71340:
        result = ra3::ui::cancelInputCapture(
            ra3::ui::InputRouterView(blob.data()), argument(input), callbacks);
        break;
    case 0x00A71B20:
        ra3::ui::initializeHitRegion(ra3::ui::HitRegionView(blob.data()));
        result = 1;
        break;
    case 0x00A71B40:
    case 0x00A71B70:
        {
            auto* points = reinterpret_cast<ra3::ui::Point*>(auxiliary.data());
            const std::size_t pointCount = auxiliary.size() / sizeof(ra3::ui::Point);
            auto region = ra3::ui::HitRegionView(blob.data());
            region.setPoints(points, points + pointCount, points + pointCount);
            if (address == 0x00A71B40) {
                const auto bounds = ra3::ui::copyHitRegionBounds(region);
                output.resize(sizeof(bounds));
                std::memcpy(output.data(), &bounds, sizeof(bounds));
                result = 1;
            } else {
                const auto x = static_cast<std::int32_t>(argument(input));
                const auto y = static_cast<std::int32_t>(argument(input));
                const ra3::ui::Point point{
                    x, y};
                result = ra3::ui::hitRegionContains(region, point) ? 1u : 0u;
            }
            write(blob, 0, auxiliary.empty() ? 0u : 1u);
            write(blob, 4, auxiliary.empty()
                ? 0u : 1u + static_cast<std::uint32_t>(pointCount * sizeof(ra3::ui::Point)));
            write(blob, 8, auxiliary.empty()
                ? 0u : 1u + static_cast<std::uint32_t>(pointCount * sizeof(ra3::ui::Point)));
        }
        break;
    case 0x00A71E80:
        {
            auto edit = ra3::ui::TextEditView(blob.data());
            edit.setCaret(read<std::uint32_t>(blob, 0x10));
            write(blob, 8, pointerWord(auxiliary.data()));
            ra3::ui::moveTextCaretLeft(edit, argument(input));
            write(blob, 8, auxiliary.empty() ? 0u : 1u);
        }
        break;
    case 0x00A72380:
        ra3::ui::synchronizeControllerState(
            ra3::ui::LayoutView(blob.data()), callbacks);
        break;
    case 0x00A72660:
        result = ra3::ui::setNamedControlBit(
            reinterpret_cast<std::uint32_t*>(blob.data()), argument(input),
            &recorder, &Recorder::resolveBit)
            ? 1u : 0u;
        break;
    case 0x00A72D30:
        ra3::ui::deactivateEventGate(ra3::ui::LayoutView(blob.data()), callbacks);
        break;
    case 0x00A72DC0:
        {
            const std::uint32_t mask = argument(input);
            const bool blocked[]{
                (mask & 1u) != 0, (mask & 2u) != 0, (mask & 4u) != 0};
            ra3::ui::dispatchActiveInputSlots(
                ra3::ui::InputRouterView(blob.data()), blocked,
                read<ra3::ui::Pair>(auxiliary), callbacks);
        }
        break;
    case 0x00A72E20:
        {
            const std::uint32_t tag = argument(input);
            const void* payload = auxiliary.empty() ? nullptr : auxiliary.data();
            if (auxiliary.empty()) {
                payload = reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(argument(input)));
            }
            const ra3::ui::TypedEvent event{tag, payload};
            result = ra3::ui::routeTypedInputEvent(
                ra3::ui::InputRouterView(blob.data()), event, callbacks);
        }
        break;
    case 0x00A730B0:
        {
            const std::uint32_t index = argument(input);
            ra3::ui::setPrimaryControlBit(
                ra3::ui::LayoutView(blob.data()), index, argument(input) != 0, callbacks);
        }
        break;
    case 0x00A73130:
        {
            const std::uint32_t index = argument(input);
            ra3::ui::setSecondaryControlBit(
                ra3::ui::LayoutView(blob.data()), index, argument(input) != 0, callbacks);
        }
        break;
    case 0x00A73590:
    case 0x00A735F0:
    case 0x00A73640:
        {
            std::uint32_t handle = pointerWord(auxiliary.empty() ? nullptr : auxiliary.data() + 4);
            ra3::ui::TextSlice slice{};
            if (address == 0x00A73590) {
                const auto start = static_cast<std::int32_t>(argument(input));
                slice = ra3::ui::makeTextSlice(
                    &handle, start, static_cast<std::int32_t>(argument(input)));
            } else if (address == 0x00A735F0) {
                slice = ra3::ui::makeTextPrefix(
                    &handle, static_cast<std::int32_t>(argument(input)));
            } else {
                slice = ra3::ui::makeTextSuffix(
                    &handle, static_cast<std::int32_t>(argument(input)));
            }
            const std::uint32_t normalized[]{
                slice.sourceHandle == &handle ? 1u : 0u, slice.start, slice.count};
            output.resize(sizeof(normalized));
            std::memcpy(output.data(), normalized, sizeof(normalized));
            result = 1;
        }
        break;
    case 0x00A740A0: case 0x00A740E0: case 0x00A74120:
    case 0x00A74170: case 0x00A741B0: case 0x00A741F0:
    case 0x00A74230: case 0x00A74270: case 0x00A742C0:
    case 0x00A74300: case 0x00A74340: case 0x00A74380:
    case 0x00A743C0: case 0x00A74410: case 0x00A74450:
        result = dispatchMessageOperation(
            address, blob, auxiliary, argument(input), callbacks);
        break;
    case 0x00A74700:
        {
            auxiliary.resize(std::max<std::size_t>(auxiliary.size(), 8u));
            write(blob, 4, pointerWord(auxiliary.data()));
            const auto event = read<ra3::ui::PointerEvent>(blob, 0x20);
            result = ra3::ui::routeNearPointerEvent(
                ra3::ui::LayoutView(blob.data()), event, callbacks);
            write(blob, 4, 1u);
        }
        break;
    case 0x00A748F0:
        ra3::ui::activateEventGate(ra3::ui::LayoutView(blob.data()), callbacks);
        break;
    case 0x00A74E70:
        {
            const auto x = static_cast<std::int32_t>(argument(input));
            const auto y = static_cast<std::int32_t>(argument(input));
            ra3::ui::beginTextSelection(
                ra3::ui::TextEditView(blob.data()), x, y, callbacks);
        }
        break;
    case 0x00A74EC0:
        {
            auto last = read<ra3::ui::Point>(auxiliary);
            const auto x = static_cast<std::int32_t>(argument(input));
            const auto y = static_cast<std::int32_t>(argument(input));
            ra3::ui::updateTextSelectionFromPoint(
                ra3::ui::TextEditView(blob.data()), last, x, y, callbacks);
            std::memcpy(auxiliary.data(), &last, sizeof(last));
        }
        break;
    case 0x00A75570:
        result = ra3::ui::forwardUiMessage106(
            reinterpret_cast<const std::uint32_t*>(blob.data()),
            argument(input) != 0, callbacks);
        break;
    case 0x00A759F0:
        result = ra3::ui::routePrimaryDoubleClick(
            ra3::ui::LayoutView(blob.data()),
            read<ra3::ui::PointerEvent>(auxiliary), callbacks);
        break;
    case 0x00A75A60:
        result = ra3::ui::routeSecondaryDoubleClick(
            ra3::ui::LayoutView(blob.data()),
            read<ra3::ui::PointerEvent>(auxiliary), callbacks);
        break;
    default:
        throw std::runtime_error("unknown address");
    }

    std::cout << std::hex << result << std::dec << ' ';
    encode(blob.data(), blob.size());
    std::cout << ' ';
    encode(auxiliary.data(), auxiliary.size());
    std::cout << ' ';
    encode(output.data(), output.size());
    std::cout << ' ';
    encode(recorder.trace.data(), recorder.trace.size() * sizeof(std::uint32_t));
    std::cout << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }
        try {
            run(line);
        } catch (const std::exception&) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
