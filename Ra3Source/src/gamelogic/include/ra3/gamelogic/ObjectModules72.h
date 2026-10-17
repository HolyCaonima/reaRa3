#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Callees of the 0x00720000 object-module cluster. Checker stubs and the
// Unicorn oracle install the same side effects. Cleanup matches each
// callee's ret imm (cdecl callees leave the stack to the caller).
struct Object72Calls {
    std::uint32_t(__fastcall* usable)(void* self, void* edx);                         // 0x006C0ED0
    std::uint32_t(__fastcall* aimAt)(void* self, void* edx, std::uint32_t target);    // 0x007997B0 ret 4
    std::uint32_t(__fastcall* findWeapon)(void* self, void* edx, std::uint32_t id);   // 0x006A1770 ret 4
    std::uint32_t(__fastcall* findWeaponAlt)(void* self, void* edx, std::uint32_t found,
                                             std::uint32_t id);                       // 0x006A1720 ret 8
    std::uint32_t(__fastcall* followCd10)(void* self, void* edx, std::uint32_t arg);  // 0x0072CD10 ret 4
    std::uint32_t(__fastcall* followCe90)(void* self, void* edx, std::uint32_t arg);  // 0x0072CE90 ret 4
    std::uint32_t(__fastcall* followCf30)(void* self, void* edx, std::uint32_t arg);  // 0x0072CF30 ret 4
    std::uint32_t(__fastcall* followCdf0)(void* self, void* edx, void* arg);          // 0x0072CDF0 ret 4
    std::uint32_t(__fastcall* markObject)(void* self, void* edx, std::uint32_t arg);  // 0x006C0F40 ret 4
    std::uint32_t(__fastcall* prepareDed70)(void* self, void* edx, std::uint32_t arg);// 0x007DED70 ret 4
    std::uint32_t(__fastcall* queryLoco)(void* self, void* edx);                      // 0x005B9F30
    std::uint32_t(__fastcall* currentTarget)(void* self, void* edx);                  // 0x006EFE40
    std::uint32_t(__fastcall* readPoint)(void* self, void* edx, void* out);           // 0x0085CEE0 ret 4
    std::uint32_t(__fastcall* queryD2120)(void* self, void* edx);                     // 0x007D2120
    std::uint32_t(__fastcall* queryA18a0)(void* self, void* edx);                     // 0x006A18A0
    std::uint32_t(__fastcall* bindPair)(void* self, void* edx, void* other);          // 0x00714060 ret 4
    std::uint32_t(__fastcall* noteAa2a0)(void* self, void* edx, std::uint32_t arg);   // 0x005AA2A0 ret 4
    std::uint32_t(__fastcall* placeAc4f0)(void* self, void* edx, void* pos,
                                          std::uint32_t bits);                        // 0x005AC4F0 ret 8
    std::uint32_t(__fastcall* steerB67a0)(void* self, void* edx, void* pos,
                                          std::uint32_t bits, std::uint32_t mode);    // 0x006B67A0 ret 12
    std::uint32_t(__fastcall* applyKey)(void* self, void* edx, std::uint32_t key);    // 0x004D9AB0 ret 4
    std::uint32_t(__fastcall* triggerAbility)(void* self, void* edx, std::uint32_t flag); // 0x00731E00 ret 4
    std::uint32_t(__fastcall* abilityReady)(void* self, void* edx);                   // 0x00731C20
    std::uint32_t(__fastcall* xferBase88)(void* self, void* edx, std::uint32_t a,
                                          std::uint32_t b);                           // 0x00718B80 ret 8
    std::uint32_t(__fastcall* beamPulse)(void* self, void* edx);                      // 0x00710C90
    std::uint32_t(__fastcall* applyBeam)(void* self, void* edx, void* pos,
                                         std::uint32_t value);                        // 0x006C5030 ret 8
    std::uint32_t(__fastcall* notifyBit)(void* self, void* edx, std::uint32_t code,
                                         std::uint32_t flag);                         // 0x006D1ED0 ret 8
    std::uint32_t(__fastcall* allocNode)(void* self, void* edx, std::uint32_t size,
                                         std::uint32_t flags);                        // 0x004D99E0 ret 8
    std::uint32_t(__fastcall* growVector)(void* self, void* edx, std::uint32_t end,
                                          std::uint32_t valuePtr);                    // 0x008ADD50 ret 8
    std::uint32_t(__fastcall* xferRoot)(void* self, void* edx, void* xfer);           // 0x008D82F0 ret 4
    std::uint32_t(__fastcall* xferAt10)(void* self, void* edx, void* xfer);           // 0x00685080 ret 4
    std::uint32_t(__fastcall* xferStealth)(void* self, void* edx, void* xfer);        // 0x006858E0 ret 4
    std::uint32_t(__fastcall* resizeCrate)(void* self, void* edx, std::uint32_t n);   // 0x005C3300 ret 4
    std::uint32_t(__fastcall* resizeStrafe)(void* self, void* edx, std::uint32_t n);  // 0x0071D4E0 ret 4
    std::uint32_t(__fastcall* resizeStorage)(void* self, void* edx, std::uint32_t n); // 0x0071D540 ret 4
    std::uint32_t(__fastcall* xferShieldA)(void* self, void* edx, void* xfer);        // 0x0071D250 ret 4
    std::uint32_t(__fastcall* xferShieldB)(void* self, void* edx, void* xfer);        // 0x00707C20 ret 4
    std::uint32_t(__fastcall* xferShieldC)(void* self, void* edx, void* xfer);        // 0x0071D2F0 ret 4
    std::uint32_t(__fastcall* xferPower)(void* self, void* edx, void* xfer);          // 0x00681220 ret 4
    std::uint32_t(__fastcall* stopMonitor)(void* self, void* edx);                    // 0x007010D0
    std::uint32_t(__fastcall* clearUpgrade)(void* self, void* edx);                   // 0x006E1890
    std::uint32_t(__fastcall* activateOne)(void* self, void* edx, std::uint32_t flag);// 0x006E0450 ret 4
    std::uint32_t(__fastcall* weaponIdle)(void* self, void* edx);                     // 0x00688810
    std::uint32_t(__stdcall* fireAt)(void* obj, void* pos);                           // 0x00739E60 ret 8
    std::uint32_t(__fastcall* xferDeploy)(void* self, void* edx, void* xfer);         // 0x007DAE90 ret 4
    std::uint32_t(__fastcall* xferAbility)(void* self, void* edx, void* xfer);        // 0x007321F0 ret 4
    std::uint32_t(__fastcall* xferAi)(void* self, void* edx, void* xfer);             // 0x0072E810 ret 4
    std::uint32_t(__fastcall* xferBody)(void* self, void* edx, void* xfer);           // 0x00722750 ret 4
    std::uint32_t(__cdecl* xferField)(void* xfer, void* field);                       // 0x0071D020
    std::uint32_t(__cdecl* xferPair)(void* xfer, void* field);                        // 0x0054A090
    std::uint32_t(__cdecl* xferRadarTail)(void* xfer, void* field);                   // 0x006F3160
};

void setObject72Calls(const Object72Calls& calls) noexcept;

// Dword at 0x00BE700C, loaded by the move-command bodies.
constexpr std::uint32_t kLocomotorLimitBits = 0x7F7FFFFFu;
// Dword at 0x00BD90A4 (1.0f).
constexpr std::uint32_t kUnitFloatBits = 0x3F800000u;
// Semantic-mutation site: AIUpdateInterface::aim state pushed at 0x0072D777.
constexpr std::uint32_t kAimState2 = 2;

// AIUpdateInterface vtable 0x00C22118. Slot 2 returns AIUpdateInterface.
std::uint32_t AIUpdateInterface_moveWithVector(std::uint8_t* self, std::uint32_t pos,
                                               std::uint32_t vec, std::uint32_t bits,
                                               std::uint32_t mode) noexcept;          // 0x0072CFD0 slot 22
std::uint32_t AIUpdateInterface_moveToObject(std::uint8_t* self, std::uint32_t target,
                                             std::uint32_t mode) noexcept;            // 0x0072D1A0 slot 25
std::uint32_t AIUpdateInterface_moveClear1C(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept;             // 0x0072D260 slot 29
std::uint32_t AIUpdateInterface_moveClear1D(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept;             // 0x0072D310 slot 30
std::uint32_t AIUpdateInterface_moveState1E(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept;             // 0x0072D3C0 slot 31
std::uint32_t AIUpdateInterface_moveState35(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept;             // 0x0072D460 slot 32
std::uint32_t AIUpdateInterface_faceObject(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept;              // 0x0072D500 slot 34
std::uint32_t AIUpdateInterface_command2F(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept;               // 0x0072D590 slot 100
std::uint32_t AIUpdateInterface_aimState3(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept;               // 0x0072D620 slot 37
std::uint32_t AIUpdateInterface_aimState5(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept;               // 0x0072D6D0 slot 39
std::uint32_t AIUpdateInterface_aimState2(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept;               // 0x0072D740 slot 38
std::uint32_t AIUpdateInterface_aimState4(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept;               // 0x0072D7A0 slot 40
std::uint32_t AIUpdateInterface_order2C(std::uint8_t* self, std::uint32_t target,
                                        std::uint32_t mode) noexcept;                 // 0x0072DDB0 slot 46
std::uint32_t AIUpdateInterface_order0B(std::uint8_t* self, std::uint32_t target,
                                        std::uint32_t aux, std::uint32_t mode) noexcept; // 0x0072DE40 slot 49
std::uint32_t AIUpdateInterface_order16(std::uint8_t* self, std::uint32_t key,
                                        std::uint32_t aux, std::uint32_t mode) noexcept; // 0x0072DEC0 slot 50
std::uint32_t AIUpdateInterface_aimFlagged(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t aux, std::uint32_t flag,
                                           std::uint32_t mode) noexcept;              // 0x0072E560 slot 54
std::uint32_t AIUpdateInterface_guardPoint(std::uint8_t* self, std::uint32_t point,
                                           std::uint32_t mode) noexcept;              // 0x0072E600 slot 86
std::uint32_t AIUpdateInterface_attach60(std::uint8_t* self, std::uint32_t arg,
                                         std::uint32_t mode) noexcept;                // 0x0072E680 slot 55

// StancesBehavior 0x00C11840. The body is also installed on secondary tables;
// counted once under this class.
std::uint32_t StancesBehavior_addStanceId(std::uint8_t* self, std::uint32_t id) noexcept; // 0x00722660 slot 22
std::uint32_t StancesBehavior_linkStance(std::uint8_t* self, std::uint32_t id) noexcept;  // 0x007226C0 slot 29

// WeaponFireSpecialAbilityUpdate 0x00C1E918. The same bodies sit in
// DelayedWeaponFireSpecialAbilityUpdate 0x00C202F0; counted once.
std::uint32_t WeaponFireSpecialAbilityUpdate_templateBit8(std::uint8_t* self) noexcept;   // 0x0073F5C0 slot 23
std::uint32_t WeaponFireSpecialAbilityUpdate_armAndTrigger(std::uint8_t* self) noexcept;  // 0x0073F590 slot 26

std::uint32_t ToggleMountedSpecialAbilityUpdate_clearMountedBit(std::uint8_t* self,
                                                                std::uint32_t a,
                                                                std::uint32_t b) noexcept; // 0x0072A8D0 slot 36
std::uint32_t BeamSpecialAbilityUpdate_resetBeam(std::uint8_t* self, std::uint32_t a,
                                                 std::uint32_t b) noexcept;               // 0x007289D0 slot 36
std::uint32_t ActivateModuleSpecialAbilityUpdate_xferAndActivate(std::uint8_t* self,
                                                                 std::uint32_t a,
                                                                 std::uint32_t b) noexcept; // 0x007287E0 slot 36
std::uint32_t GiveUpgradeUpdate_clearThenXfer(std::uint8_t* self, std::uint32_t a,
                                              std::uint32_t b) noexcept;                   // 0x007295C0 slot 36
std::uint32_t OCLMonitorSpecialAbilityUpdate_stopThenXfer(std::uint8_t* self, std::uint32_t a,
                                                          std::uint32_t b) noexcept;       // 0x00730D90 slot 36

std::uint32_t ShieldSphereManager_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;   // 0x00731900 slot 3
std::uint32_t RadarScanMapSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073EE80 slot 3
std::uint32_t CaptureSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073E780 slot 3
std::uint32_t CompositeStructureInfoModule_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x007284C0 slot 3
std::uint32_t StoreObjectsSpecialPower_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0072A580 slot 3
std::uint32_t GiveUpgradeUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;      // 0x0073ED30 slot 3
std::uint32_t DeployStyleAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;    // 0x0073FAA0 slot 3
std::uint32_t HordeAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;          // 0x0073FFF0 slot 3
std::uint32_t ManagedActiveBody_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;      // 0x00722970 slot 3
std::uint32_t SpawnCrateUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;       // 0x00731B70 slot 3
std::uint32_t BroadcastStealthUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0072FA80 slot 3
std::uint32_t StrafeAreaTargetChooser_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;// 0x007216E0 slot 3
std::uint32_t SubStructureStorageBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x00721820 slot 3

std::uint32_t FireWeaponWhenDamagedBehavior_onDamage(std::uint8_t* self,
                                                     std::uint8_t* damage) noexcept;       // 0x0073B200 slot 25

}  // namespace ra3::gamelogic
