#pragma once

#include <cstdint>

namespace ra3::gameclient {

// Drawable accessor and predicate cluster from ra3_1.0.game, image range
// 0x00520000-0x0052FFFF.  Ownership is a Drawable vtable slot or a direct
// call edge to one of those slots or to the Drawable.cpp anchors, not
// address adjacency.  The anchors are the Drawable.cpp-asserting helper 0x0051FCB0
// (source path SageEngine\source\GameEngine\gameclient\Drawable.cpp) and
// Drawable::xfer at 0x0053EE30 (string "Drawable::xfer").
//
// Primary vtable 0x00BE1058 is installed by 0x0053CAF0 and 0x00542A10.
// The class name "Drawable" follows it at 0x00BE1080.  Secondary vtable
// 0x00BE1048 is installed at this+0x60 by the same constructors.
// TintEnvelope vtable 0x00BDFFD4 is installed by Drawable::xfer.
//
// work/references/generals/GeneralsMD/Code/GameEngine/Include/GameClient/Drawable.h
// and Source/GameClient/Drawable.cpp were read only as architecture
// (Thing + Snapshot, module lists, tint, opacity).  RA3 offsets below are
// the offsets in the shipped instructions.  No Generals code is copied.
//
// Null this is outside the contract wherever the body loads through ECX.
// Byte operations that are `mov al, ...` keep EAX[31:8].  Functions that
// execute `lahf` after `ucomiss` define AH from that compare; AF (AH bit 4)
// is not defined by ucomiss.

constexpr std::uint32_t kTintEnvelopeStatic = 0x00BDFFE4;
constexpr std::uint32_t kMissingRecord = 0x00DF0408;
constexpr std::uint32_t kAuxObjectVptr = 0x00BDF9F8;
constexpr std::uint32_t kSwayStateVptr = 0x00BDF9FC;
constexpr std::uint32_t kIdentityDiagonal = 0x3F800000;
constexpr std::uint32_t kHalfPiBits = 0x3FC90FDB;
constexpr std::uint32_t kNegativeZeroBits = 0x80000000;
constexpr std::uint32_t kSwayClampHighBits = 1067506044u;
constexpr std::uint32_t kSwayClampLowBits = 3214989692u;
constexpr std::uint32_t kRandomHighBits = 1086918619u;
constexpr std::uint8_t kScalarPublishBit = 2;

struct PositionGlobals {
    float xyz[3];
    std::uint32_t ready;
};

struct TransformGlobals {
    float cells[12];
    std::uint32_t ready;
};

// External calls the shipped bodies make.  The checker and the offline
// oracle supply the same answers.  `slot` is the vtable byte offset.
class DrawableServices {
public:
    virtual ~DrawableServices() = default;
    virtual std::uint32_t callSlot(
        void* object, std::uint32_t slot, const std::uint32_t* args, int argCount,
        std::uint32_t eax) = 0;
    virtual std::uint32_t callDirect(
        std::uint32_t address, void* ecx, const std::uint32_t* args, int argCount,
        std::uint32_t eax) = 0;
    virtual void* allocate(std::uint32_t bytes) = 0;
    virtual float randomReal(float low, float high, std::uint32_t nameKey) = 0;
};

// 0x005261A0 TintEnvelope vtable 0x00BDFFD4 slot 2.  EAX = 0x00BDFFE4.
std::uint32_t tintEnvelopeStatic() noexcept;

// 0x0052A800.  Twelve dword copies, fld/fstp in address order.  EAX = dest.
// Direct callee of secondary slot 0x0052DEF0.  ret 4.
std::uint32_t copyTwelveDwords(void* destination, const void* source) noexcept;

// 0x00525CF0.  Direct callee of the Drawable.cpp anchor 0x0052C1C0.
// Returns this+0x124, or the global at 0x00DEED68 when [this+0x130] == -1.
std::uint32_t positionPointer(void* self, PositionGlobals* globals) noexcept;

// 0x0052D790.  Direct callee of Drawable::xfer 0x0053EE30.
// Returns this+0xF4, or the global matrix at 0x00DEEDC0 when [this+0x130] == -1.
std::uint32_t transformPointer(void* self, TransformGlobals* globals) noexcept;

// 0x005209B0 primary vtable slot 8.  Walks [this+0x178], calls vt+0x20.
std::uint32_t visitModules20(void* self, DrawableServices& services, std::uint32_t eax);

// 0x00520F50 primary vtable slot 9.  Walks [this+0x178], vt+0xB0 then vt+0x70.
std::uint32_t queryModulesB0(
    void* self, std::uint32_t argument, DrawableServices& services, std::uint32_t eax);

// 0x00525BD0 primary vtable slot 5.  ORs bit value 2 at +0x13F, then
// publishes (arg0, arg1, float) to vt+0xA4 and vt+0x30.  ret 0xC.
std::uint32_t publishScalar(
    void* self, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t floatBits,
    DrawableServices& services, std::uint32_t eax);

// 0x00524050.  Direct callee of secondary slot 0x0052DEF0.
// Selects a condition byte and publishes it through vt+0x90.
std::uint32_t publishCondition(void* self, DrawableServices& services, std::uint32_t eax);

// 0x00524010.  Direct callee of 0x005371D0, which also calls 0x00524050
// and writes the +0x13D bit that publishCondition reads.
std::uint32_t publishArgument(
    void* self, std::uint32_t argument, DrawableServices& services, std::uint32_t eax);

// 0x00520170.  Direct callee of 0x0052C3D0 and 0x0052BAD0, both of which
// call the Drawable.cpp-asserting helper 0x0051FCB0 and store it at +0x208.
std::uint32_t addSwayOffset(void* self, void* record, void* point, std::uint32_t eax);

// 0x00520220.  Same callers as addSwayOffset.  Integrates [sway+0x18] and
// clamps it to +/- the dword at 0x00CAC484.
std::uint32_t integrateSwayAngle(void* self, void* record, float sample, std::uint32_t eax);

// 0x0052B960 / 0x0052B980.  Callers of 0x00524050.  ret 4.
std::uint32_t assignByte25A(
    void* self, std::uint32_t value, DrawableServices& services, std::uint32_t eax);
std::uint32_t assignByte25B(
    void* self, std::uint32_t value, DrawableServices& services, std::uint32_t eax);

// 0x00520380.  Direct callee of Drawable::xfer.  Ensures the 0x24-byte
// object at +0x328 (vptr 0x00BDF9F8).  Does not hook: it calls the allocator.
std::uint32_t ensureAuxRecord(void* self, DrawableServices& services);

// 0x00525D40.  Direct callee of Drawable::xfer.  Notifies [0x00DF03FC] vt+0x6C
// and releases the pointer at +0x1F4.
std::uint32_t releaseTracked(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax);

// 0x00525DA0.  Direct caller of 0x00525D40.  Clears +0x13D bit 0x10.
std::uint32_t clearAttachedBit(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax);

// 0x00520CB0 TintEnvelope vtable slot 3.  ret 4.
std::uint32_t readTintEnvelope(
    void* self, void* xfer, DrawableServices& services, std::uint32_t eax);

// 0x00523140.  Direct callee of Drawable::xfer.  EAX = the xfer object.
std::uint32_t readCountedBlock(
    void* self, void* xfer, void* counter, DrawableServices& services);

// 0x005268E0.  Direct callee of 0x0052CDD0 and of 0x0052DA30, which
// 0x0052DBD0 calls after calling 0x00525D40.
std::uint32_t resolveRecord(void* self, std::uint32_t key, DrawableServices& services);

// 0x0052CDD0.  The predicate call is entry-recoverable 0x0052CD70 and is
// answered by DrawableServices, not by reading that body.
std::uint32_t selectRecordKey(void* self, std::uint32_t mode, DrawableServices& services);

// 0x0052DDB0.  Direct caller of 0x0052DC90.  Writes +0x13C bit 1.  ret 4.
std::uint32_t assignFlag13CBit1(
    void* self, std::uint32_t bit, DrawableServices& services, std::uint32_t eax);

// 0x0052DAE0.  Direct caller of 0x00525D40 and 0x0052D990.  Writes +0x13D bit 1.
std::uint32_t assignFlag13DBit1(
    void* self, std::uint32_t bit, void* registry, DrawableServices& services,
    std::uint32_t eax);

// 0x0052D990.  Direct caller of 0x00525D40.  ret 4.
std::uint32_t refreshIfVisible(
    void* self, std::uint32_t argument, void* registry, DrawableServices& services,
    std::uint32_t eax);

// 0x0052DBD0.  Direct caller of 0x00525D40.  Sets +0x13D bit 0x10.
std::uint32_t setAttachedAndRefresh(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax);

// 0x0052DC90.  Direct callee of Drawable::xfer and of 0x0052DDB0.
std::uint32_t refreshModuleGate(
    void* self, void* playerRoster, void* nameRegistry, DrawableServices& services,
    std::uint32_t eax);

// 0x0052DEF0 secondary vtable 0x00BE1048 slot 1.  `secondary` is primary+0x60.
std::uint32_t syncSecondary(void* secondary, DrawableServices& services, std::uint32_t eax);

// 0x00524310.  Direct caller of the Drawable.cpp-asserting helper 0x0051FCB0.
// ret 8.  `logicFrame` is the dword at [[0x00DEBA64]+0x58].
std::uint32_t updateSwayState(
    void* self, void* record, void* out, std::uint32_t logicFrame, DrawableServices& services);

// 0x0052D8F0.  Direct callee of 0x0052D990 and 0x0052DBD0.  ret 8.
// 0x0052D830 is answered by DrawableServices (its body calls protected
// 0x0052CD70 and the thunk 0x0052B060).
std::uint32_t installResolved(
    void* self, std::uint32_t mode, std::uint32_t flag, void* registry,
    DrawableServices& services, std::uint32_t eax);

// 0x0052DA30.  Direct callee of 0x0052DBD0.
std::uint32_t applyAlternate(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax);

// 0x0052D9E0.  Same flag gate as 0x0052D990.  ret 4.
std::uint32_t refreshSupplied(
    void* self, std::uint32_t identifier, void* registry, DrawableServices& services,
    std::uint32_t eax);

}  // namespace ra3::gameclient
