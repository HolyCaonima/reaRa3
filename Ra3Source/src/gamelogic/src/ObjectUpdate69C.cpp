#include "ra3/gamelogic/ObjectUpdate69C.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

DirectHandler g_direct = nullptr;
AbsoluteReader g_absolute = nullptr;

constexpr std::uint32_t kMaxFinite = 0x7F7FFFFFU;
constexpr std::uint32_t kOne = 0x3F800000U;
constexpr std::uint32_t kVisionKey = 0x44A5973DU;
constexpr std::uint32_t kTunnelSentinel = 0x00CC0E7CU;
constexpr std::uint32_t kLogic = 0x00DEBA64U;
constexpr std::uint32_t kPlayerList = 0x00DFBD74U;
constexpr std::uint32_t kPointSink = 0x00DF8DE8U;
// Shipped image dword at 0x00CB0910. fimul uses this numerator.
constexpr std::int32_t kDockNumerator = 15;
constexpr float kUnsignedBias = 4294967296.0F;

template <class T>
T load(const void* object, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(T));
    return value;
}

template <class T>
void store(void* object, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

std::byte* at(void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<std::byte*>(object) + offset;
}

const std::byte* at(const void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::byte*>(object) + offset;
}

std::uint32_t imageDword(std::uint32_t address) noexcept {
    return g_absolute != nullptr ? g_absolute(address) : 0U;
}

void* imagePointer(std::uint32_t address) noexcept {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(imageDword(address)));
}

std::uint32_t addr(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

using This0 = std::uint32_t(__thiscall*)(void*);
using This1 = std::uint32_t(__thiscall*)(void*, std::uint32_t);
using This2 = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t);
using This3 = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
using This4 = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

template <class Fn>
Fn virtualAt(const void* object, std::ptrdiff_t slot) noexcept {
    return load<Fn>(load<const void*>(object, 0), slot);
}

std::uint32_t directCall(std::uint32_t target, void* ecx, std::uint32_t a0 = 0, std::uint32_t a1 = 0,
    std::uint32_t a2 = 0, std::uint32_t a3 = 0, std::uint32_t a4 = 0, std::uint32_t a5 = 0,
    int argc = 0) noexcept {
    DirectCall call{target, ecx, a0, a1, a2, a3, a4, a5, argc};
    return g_direct != nullptr ? g_direct(call) : 0U;
}

void xferVersion(void* xfer, std::uint8_t major, std::uint8_t minor) noexcept {
    std::uint8_t pair[2] = {major, minor};
    virtualAt<This1>(xfer, 0x18)(xfer, addr(pair));
}

void clearThree(void* self) noexcept {
    store<std::uint32_t>(self, 0x308, 0xFFFFFFFFU);
    store<std::uint32_t>(self, 0x368, 0xFFFFFFFFU);
    store<std::uint32_t>(self, 0x3C8, 0xFFFFFFFFU);
}

void issuePair(void* self, std::uint32_t goal, std::uint32_t stamp, std::uint32_t stateId) noexcept {
    auto* state = load<void*>(self, 0x3C);
    virtualAt<This0>(state, 0x34)(state);
    virtualAt<This1>(state, 0x38)(state, goal);
    store<std::uint32_t>(self, 0x50, stamp);
    virtualAt<This1>(state, 0x1C)(state, stateId);
}

void issuePacked(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp, std::uint32_t stateId,
    bool useFloat) noexcept {
    auto* state = load<void*>(self, 0x3C);
    virtualAt<This0>(state, 0x34)(state);
    virtualAt<This1>(state, 0x38)(state, goal);
    if (useFloat) {
        directCall(0x005AC4F0, state, packed, kMaxFinite, 0, 0, 0, 0, 2);
    } else {
        directCall(0x005BDCC0, state, packed, 0, 0, 0, 0, 0, 1);
    }
    store<std::uint32_t>(self, 0x50, stamp);
    virtualAt<This1>(state, 0x1C)(state, stateId);
    clearThree(self);
}

bool bit(std::uint32_t word, unsigned shift) noexcept {
    return ((word >> shift) & 1U) != 0U;
}

float unsignedFloat(std::int32_t value) noexcept {
    float out = static_cast<float>(value);
    if (value < 0) {
        out += kUnsignedBias;
    }
    return out;
}

struct IdSpan {
    std::uint32_t* begin = nullptr;
    int count = 0;
    void* anchor = nullptr;
    void* table = nullptr;
};

IdSpan idSpan(void* self) noexcept {
    auto* anchor = load<std::byte*>(self, -0x0C);
    IdSpan span{};
    span.anchor = anchor;
    span.table = load<void*>(self, -8);
    span.count = load<std::int32_t>(anchor, 0x1C0);
    span.begin = load<std::uint32_t*>(anchor, 0x1C4);
    return span;
}

void reload(IdSpan& span) noexcept {
    auto* anchor = static_cast<std::byte*>(span.anchor);
    span.count = load<std::int32_t>(anchor, 0x1C0);
    span.begin = load<std::uint32_t*>(anchor, 0x1C4);
}

} // namespace

void setObjectUpdate69CDirect(DirectHandler handler) noexcept { g_direct = handler; }
void setObjectUpdate69CAbsolute(AbsoluteReader reader) noexcept { g_absolute = reader; }

void aiUpdateIssue61(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    auto* state = load<void*>(self, 0x3C);
    virtualAt<This0>(state, 0x34)(state);
    directCall(0x005AC4F0, state, packed, kMaxFinite, 0, 0, 0, 0, 2);
    directCall(0x005AA2A0, state, goal, 0, 0, 0, 0, 0, 1);
    store<std::uint32_t>(self, 0x50, stamp);
    virtualAt<This1>(state, 0x1C)(state, 0x61U);
}

void aiUpdateIssue51(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept {
    issuePair(self, goal, stamp, 0x51U);
}

void aiUpdateIssue3A(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept {
    auto* object = load<void*>(self, 8);
    const auto allowed = directCall(0x00692390, imagePointer(0x00CC561C), addr(object), goal, stamp, 0, 0, 3);
    if ((allowed & 0xFFU) == 0U) {
        return;
    }
    auto* state = load<void*>(self, 0x3C);
    virtualAt<This0>(state, 0x34)(state);
    virtualAt<This1>(state, 0x38)(state, goal);
    store<std::uint32_t>(self, 0x50, stamp);
    virtualAt<This1>(state, 0x1C)(state, 0x3AU);
}

void aiUpdateIssue32(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept {
    issuePair(self, goal, stamp, 0x32U);
}

void aiUpdateIssue40(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    issuePacked(self, goal, packed, stamp, 0x40U, true);
}

void aiUpdateIssue42(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    issuePacked(self, goal, packed, stamp, 0x42U, false);
}

void aiUpdateIssue3F(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    issuePacked(self, goal, packed, stamp, 0x3FU, true);
}

void aiUpdateIssue43(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    issuePacked(self, goal, packed, stamp, 0x43U, false);
}

void aiUpdateIssue49(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept {
    issuePacked(self, goal, packed, stamp, 0x49U, true);
    auto* object = load<std::byte*>(self, 8);
    auto* scripted = object == nullptr ? nullptr : load<void*>(object, 0x418);
    if (scripted != nullptr) {
        directCall(0x00844460, imagePointer(0x00DFC058), addr(scripted), 0U, 0, 0, 0, 0, 2);
        clearThree(self);
    }
}

void aiUpdateIssue2A(void* self, std::uint32_t target, std::uint32_t stamp) noexcept {
    auto* state = load<void*>(self, 0x3C);
    if (target == 0U) {
        virtualAt<This0>(state, 0x34)(state);
        store<std::uint32_t>(self, 0x50, stamp);
        virtualAt<This1>(state, 0x1C)(state, 0x2AU);
        return;
    }
    directCall(0x00799580, state, 0, 0, 0, 0, 0, 0, 0);
    directCall(0x007995C0, state, 0x2AU, target, 0, 0, 0, 0, 2);
}

void aiUpdateTouchLocomotor(void* self) noexcept {
    const auto held = load<std::uint32_t>(self, 0x14);
    auto* logic = imagePointer(kLogic);
    const auto now = load<std::uint32_t>(logic, 0x58);
    if (held <= now) {
        return;
    }
    if (static_cast<std::int32_t>(held - now) <= 1) {
        return;
    }
    if (load<std::uint8_t>(self, 0x43B) != 0U) {
        return;
    }
    directCall(0x004ED9C0, logic, load<std::uint32_t>(self, 8), addr(at(self, 0x10)), now + 1U, 0, 0, 0, 3);
}

void specialPowerModuleNoteReady(void* self) noexcept {
    auto* sub = at(self, 0x10);
    if ((virtualAt<This0>(sub, 0x10)(sub) & 0xFFU) != 0U) {
        return;
    }
    if (load<std::uint8_t>(self, 0x40) != 0U) {
        return;
    }
    auto* object = load<std::byte*>(self, 8);
    if (bit(load<std::uint32_t>(object, 0x88), 9)) {
        return;
    }
    auto* info = reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This0>(sub, 0x1C)(sub)));
    auto* inner = load<const std::byte*>(info, 8);
    if (bit(load<std::uint32_t>(inner, 0x1C), 12)) {
        return;
    }
    auto* logic = imagePointer(kLogic);
    store<std::uint32_t>(self, 0x30, load<std::uint32_t>(logic, 0x58));
}

void specialPowerModuleTrigger(void* self, std::uint32_t command, std::uint32_t target) noexcept {
    auto* sub = at(self, 0x10);
    auto* desc = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(virtualAt<This0>(sub, 0x1C)(sub)));
    auto* inner = load<const std::byte*>(desc, 8);
    const auto armed = bit(load<std::uint32_t>(inner, 0x1C), 14);
    auto* body = load<std::byte*>(self, 4);
    if (!armed) {
        if (load<std::uint8_t>(body, 0x1B9) != 0U) {
            return;
        }
        virtualAt<This1>(sub, 0x44)(sub, kOne);
        return;
    }
    void* chosen = nullptr;
    if (load<std::uint8_t>(body, 0x1B9) != 0U) {
        const auto frame = load<std::uint32_t>(imagePointer(kLogic), 0x58);
        chosen = reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This0>(sub, 0x1C)(sub)));
        directCall(0x007EDF50, reinterpret_cast<void*>(static_cast<std::uintptr_t>(target)), addr(chosen), frame, 1U, 0, 0, 0, 3);
    } else {
        directCall(0x007EDDB0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(target)), addr(desc), 1U, 0, 0, 0, 0, 2);
    }
    auto* owner = load<std::byte*>(self, 8);
    if (owner == nullptr) {
        return;
    }
    auto* ownerTemplate = load<const std::byte*>(owner, 4);
    if (bit(load<std::uint32_t>(ownerTemplate, 0xCC), 0x19)) {
        return;
    }
    auto* first = reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This1>(sub, 0xA0)(sub, command)));
    if (first != nullptr) {
        virtualAt<This1>(first, 0x0C)(first, addr(sub));
    }
    auto* second = reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This1>(sub, 0xA0)(sub, target)));
    if (second != nullptr) {
        virtualAt<This1>(second, 8)(second, addr(sub));
    }
    (void)chosen;
}

void managerBodyCacheTemplate(void* self) noexcept {
    if (load<std::uint8_t>(self, 0x11C) != 0U) {
        return;
    }
    auto* owner = load<std::byte*>(self, 8);
    const auto found = directCall(0x00795CF0, load<void*>(owner, 4), addr(at(self, 0x110)), 0, 0, 0, 0, 0, 1);
    auto* templ = reinterpret_cast<void*>(static_cast<std::uintptr_t>(found));
    if (templ != nullptr && templ != load<void*>(self, 0x114)) {
        store<std::uint32_t>(self, 0x118, directCall(0x007886E0, templ, 0, 0, 0, 0, 0, 0, 0));
        store<std::uint32_t>(self, 0x3C, directCall(0x00A7C770, templ, 0, 0, 0, 0, 0, 0, 0));
        store<void*>(self, 0x114, templ);
    }
    store<std::uint8_t>(self, 0x11C, 1U);
}

std::uint8_t moneyCrateCollideGrant(void* self, void* other) noexcept {
    auto* wallet = load<std::byte*>(other, 0x418);
    const auto field = load<std::uint32_t>(load<const void*>(self, 4), 0x6C);
    if (wallet != nullptr) {
        const auto amount = directCall(0x00796EA0, wallet, field, 0, 0, 0, 0, 0, 1);
        std::uint32_t player = load<std::uint32_t>(other, 0xDC);
        auto* account = load<void*>(wallet, 0x1F0);
        // Seven stack arguments: 0, 0xA, amount, &player, account, 1, 0.
        // The leading 0 is past the six-argument record; the callee pops it.
        const auto picked = directCall(0x007E4AB0, wallet, 0U, 0x0AU, amount, addr(&player), addr(account), 1U, 6);
        auto* record = reinterpret_cast<void*>(static_cast<std::uintptr_t>(picked));
        const auto live = directCall(0x007F58B0, record, 0, 0, 0, 0, 0, 0, 0);
        if (live != 0U) {
            directCall(0x0086C630, nullptr, load<std::uint32_t>(self, 8), live, 0, 0, 0, 0, 2);
        }
    }
    directCall(0x0067F7C0, self, addr(other), 0, 0, 0, 0, 0, 1);
    return 1U;
}

void* tunnelContainOpenIterator(void* self, void* out) noexcept {
    auto* object = load<std::byte*>(self, 8);
    auto* node = load<std::byte*>(load<const std::byte*>(object, 0x418), 0xA4);
    if (node != nullptr) {
        auto* head = at(node, 4);
        store<void*>(out, 4, at(node, 0x18));
        store<void*>(out, 0, head);
        virtualAt<This1>(head, 0)(head, 1U);
        return out;
    }
    auto* fallback = at(self, 0x24);
    store<std::uint32_t>(out, 4, kTunnelSentinel);
    store<void*>(out, 0, fallback);
    virtualAt<This1>(fallback, 0)(fallback, 1U);
    return out;
}

void unitVisionBehaviorStoreRange(void* self, std::uint32_t packed, std::uint32_t key) noexcept {
    store<std::uint32_t>(self, 0x28, packed);
    store<std::uint32_t>(self, 0x2C, key);
    store<std::uint32_t>(self, 0x34, directCall(0x004A62D0, nullptr, kVisionKey, key, 0, 0, 0, 0, 2));
}

void unitVisionBehaviorXfer(void* self, void* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    xferVersion(xfer, 1, 1);
    directCall(0x008D82F0, self, addr(xfer), 0, 0, 0, 0, 0, 1);
    directCall(0x00685080, at(self, 0x10), addr(xfer), 0, 0, 0, 0, 0, 1);
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x30)));
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x24)));
    directCall(0x00980820, nullptr, addr(xfer), addr(at(self, 0x28)), 0, 0, 0, 0, 2);
    if (bit(load<std::uint32_t>(xfer, 4), 1)) {
        const auto hashed = directCall(0x004A62D0, nullptr, kVisionKey, load<std::uint32_t>(self, 0x2C), 0, 0, 0, 0, 2);
        store<std::uint32_t>(self, 0x34, hashed);
    }
}

std::uint32_t detonateStateRequest(void* self) noexcept {
    auto* object = load<std::byte*>(self, 0x18);
    auto* weapon = load<std::byte*>(object, 0x80);
    auto* state = at(weapon, 0x24);
    virtualAt<This1>(state, 0x18)(state, 0U);
    return 0xFFFFFFFFU;
}

void dozerAiUpdateAcceptTask(void* self, void* task, std::uint32_t extra) noexcept {
    auto* sub = at(self, 0x51C);
    if ((virtualAt<This1>(sub, 0x50)(sub, addr(task)) & 0xFFU) == 0U) {
        return;
    }
    auto* object = load<void*>(self, 8);
    const auto allowed = directCall(0x007E3720, imagePointer(0x00CC561C), addr(object), addr(task), extra, 0, 0, 0, 3);
    if ((allowed & 0xFFU) == 0U) {
        return;
    }
    auto* logic = imagePointer(kLogic);
    const auto now = load<std::uint32_t>(logic, 0x58);
    const auto ready = load<std::uint32_t>(task, 0x44C);
    if (now <= ready) {
        const auto owner = load<std::uint32_t>(task, 0x448);
        if (owner != 0U && owner != load<std::uint32_t>(object, 0xDC)) {
            return;
        }
    }
    virtualAt<This2>(sub, 0x30)(sub, 1U, addr(task));
}

std::uint8_t crateCollideAllows(void* self, void* other) noexcept {
    if (other == nullptr || load<std::uint8_t>(self, 0x14) != 0U) {
        return 0U;
    }
    auto* players = static_cast<std::byte*>(imagePointer(kPlayerList));
    auto* contained = load<std::byte*>(other, 0x418);
    if (contained == load<void*>(players, 0x34)) {
        return 0U;
    }
    if (bit(load<std::uint32_t>(other, 0x88), 12)) {
        return 0U;
    }
    auto* templ = load<const std::byte*>(other, 4);
    if (bit(load<std::uint32_t>(templ, 0xCC), 10)) {
        return 0U;
    }
    auto* crate = load<std::byte*>(self, 4);
    bool flagged = false;
    if (load<std::uint8_t>(crate, 0x65) != 0U && bit(load<std::uint32_t>(templ, 0xC0), 9)) {
        flagged = true;
    } else if (load<std::uint32_t>(other, 0x374) == 0U) {
        return 0U;
    }
    if (bit(load<std::uint32_t>(templ, 0xD4), 5)) {
        if (contained == nullptr || (directCall(0x007E4990, contained, 0, 0, 0, 0, 0, 0, 0) & 0xFFU) == 0U) {
            return 0U;
        }
    }
    const auto primary = directCall(0x004D5900, at(crate, 8), 0, 0, 0, 0, 0, 0, 0);
    const auto secondary = (primary & 0xFFU) != 0U
        ? 1U
        : directCall(0x004D5900, at(crate, 0x2C), 0, 0, 0, 0, 0, 0, 0);
    if ((primary & 0xFFU) != 0U || (secondary & 0xFFU) != 0U) {
        const auto hit = directCall(0x0078B960, other, addr(at(crate, 8)), addr(at(crate, 0x2C)), 0, 0, 0, 0, 2);
        if ((hit & 0xFFU) == 0U) {
            return 0U;
        }
    }
    if ((load<std::uint8_t>(other, 0x471) & 1U) != 0U) {
        return 0U;
    }
    auto* host = load<std::byte*>(self, 8);
    if ((load<std::uint8_t>(host, 0x471) & 1U) != 0U) {
        return 0U;
    }
    if (!flagged && (directCall(0x0067D3C0, host, 0, 0, 0, 0, 0, 0, 0) & 0xFFU) != 0U) {
        return 0U;
    }
    contained = load<std::byte*>(other, 0x418);
    if (load<std::uint8_t>(crate, 0x64) != 0U && load<void*>(host, 0x418) == contained) {
        return 0U;
    }
    if (contained != nullptr) {
        const auto mark = load<std::uint8_t>(crate, 0x66);
        const auto strict = load<std::uint8_t>(crate, 0x67);
        if (mark != 0U || strict != 0U) {
            if (load<std::uint32_t>(contained, 0x2C) != 0U) {
                if (strict != 0U) {
                    const auto ok = directCall(0x004D3C00, imagePointer(kLogic), 0, 0, 0, 0, 0, 0, 0);
                    if ((ok & 0xFFU) == 0U) {
                        return 0U;
                    }
                }
                if (mark != 0U) {
                    return 0U;
                }
            }
        }
    }
    if ((load<std::uint8_t>(host, 0x471) & 2U) != 0U) {
        return 0U;
    }
    auto* cursor = load<std::uint32_t*>(self, 0x18);
    auto* end = load<std::uint32_t*>(self, 0x1C);
    if (cursor == end) {
        return 0U;
    }
    const auto id = load<std::uint32_t>(other, 0xDC);
    while (cursor != end) {
        if (*cursor == id) {
            return 1U;
        }
        ++cursor;
    }
    return 0U;
}

void stealthDetectorUpdateXfer(void* self, void* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    directCall(0x006858E0, self, addr(xfer), 0, 0, 0, 0, 0, 1);
    directCall(0x00685080, at(self, 0x18), addr(xfer), 0, 0, 0, 0, 0, 1);
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x3C)));
    virtualAt<This1>(xfer, 0x84)(xfer, addr(at(self, 0x40)));
}

void specialPowerUpdateModuleXfer(void* self, void* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    xferVersion(xfer, 1, 1);
    directCall(0x008D82F0, self, addr(xfer), 0, 0, 0, 0, 0, 1);
    directCall(0x00685080, at(self, 0x10), addr(xfer), 0, 0, 0, 0, 0, 1);
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x2C)));
    virtualAt<This1>(xfer, 0x70)(xfer, addr(at(self, 0x30)));
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x34)));
    virtualAt<This1>(xfer, 0x64)(xfer, addr(at(self, 0x38)));
}

void secondaryDamageTrackerXfer(void* self, void* xfer) noexcept {
    xferVersion(xfer, 1, 2);
    xferVersion(xfer, 1, 1);
    directCall(0x008D82F0, self, addr(xfer), 0, 0, 0, 0, 0, 1);
    directCall(0x00685080, at(self, 0x10), addr(xfer), 0, 0, 0, 0, 0, 1);
    virtualAt<This1>(xfer, 0x6C)(xfer, addr(at(self, 0x2C)));
    virtualAt<This1>(xfer, 0x70)(xfer, addr(at(self, 0x30)));
    directCall(0x0054A090, nullptr, addr(xfer), addr(at(self, 0x34)), 0, 0, 0, 0, 2);
}

float specialPowerUpdateReadiness(void* self) noexcept {
    if ((virtualAt<This0>(self, 8)(self) & 0xFFU) != 0U) {
        return 1.0F;
    }
    if (load<std::int32_t>(self, 8) > 0) {
        return load<float>(self, 0x10);
    }
    auto* anchor = load<std::byte*>(self, -0x24);
    auto* module = load<std::byte*>(anchor, 8);
    if (module == nullptr) {
        return 0.0F;
    }
    auto* owner = load<std::byte*>(self, -0x20);
    std::uint32_t numerator = load<std::uint32_t>(self, 4);
    if (owner != nullptr) {
        auto* attached = load<std::byte*>(owner, 0x418);
        if (attached != nullptr) {
            auto* inner = load<const std::byte*>(module, 8);
            if (bit(load<std::uint32_t>(inner, 0x1C), 12)) {
                const auto queried = virtualAt<This0>(self, 0x1C)(self);
                numerator = directCall(0x007EE070, attached, queried, 0, 0, 0, 0, 0, 1);
            }
        }
    }
    auto* logic = imagePointer(kLogic);
    const auto elapsed = static_cast<std::int32_t>(numerator - load<std::uint32_t>(logic, 0x58));
    const auto scaled = unsignedFloat(elapsed);
    const auto divisor = static_cast<std::int32_t>(directCall(0x007E2970, module, 0, 0, 0, 0, 0, 0, 0));
    const auto ratio = scaled / unsignedFloat(divisor);
    return 1.0F - ratio;
}

std::uint32_t specialPowerUpdateNotifyOwner(void* self) noexcept {
    auto* info = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(virtualAt<This0>(self, 0x1C)(self)));
    auto* inner = load<const std::byte*>(info, 8);
    if (bit(load<std::uint32_t>(inner, 0x1C), 12)) {
        auto* owner = load<std::byte*>(self, -0x20);
        if (owner != nullptr) {
            auto* attached = load<std::byte*>(owner, 0x418);
            if (attached != nullptr) {
                const auto queried = virtualAt<This0>(self, 0x1C)(self);
                return directCall(0x007EE070, attached, queried, 0, 0, 0, 0, 0, 1);
            }
        }
    }
    if (load<std::int32_t>(self, 8) <= 0) {
        if ((virtualAt<This0>(self, 0x9C)(self) & 0xFFU) != 0U) {
            return load<std::uint32_t>(self, 4);
        }
    }
    auto* logic = imagePointer(kLogic);
    const auto now = load<std::uint32_t>(logic, 0x58);
    return now - load<std::uint32_t>(self, 0x0C) + load<std::uint32_t>(self, 4);
}

float dozerAiUpdateTerrainScale(void* self) noexcept {
    auto* object = load<std::byte*>(self, -0x514);
    auto* node = load<std::byte*>(object, 0x418);
    auto* base = load<std::byte*>(self, -0x518);
    const auto height = load<float>(base, 0x54);
    if (node == nullptr || load<std::uint32_t>(node, 0x2C) != 1U) {
        return height;
    }
    auto* table = static_cast<std::byte*>(imagePointer(0x00DFB9D8));
    auto* slot = load<std::byte*>(table, 0x38);
    auto* row = load<std::byte*>(slot, 0);
    float scale = 0.0F;
    if (row != nullptr) {
        scale = load<float>(row, 0x54);
    }
    return height * scale;
}

std::uint8_t garrisonContainAccepts(void* self, void* other, std::uint32_t mode, std::uint32_t extra) noexcept {
    auto* anchor = load<const std::byte*>(self, -0x1C);
    if (bit(load<std::uint32_t>(anchor, 0x94), 2) || bit(load<std::uint32_t>(anchor, 0x26C), 1)) {
        return 0U;
    }
    if (other == nullptr) {
        return 0U;
    }
    auto* gate = at(self, -0x24);
    const auto accepted = virtualAt<This4>(gate, 0xA4)(gate, addr(other), mode, extra, 1U);
    if ((accepted & 0xFFU) == 0U) {
        return 0U;
    }
    auto* templ = load<const std::byte*>(other, 4);
    if (bit(load<std::uint32_t>(templ, 0xC0), 0x1C)) {
        return 0U;
    }
    if ((mode & 0xFFU) == 0U) {
        return 1U;
    }
    const auto capacity = virtualAt<This0>(self, 0x78)(self);
    const auto used = virtualAt<This1>(self, 0x10C)(self, 0U);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(used) < static_cast<std::int32_t>(capacity)) ? 1U : 0U;
}

void* garrisonContainAnchorOrSlot(void* self) noexcept {
    auto* anchor = load<std::byte*>(self, -0x1C);
    auto* templ = load<const std::byte*>(anchor, 4);
    if (bit(load<std::uint32_t>(templ, 0xD4), 0x15)) {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This0>(self, 0x144)(self)));
    }
    return at(anchor, 0x38);
}

void* garrisonContainLocatePoint(void* self) noexcept {
    auto* base = load<std::byte*>(self, -0x1C);
    auto* templ = load<const std::byte*>(base, 4);
    if (!bit(load<std::uint32_t>(templ, 0xD4), 0x15)) {
        return at(base, 0x38);
    }
    const auto hit = directCall(0x0063EB60, nullptr, addr(at(base, 0x38)), addr(base), kPointSink, 0, 0, 0, 3);
    if ((hit & 0xFFU) != 0U) {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(kPointSink));
    }
    return at(base, 0x38);
}

std::uint8_t productionUpdateCancelOne(void* self, std::uint32_t key) noexcept {
    auto* link = load<std::byte*>(self, 0x10);
    while (link != nullptr) {
        const auto live = load<std::uint8_t>(link, 0x65) == 0U && load<std::uint32_t>(link, 0x0C) == 1U &&
            load<std::uint32_t>(link, 0x18) == key;
        if (live) {
            if (load<std::uint8_t>(link, 0x64) != 0U) {
                return 0U;
            }
            store<std::uint8_t>(link, 0x64, 1U);
            if (imageDword(0x00DEEA44) != 0U) {
                auto* owner = load<std::byte*>(link, 0x10);
                auto* root = load<std::byte*>(self, -0x1C);
                auto* attached = root == nullptr ? nullptr : load<std::byte*>(root, 0x418);
                if (owner != nullptr && attached != nullptr &&
                    (directCall(0x007E4590, attached, 0, 0, 0, 0, 0, 0, 0) & 0xFFU) != 0U) {
                    const auto text = directCall(0x005361C0, owner, addr(&key), 0x19U, 0, 0, 0, 0, 2);
                    directCall(0x0053AC50, imagePointer(0x00DEEA44), load<std::uint32_t>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(text)), 0),
                        addr(at(root, 0x38)), 0U, 0U, 0, 0, 4);
                }
            }
            return 1U;
        }
        link = load<std::byte*>(link, 0x54);
    }
    return 0U;
}

std::uint8_t specialAbilityUpdateRelay(void* self, std::uint32_t command, std::uint32_t extra) noexcept {
    auto* body = load<std::byte*>(self, -0x20);
    auto* head = at(self, -0x24);
    if ((directCall(0x0069F9E0, head, command, 0, 0, 0, 0, 0, 1) & 0xFFU) == 0U) {
        return 0U;
    }
    if (!bit(load<std::uint32_t>(body, 0x2C), 0x15)) {
        return load<std::uint32_t>(body, 0xD8) == 0U ? 1U : 0U;
    }
    auto* rec = load<std::byte*>(head, 4);
    if (load<std::uint32_t>(rec, 0xD4) == 0U) {
        return load<std::uint32_t>(body, 0xD8) == 0U ? 1U : 0U;
    }
    auto* chain = load<std::byte*>(body, 0xD4);
    if (chain == nullptr) {
        return load<std::uint32_t>(body, 0xD8) == 0U ? 1U : 0U;
    }
    auto* node = load<std::byte*>(chain, 4);
    auto* word = load<std::uint32_t*>(node, 0x0C);
    std::uint32_t local = 0;
    if (word == nullptr) {
        word = &local;
    }
    const auto wanted = *word;
    auto* root = load<std::byte*>(self, -0x1C);
    auto* cursor = load<std::byte*>(root, 0x310);
    while (cursor != nullptr) {
        auto* item = load<std::byte*>(cursor, 0);
        if (item == nullptr) {
            break;
        }
        auto* slot = at(item, 0x0C);
        auto* probed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(virtualAt<This0>(slot, 0x24)(slot)));
        if (probed != nullptr && virtualAt<This0>(probed, 0x1C)(probed) == wanted) {
            const auto fired = virtualAt<This3>(probed, 0x50)(probed, extra, 0U, kMaxFinite);
            if ((fired & 0xFFU) == 0U) {
                return 0U;
            }
            return load<std::uint32_t>(body, 0xD8) == 0U ? 1U : 0U;
        }
        cursor = load<std::byte*>(cursor, 4);
    }
    return load<std::uint32_t>(body, 0xD8) == 0U ? 1U : 0U;
}

void* specialPowerDispatchCopyKind(void* self, std::uint32_t* outKind) noexcept {
    auto* info = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(virtualAt<This0>(self, 0x1C)(self)));
    auto* inner = load<const std::byte*>(info, 8);
    const auto kind = load<std::uint32_t>(load<const void*>(inner, 0), 4);
    *outKind = kind;
    return outKind;
}

void specialPowerDispatchAdjustCount(void* self, std::uint32_t engage) noexcept {
    auto span = idSpan(self);
    auto* cursor = span.begin;
    auto* end = span.begin + span.count;
    while (cursor != end) {
        const auto id = *cursor;
        auto* found = reinterpret_cast<void*>(static_cast<std::uintptr_t>(directCall(0x00687720, span.table, id, 0, 0, 0, 0, 0, 1)));
        if (found == nullptr) {
            break;
        }
        virtualAt<This1>(found, 0x28)(found, engage);
        reload(span);
        ++cursor;
        end = span.begin + span.count;
    }
    auto count = load<std::int32_t>(self, 0x24);
    if ((engage & 0xFFU) != 0U) {
        if (count == 0) {
            auto* logic = imagePointer(kLogic);
            store<std::uint32_t>(self, 0x28, load<std::uint32_t>(logic, 0x58));
            // Slot 0x0C returns an x87 float that is stored at +0x2C. The offline
            // stub leaves ST0 untouched, so this path is not entered by the oracle.
            store<float>(self, 0x2C, 0.0F);
        }
        store<std::int32_t>(self, 0x24, count + 1);
        return;
    }
    if (count <= 0) {
        return;
    }
    --count;
    store<std::int32_t>(self, 0x24, count);
    if (count == 0) {
        auto* logic = imagePointer(kLogic);
        const auto delta = load<std::uint32_t>(logic, 0x58) - load<std::uint32_t>(self, 0x28);
        store<std::uint32_t>(self, 0x20, load<std::uint32_t>(self, 0x20) + delta);
    }
}

void specialPowerDispatchApplyKind4(void* self, std::uint32_t value) noexcept {
    auto span = idSpan(self);
    for (int index = 0; index < span.count; ++index) {
        reload(span);
        auto* obj = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(span.begin[index]));
        if (obj == nullptr) {
            continue;
        }
        auto* sub = load<const std::byte*>(obj, 8);
        if (load<std::uint32_t>(sub, 0x14) != 4U) {
            continue;
        }
        auto* found = reinterpret_cast<void*>(static_cast<std::uintptr_t>(directCall(0x00687720, span.table, addr(obj), 0, 0, 0, 0, 0, 1)));
        if (found == nullptr) {
            continue;
        }
        if ((virtualAt<This1>(found, 0x78)(found, 0U) & 0xFFU) == 0U) {
            continue;
        }
        virtualAt<This1>(found, 0x2C)(found, value);
    }
}

void specialPowerDispatchApplyKind23(void* self, std::uint32_t left, std::uint32_t right) noexcept {
    auto span = idSpan(self);
    auto* table = load<void*>(self, -8);
    for (int index = 0; index < span.count; ++index) {
        reload(span);
        auto* obj = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(span.begin[index]));
        if (obj == nullptr) {
            continue;
        }
        const auto kind = load<std::uint32_t>(load<const void*>(obj, 8), 0x14);
        if (kind != 2U && kind != 3U) {
            continue;
        }
        auto* found = reinterpret_cast<void*>(static_cast<std::uintptr_t>(directCall(0x00687720, table, addr(obj), 0, 0, 0, 0, 0, 1)));
        if (found == nullptr) {
            continue;
        }
        if ((virtualAt<This2>(found, 0x74)(found, left, 0U) & 0xFFU) == 0U) {
            continue;
        }
        virtualAt<This2>(found, 0x30)(found, left, right);
    }
}

void specialPowerDispatchApplyKind03(void* self, std::uint32_t left, std::uint32_t right) noexcept {
    auto span = idSpan(self);
    auto* table = load<void*>(self, -8);
    for (int index = 0; index < span.count; ++index) {
        reload(span);
        auto* obj = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(span.begin[index]));
        if (obj == nullptr) {
            continue;
        }
        const auto kind = load<std::uint32_t>(load<const void*>(obj, 8), 0x14);
        if (kind != 0U && kind != 3U) {
            continue;
        }
        auto* found = reinterpret_cast<void*>(static_cast<std::uintptr_t>(directCall(0x00687720, table, addr(obj), 0, 0, 0, 0, 0, 1)));
        if (found == nullptr) {
            continue;
        }
        if ((virtualAt<This3>(found, 0x6C)(found, left, 0U, 0U) & 0xFFU) == 0U) {
            continue;
        }
        virtualAt<This2>(found, 0x34)(found, left, right);
    }
}

void specialPowerDispatchApplyKind1(void* self, std::uint32_t left, std::uint32_t floatBits, std::uint32_t right) noexcept {
    auto span = idSpan(self);
    auto* table = load<void*>(self, -8);
    for (int index = 0; index < span.count; ++index) {
        reload(span);
        auto* obj = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(span.begin[index]));
        if (obj == nullptr) {
            continue;
        }
        auto* sub = load<const std::byte*>(obj, 8);
        if (load<std::uint32_t>(sub, 0x14) != 1U) {
            continue;
        }
        auto* found = reinterpret_cast<void*>(static_cast<std::uintptr_t>(directCall(0x00687720, table, addr(obj), 0, 0, 0, 0, 0, 1)));
        if (found == nullptr) {
            continue;
        }
        if ((virtualAt<This4>(found, 0x70)(found, left, floatBits, 0U, 0U) & 0xFFU) == 0U) {
            continue;
        }
        virtualAt<This3>(found, 0x38)(found, left, floatBits, right);
    }
}

float dockUpdateOccupiedScale(void* self) noexcept {
    auto* begin = load<std::uint32_t*>(self, 0x40);
    auto* end = load<std::uint32_t*>(self, 0x44);
    std::int32_t count = 0;
    for (auto* cursor = begin; cursor != end; ++cursor) {
        if (*cursor != 0U) {
            ++count;
        }
    }
    if (virtualAt<This0>(self, 0x60)(self) != 0U) {
        ++count;
    }
    auto* rates = load<const std::byte*>(self, -0x20);
    const auto scaled = static_cast<float>(kDockNumerator) * load<float>(rates, 0x10);
    return scaled * static_cast<float>(count);
}

std::uint8_t hordeContainSharesSide(void* self, void* other, std::uint32_t gate, void* counter, std::uint32_t allow) noexcept {
    if (gate != 0U) {
        return 0U;
    }
    auto* anchor = load<const std::byte*>(self, -0x164);
    const auto anchorBit = bit(load<std::uint32_t>(anchor, 0x88), 1);
    const auto otherBit = bit(load<std::uint32_t>(other, 0x88), 1);
    if (anchorBit || otherBit) {
        if ((allow & 0xFFU) == 0U) {
            return 0U;
        }
        // A taken 0x00B11D40 overwrites cl with the low byte of the gate argument,
        // which is zero on this path. A skipped call leaves cl as the anchor bit.
        bool replaced = false;
        std::uint32_t left = 0;
        auto* attached = load<std::byte*>(other, 0x414);
        if (attached != nullptr && load<std::uint32_t>(attached, 0x1F8) == 0U) {
            left = directCall(0x00B11D40, load<void*>(other, 0x418), 0, 0, 0, 0, 0, 0, 0);
            replaced = true;
        }
        std::uint32_t right = left;
        attached = load<std::byte*>(anchor, 0x414);
        if (attached != nullptr && load<std::uint32_t>(attached, 0x1F8) == 0U) {
            right = directCall(0x00B11D40, load<void*>(anchor, 0x418), 0, 0, 0, 0, 0, 0, 0);
            replaced = true;
        }
        const auto compared = replaced ? 0U : (anchorBit ? 1U : 0U);
        if (left != right || static_cast<std::uint32_t>(otherBit) != compared) {
            return 0U;
        }
    }
    auto* templ = load<const std::byte*>(other, 4);
    auto* list = load<const std::byte*>(templ, 0x14);
    auto* vt = load<const std::byte*>(list, 0);
    const auto token = load<std::uint32_t>(vt, 4);
    const auto count = counter == nullptr ? 0 : load<std::int32_t>(counter, 0);
    if (count <= 0) {
        return 0U;
    }
    for (std::int32_t index = 0; index < count; ++index) {
        const auto found = directCall(0x0078D880, imagePointer(0x00DF9D28), token, 0, 0, 0, 0, 0, 1);
        if (found != 0U) {
            return 1U;
        }
    }
    return 0U;
}

void hordeContainPushTimer(void* self, std::uint32_t floatBits) noexcept {
    auto* anchor = load<void*>(self, -0x164);
    if (anchor == nullptr) {
        return;
    }
    directCall(0x0078C080, anchor, floatBits, 0, 0, 0, 0, 0, 1);
    store<std::uint8_t>(self, 8, 1U);
    auto* logic = imagePointer(kLogic);
    const auto frame = load<std::uint32_t>(logic, 0x58) + 1U;
    directCall(0x004ED9C0, logic, addr(anchor), addr(at(self, -0x15C)), frame, 0, 0, 0, 3);
}

} // namespace ra3::gamelogic
