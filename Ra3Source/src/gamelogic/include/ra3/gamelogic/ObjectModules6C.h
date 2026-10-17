#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Callees of the 0x006C0000-0x006DFFFF object-module cluster. The checker and
// the Unicorn oracle install the same side effects. Cleanup matches each
// callee's stack use at the call site (thiscall ret N, or cdecl).
struct Object6CCalls {
    std::uint32_t(__fastcall* usable)(void* self, void* edx);                         // 0x006C0ED0
    std::uint32_t(__fastcall* steer)(void* self, void* edx, std::uint32_t pos,
                                     std::uint32_t bits, std::uint32_t mode);        // 0x006B67A0 ret 12
    std::uint32_t(__fastcall* place)(void* self, void* edx, void* pos,
                                     std::uint32_t bits);                            // 0x005AC4F0 ret 8
    std::uint32_t(__fastcall* aimAt)(void* self, void* edx, std::uint32_t target);    // 0x007997B0 ret 4
    std::uint32_t(__fastcall* pairA)(void* self, void* edx, std::uint32_t obj,
                                     std::uint32_t first, std::uint32_t second);     // 0x007E3640 ret 12
    std::uint32_t(__fastcall* apply59)(void* self, void* edx, std::uint32_t first,
                                       std::uint32_t second);                        // 0x006C59E0 ret 8
    std::uint32_t(__fastcall* pairB)(void* self, void* edx, std::uint32_t obj,
                                     std::uint32_t first, std::uint32_t second);     // 0x007E3110 ret 12
    std::uint32_t(__fastcall* apply5A)(void* self, void* edx, std::uint32_t first,
                                       std::uint32_t second);                        // 0x006C5AE0 ret 8
    std::uint32_t(__fastcall* dispatch5)(void* self, void* edx, std::uint32_t obj,
                                         void* record, std::uint32_t mode,
                                         std::uint32_t one, std::uint32_t zero);     // 0x007E3900 ret 20
    std::uint32_t(__fastcall* pushState)(void* self, void* edx, std::uint32_t code); // 0x0054F490 ret 4
    std::uint32_t(__fastcall* findWeapon)(void* self, void* edx, std::uint32_t id);  // 0x006A1770 ret 4
    std::uint32_t(__fastcall* findWeaponAlt)(void* self, void* edx, std::uint32_t found,
                                             std::uint32_t id);                      // 0x006A1720 ret 8
    std::uint32_t(__fastcall* readId)(void* self, void* edx, void* slot);            // 0x006A6770 ret 4
    std::uint32_t(__fastcall* lookup)(void* self, void* edx, std::uint32_t id);      // 0x004DEE90 ret 4
    std::uint32_t(__fastcall* applyNeg)(void* self, void* edx, std::uint32_t key,
                                        std::uint32_t flag);                         // 0x006C1C50 ret 8
    std::uint32_t(__fastcall* applyOne)(void* self, void* edx, std::uint32_t key);   // 0x006C1D60 ret 4
    std::uint32_t(__fastcall* collideBase)(void* self, void* edx, void* other);      // 0x0067F7C0 ret 4
    std::uint32_t(__fastcall* notifyBit)(void* self, void* edx, std::uint32_t code,
                                         std::uint32_t flag);                        // 0x006D1ED0 ret 8
    std::uint32_t(__fastcall* notifyAlt)(void* self, void* edx, std::uint32_t code,
                                         std::uint32_t flag);                        // 0x006D2020 ret 8
    std::uint32_t(__fastcall* findNode)(void* self, void* edx, void* outB, void* outA); // 0x00AAFE10 ret 8
    void(__cdecl* unlinkA)(void* node);                                              // 0x009C9980
    void(__cdecl* unlinkB)(void* node, void* end);                                   // 0x00B66F80
    void(__cdecl* release)(void* node);                                              // 0x004166B0
    std::uint32_t(__fastcall* allocNode)(void* self, void* edx, std::uint32_t size); // 0x004D36C0 ret 4
    std::uint32_t(__fastcall* lockName)(void* self, void* edx, std::uint32_t key);   // 0x00A21BE0 ret 4
    std::uint32_t(__fastcall* mapFind)(void* self, void* edx, void* out, void* key); // 0x0050B5F0 ret 8
    std::uint32_t(__fastcall* unlockName)(void* self, void* edx, std::uint32_t flag);// 0x00A21C90 ret 4
    std::uint32_t(__fastcall* acceptDamage)(void* self, void* edx, void* info,
                                            std::uint32_t who, std::uint32_t src);   // 0x005B7DE0 ret 12
    std::uint32_t(__fastcall* damageDelay)(void* self, void* edx, std::uint32_t key);// 0x005A8230 ret 4
    std::uint32_t(__fastcall* hideOne)(void* self, void* edx, void* obj);            // 0x007E16A0 ret 4
    std::uint32_t(__fastcall* hideTwo)(void* self, void* edx, void* obj);            // 0x007E1670 ret 4
    std::uint32_t(__fastcall* killObj)(void* self, void* edx, void* obj);            // 0x004EDD70 ret 4
    std::uint32_t(__fastcall* tailControl)(void* self, void* edx);                   // 0x006C8020
    std::uint32_t(__fastcall* schedule)(void* self, void* edx, void* at,
                                        std::uint32_t when);                         // 0x004D6960 ret 8
    std::uint32_t(__fastcall* ready)(void* self, void* edx);                         // 0x00538530
    std::uint32_t(__fastcall* fireAt)(void* self, void* edx, void* target,
                                      std::uint32_t zero);                           // 0x0052EFC0 ret 8
    void(__stdcall* enterLock)(std::uint32_t cs);                                    // IAT 0x00BBA1E4
    std::uint32_t(__stdcall* leaveLock)(std::uint32_t cs);                           // IAT 0x00BBA1E8
    std::uint32_t(__fastcall* readPoint)(void* self, void* edx, void* out);          // 0x0085CEE0 ret 4
};

// Absolute cells the bodies load. The checker points these at stand-in storage;
// the oracle writes the same pointer values into the shipped image.
struct Object6CEnv {
    std::uint32_t* clockCell;     // dword at 0x00DEBA64
    std::uint32_t* registryCell;  // dword at 0x00CC561C
    std::uint32_t* worldCell;     // dword at 0x00DFC190
    std::uint32_t* catalogCell;   // dword at 0x00DEBA80
    std::uint8_t* nameLock;       // object addressed by the immediate 0x00DEBB00
};

void setObject6CCalls(const Object6CCalls& calls) noexcept;
void setObject6CEnv(const Object6CEnv& env) noexcept;

// Dword at 0x00BE700C.
constexpr std::uint32_t kLimitBits = 0x7F7FFFFFu;
// Dword at 0x00BDB9A0 (0.0f) and 0x00BD9520 (0.1f).
constexpr std::uint32_t kZeroBits = 0x00000000u;
constexpr std::uint32_t kTenthBits = 0x3DCCCCCDu;
// Immediate stored by 0x006CC61F. The cell at that address is not loaded.
constexpr std::uint32_t kRecordTag = 0x00BFDC10u;
// Immediate pushed by the spawn/highlander name lock.
constexpr std::uint32_t kNameKey = 0x00BF605Cu;
// Critical-section immediate pushed by OpenContain::removeByKey.
constexpr std::uint32_t kContainLock = 0x00DF8A30u;
// Semantic-mutation site: shift used by StancesBehavior::setLinkedFlag (0x006D9A40).
constexpr std::uint32_t kStanceFlagShift = 20;

std::uint32_t AIUpdateInterface_followPosition(std::uint8_t* self, std::uint32_t pos,
                                               std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_followObject(std::uint8_t* self, std::uint32_t target,
                                             std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_steerState1B(std::uint8_t* self, std::uint32_t pos,
                                             std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_tryCommand59(std::uint8_t* self, std::uint32_t first,
                                             std::uint32_t second) noexcept;
std::uint32_t AIUpdateInterface_tryCommand5A(std::uint8_t* self, std::uint32_t first,
                                             std::uint32_t second) noexcept;
std::uint32_t AIUpdateInterface_weaponOrFollow(std::uint8_t* self, std::uint32_t target,
                                               std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_locoState0E(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_placeState2D(std::uint8_t* self, std::uint32_t pos,
                                             std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_locoState2D(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_locoState69(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_notifyPassenger(std::uint8_t* self, std::uint32_t flag,
                                                std::uint32_t code) noexcept;
std::uint32_t AIUpdateInterface_aimState11(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_tailState29(std::uint8_t* self, std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_tailState48(std::uint8_t* self, std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_aimState12(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_locoState13(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_locoState14(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_guardRect(std::uint8_t* self, std::uint32_t point,
                                          std::uint32_t aux, std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_state18(std::uint8_t* self, std::uint32_t aux,
                                        std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_followActor50(std::uint8_t* self, std::uint32_t actor,
                                              std::uint32_t aux, std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_followPoint50(std::uint8_t* self, std::uint32_t point,
                                              std::uint32_t aux, std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_guardPoint4F(std::uint8_t* self, std::uint32_t id,
                                             std::uint32_t aux, std::uint32_t mode,
                                             std::uint32_t point) noexcept;
std::uint32_t AIUpdateInterface_weaponMove3C(std::uint8_t* self, std::uint32_t target,
                                             std::uint32_t pos, std::uint32_t value,
                                             std::uint32_t mode) noexcept;
std::uint32_t AIUpdateInterface_matchRoster(std::uint8_t* self, std::uint32_t actor,
                                            std::uint32_t code) noexcept;
std::uint32_t AIUpdateInterface_walkRoster(std::uint8_t* self, std::uint32_t actor,
                                           std::uint32_t, std::uint32_t) noexcept;
std::uint32_t AIUpdateInterface_locoState38(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept;

std::uint32_t StancesBehavior_latchGoal(std::uint8_t* self) noexcept;
std::uint32_t StancesBehavior_setBit(std::uint8_t* self, std::uint32_t index) noexcept;
std::uint32_t StancesBehavior_clearBit(std::uint8_t* self, std::uint32_t index) noexcept;
std::uint32_t StancesBehavior_setLinkedFlag(std::uint8_t* self, std::uint32_t value) noexcept;
std::uint32_t StancesBehavior_setLinkedByte(std::uint8_t* self, std::uint32_t value) noexcept;
std::uint32_t StancesBehavior_releaseRecord(std::uint8_t* self, std::uint32_t key) noexcept;

std::uint32_t HighlanderBody_readIfUnset(std::uint8_t* self) noexcept;
std::uint32_t HighlanderBody_applyTemplate(std::uint8_t* self) noexcept;
std::uint32_t HighlanderBody_noteHit(std::uint8_t* self, std::uint8_t* info) noexcept;

std::uint32_t AttributeCrateCollide_onCollide(std::uint8_t* self, std::uint8_t* other) noexcept;
std::uint32_t ToggleDeploySpecialAbilityUpdate_tryDeploy(std::uint8_t* self, std::uint8_t* object) noexcept;
std::uint32_t HordeContain_removeMember(std::uint8_t* self, std::uint8_t* member) noexcept;
std::uint32_t OpenContain_removeByKey(std::uint8_t* self, std::uint32_t key) noexcept;
std::uint32_t PassiveAreaEffectBehavior_apply(std::uint8_t* self, std::uint8_t* target) noexcept;
std::uint32_t UnifiedControlBehavior_relay(std::uint8_t* self) noexcept;
std::uint32_t SpawnBehavior_update(std::uint8_t* self) noexcept;

}  // namespace ra3::gamelogic
