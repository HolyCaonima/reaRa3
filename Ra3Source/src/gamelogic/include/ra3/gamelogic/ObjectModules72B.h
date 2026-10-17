#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Stand-ins for image globals the bodies read. The checker and the Unicorn
// oracle start them at the on-disk zeros and update the name-key flags the
// same way the instructions do.
struct Object72BEnv {
    std::uint32_t nameFlag9960 = 0;  // [0x00DF9960]
    std::uint32_t nameCache995c = 0; // [0x00DF995C]
    std::uint32_t nameFlag9968 = 0;  // [0x00DF9968]
    std::uint32_t nameCache9964 = 0; // [0x00DF9964]
    std::uint32_t mapRoot = 0;       // [0x00DEBA64]
    std::uint32_t scriptEngine = 0;  // [0x00DFC058]
    std::uint32_t nameRegistry = 0;  // [0x00E1A078]
    std::uint32_t player = 0;        // [0x00DFBD74]
    std::uint32_t laserGlobal = 0;   // [0x00DF890C]
    std::uint32_t scanGlobal = 0;    // [0x00DF60A0]
};

// Direct callees. Checker stubs and Unicorn bindings share one side-effect log.
struct Object72BCalls {
    std::uint32_t(__fastcall* usable)(void* self, void* edx);
    std::uint32_t(__cdecl* nearCheck)(void* point, void* object, void* local);       // 0x0063EB60
    std::uint32_t(__fastcall* steer)(void* self, void* edx, void* pos, std::uint32_t bits,
                                     std::uint32_t mode);                            // 0x006B67A0 ret 12
    std::uint32_t(__fastcall* place)(void* self, void* edx, void* pos, std::uint32_t bits); // 0x005AC4F0 ret 8
    std::uint32_t(__fastcall* followCf)(void* self, void* edx, void* pos);           // 0x0072CF30 ret 4
    std::uint32_t(__fastcall* followCe)(void* self, void* edx, void* pos);           // 0x0072CE90 ret 4
    std::uint32_t(__fastcall* followCd)(void* self, void* edx, std::uint32_t arg);   // 0x0072CD10 ret 4
    std::uint32_t(__fastcall* bindPath)(void* self, void* edx, void* path);          // 0x005BDCC0 ret 4
    std::uint32_t(__fastcall* findWeapon)(void* self, void* edx, std::uint32_t id);  // 0x006A1770 ret 4
    std::uint32_t(__fastcall* findWeaponAlt)(void* self, void* edx, std::uint32_t found,
                                             std::uint32_t id);                      // 0x006A1720 ret 8
    std::uint32_t(__fastcall* markObject)(void* self, void* edx, std::uint32_t arg); // 0x006C0F40 ret 4
    std::uint32_t(__fastcall* nameKey)(void* self, void* edx, std::uint32_t text);   // 0x00986150 ret 4
    std::uint32_t(__fastcall* findModule)(void* self, void* edx, std::uint32_t key); // 0x00687460 ret 4
    std::uint32_t(__fastcall* pokeModule)(void* self, void* edx);                    // 0x006D7810
    std::uint32_t(__fastcall* disguiseSlot)(void* self, void* edx, std::uint32_t zero); // 0x00729D50 ret 4
    std::uint32_t(__fastcall* visitHeld)(void* self, void* edx, std::uint32_t id);   // 0x00681030 ret 4
    std::uint32_t(__fastcall* queryLoco)(void* self, void* edx);                     // 0x005B9F30
    std::uint32_t(__fastcall* currentTarget)(void* self, void* edx);                 // 0x006EFE40
    std::uint32_t(__fastcall* notePose)(void* self, void* edx, void* object);        // 0x00686B60 ret 4
    std::uint32_t(__fastcall* scriptCall)(void* self, void* edx, std::uint32_t obj,
                                          std::uint32_t zero);                       // 0x00844460 ret 8
    std::uint32_t(__fastcall* encloseTest)(void* self, void* edx, void* objectPlus); // 0x006D3870 ret 4
    std::uint32_t(__fastcall* applyKey)(void* self, void* edx, std::uint32_t id);    // 0x004D9AB0 ret 4
    std::uint32_t(__fastcall* xferBase)(void* self, void* edx, std::uint32_t a,
                                        std::uint32_t b);                            // 0x00718B80 ret 8
    std::uint32_t(__fastcall* lockMap)(void* self, void* edx, std::uint32_t token);  // 0x00A21BE0 ret 4
    std::uint32_t(__fastcall* findMap)(void* self, void* edx, std::uint32_t* out,
                                       std::uint32_t* key);                          // 0x0050B5F0 ret 8
    std::uint32_t(__fastcall* unlockMap)(void* self, void* edx, std::uint32_t flag); // 0x00A21C90 ret 4
    std::uint32_t(__fastcall* floatCall)(void* self, void* edx, std::uint32_t bits,
                                         std::uint32_t id);                          // 0x006B3660 ret 8
    std::uint32_t(__fastcall* ready)(void* self, void* edx);                         // 0x00731C20
    std::uint32_t(__fastcall* xferAbility)(void* self, void* edx, void* xfer);       // 0x007321F0 ret 4
    std::uint32_t(__fastcall* xferRoot)(void* self, void* edx, void* xfer);          // 0x008D82F0 ret 4
    std::uint32_t(__fastcall* xferAt10)(void* self, void* edx, void* xfer);          // 0x00685080 ret 4
    std::uint32_t(__fastcall* xferStealth)(void* self, void* edx, void* xfer);       // 0x006858E0 ret 4
    std::uint32_t(__fastcall* xferAi)(void* self, void* edx, void* xfer);            // 0x0072E810 ret 4
    std::uint32_t(__fastcall* xferMissile)(void* self, void* edx, void* xfer);       // 0x0071FAB0 ret 4
    std::uint32_t(__cdecl* xferField)(void* xfer, void* field);                      // 0x0071D020
    std::uint32_t(__cdecl* xferFieldB)(void* xfer, void* field);                     // 0x0071D140
    std::uint32_t(__cdecl* xferPair)(void* xfer, void* field);                       // 0x0054A090
    std::uint32_t(__cdecl* xferPairB)(void* xfer, void* field);                      // 0x00549D30
    std::uint32_t(__cdecl* xferExtra)(void* xfer, void* field);                      // 0x00708050
    std::uint32_t(__cdecl* xferString)(void* xfer, void* field);                     // 0x00980820
    std::uint32_t(__cdecl* xferList)(void* xfer, void* field);                       // 0x006D38E0
    std::uint32_t(__fastcall* allocBlock)(void* edxUnused, void* edx, std::uint32_t size); // unused
    std::uint32_t(__cdecl* allocBytes)(std::uint32_t size);                          // 0x00416820
    std::uint32_t(__fastcall* ctorList)(void* self, void* edx);                      // 0x007B6840
    std::uint32_t(__fastcall* pushItem)(void* self, void* edx, std::uint32_t value); // 0x007DC280 ret 4
    std::uint32_t(__fastcall* setList)(void* self, void* edx, void* list);           // 0x007DB000 ret 4
    std::uint32_t(__cdecl* freePtr)(void* p);                                        // 0x004166B0
    std::uint32_t(__fastcall* weaponPair)(void* self, void* edx, std::uint32_t a,
                                          std::uint32_t b);                          // 0x006BB310 ret 8
    std::uint32_t(__fastcall* testPair)(void* self, void* edx, void* out, void* obj);// 0x006A88F0 ret 8
    std::uint32_t(__fastcall* floatAdjust)(void* self, void* edx, std::uint32_t zero); // 0x00981070 ret 4
    std::uint32_t(__fastcall* laserApply)(void* self, void* edx, std::uint32_t a, void* b,
                                          void* c);                                  // 0x00717D50 ret 12
    std::uint32_t(__fastcall* stringCtor)(void* self, void* edx);                    // 0x00417070
    std::uint32_t(__fastcall* clearList)(void* self, void* edx);                     // 0x008835F0
    std::uint32_t(__fastcall* beginRecords)(void* self, void* edx);                  // 0x00710520
    std::uint32_t(__fastcall* applyRecord)(void* self, void* edx, std::uint32_t id); // 0x007282C0 ret 4
    std::uint32_t(__fastcall* trigger)(void* self, void* edx, std::uint32_t flag);   // 0x00731E00 ret 4
    std::uint32_t(__fastcall* allocNode)(void* self, void* edx, std::uint32_t size,
                                         std::uint32_t flags);                       // 0x004D99E0 ret 8
    std::uint32_t(__fastcall* linkNode)(void* self, void* edx);                      // 0x008758C0
    std::uint32_t(__cdecl* postList)(void* a, std::uint32_t n, void* b);             // 0x00879050
    std::uint32_t(__fastcall* wipeList)(void* self, void* edx);                      // 0x00550290
    std::uint32_t(__fastcall* disguiseFlag)(void* self, void* edx, std::uint32_t flag); // 0x00712390 ret 4
    std::uint32_t(__fastcall* moveVec)(void* self, void* edx, std::uint32_t a, std::uint32_t b,
                                       std::uint32_t bits, std::uint32_t mode);      // 0x0072CFD0 ret 16
    std::uint32_t(__cdecl* memMove)(void* dst, void* src, std::uint32_t n);          // [0x00BBA500]
    std::uint32_t(__fastcall* scanPulse)(void* self, void* edx, void* node, std::uint32_t bits,
                                         std::uint32_t mask);                        // 0x0098DF80 ret 12
    std::uint32_t(__fastcall* resize16)(void* self, void* edx, std::uint32_t n);     // 0x0071DBB0 ret 4
    std::uint32_t(__fastcall* projectPassengers)(void* self, void* edx, void* vec, void* local,
                                                 void* matrix, std::uint32_t extra,
                                                 std::uint32_t zero);               // 0x00716020 ret 20
    std::uint32_t(__fastcall* touchPassenger)(void* self, void* edx, std::uint32_t flag); // 0x007016B0 ret 4
    std::uint32_t(__fastcall* markPassenger)(void* self, void* edx);                 // 0x006A4FB0
};

struct Object72BEnv;

void setObject72BCalls(const Object72BCalls& calls) noexcept;
void setObject72BEnv(Object72BEnv* env) noexcept;

// Dword at 0x00BE700C.
constexpr std::uint32_t kLocomotorLimitBits = 0x7F7FFFFFu;
// Dword at 0x00BD90A4 (1.0f).
constexpr std::uint32_t kUnitFloatBits = 0x3F800000u;
// Dword at 0x00BD94EC (100.0f), the snap distance in 0x0072D090.
constexpr std::uint32_t kSnapLimitBits = 0x42C80000u;
// State id pushed at 0x0072DD38. Semantic-mutation site.
constexpr std::uint32_t kOrderState0A = 0x0Au;
// x87 control word the oracle installs before each call. fnstcw reads it.
constexpr std::uint32_t kEntryCw = 0x037Fu;

// AIUpdateInterface vtable 0x00C22118.
std::uint32_t AIUpdateInterface_snapNear(std::uint8_t* self, void* point, std::uint32_t gate,
                                         std::uint32_t mode) noexcept;                 // 0x0072D090 slot 27
std::uint32_t AIUpdateInterface_followPathFlag(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                               std::uint32_t mode, std::uint32_t flag) noexcept; // 0x0072D9C0 slot 41
std::uint32_t AIUpdateInterface_followPath34(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                             std::uint32_t bits, std::uint32_t mode) noexcept; // 0x0072DAB0 slot 42
std::uint32_t AIUpdateInterface_followPath3B(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                             std::uint32_t bits, std::uint32_t mode) noexcept; // 0x0072DBA0 slot 43
std::uint32_t AIUpdateInterface_orderWeapon0A(std::uint8_t* self, std::uint32_t target, std::uint32_t aux,
                                              std::uint32_t mode) noexcept;            // 0x0072DC90 slot 45
std::uint32_t AIUpdateInterface_buildList16(std::uint8_t* self, std::uint32_t begin, std::uint32_t end,
                                            std::uint32_t unused2, std::uint32_t unused3, std::uint32_t aux,
                                            std::uint32_t mode) noexcept;             // 0x0072DF50 slot 51
std::uint32_t AIUpdateInterface_steerFloat21(std::uint8_t* self, void* point, std::uint32_t bits,
                                             std::uint32_t aux, std::uint32_t mode) noexcept; // 0x0072E3D0 slot 24
std::uint32_t AIUpdateInterface_placeTarget21(std::uint8_t* self, std::uint8_t* target,
                                              std::uint32_t mode) noexcept;           // 0x0072E490 slot 53
std::uint32_t AIUpdateInterface_attackOrState26(std::uint8_t* self, std::uint32_t target,
                                                std::uint32_t mode) noexcept;         // 0x0072E720 slot 71

std::uint32_t EncloseBehavior_tryApplyKey(std::uint8_t* self, std::uint32_t arg) noexcept; // 0x007340F0 slot 25
std::uint32_t CaptureSpecialAbilityUpdate_releaseHeld(std::uint8_t* self, std::uint32_t a,
                                                      std::uint32_t b) noexcept;     // 0x00728F80 slot 36
std::uint32_t SpecialDisguiseUpdate_blendScale(std::uint8_t* self) noexcept;         // 0x0073EFA0 slot 23
std::uint32_t ToggleMountedSpecialAbilityUpdate_blendMounted(std::uint8_t* self) noexcept; // 0x0073F430 slot 23
std::uint32_t HordeAIUpdate_forwardMove(std::uint8_t* self, std::uint32_t a, std::uint32_t b,
                                        std::uint32_t bits, std::uint32_t mode) noexcept; // 0x0073FE20 slot 22
std::uint32_t CompositeStructureInfoModule_applyRecords(std::uint8_t* self) noexcept; // 0x00728500 slot 1
std::uint32_t CaptureSpecialAbilityUpdate_onTrigger(std::uint8_t* self, std::uint32_t flag) noexcept; // 0x0073E6E0 slot 26

std::uint32_t NotifyWhenAttackingBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x00720380 slot 3
std::uint32_t PassiveAreaEffectBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x00720DF0 slot 3
std::uint32_t ManagerBody_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;      // 0x00722AB0 slot 3
std::uint32_t CrateCollide_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;     // 0x00722C00 slot 3
std::uint32_t SquishCollide_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;    // 0x007232C0 slot 3
std::uint32_t CircularDamageUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0072FB90 slot 3
std::uint32_t InvalidTargetsDeathUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x007305C0 slot 3
std::uint32_t LinearDamageUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x00730750 slot 3
std::uint32_t MissileUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;    // 0x00730980 slot 3
std::uint32_t BeamSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073E380 slot 3
std::uint32_t CloneStoredObjectsSpecialPowerUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073EA60 slot 3
std::uint32_t EjectPassengersSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073EB10 slot 3
std::uint32_t SpecialDisguiseUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F060 slot 3
std::uint32_t ToggleHiddenSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F3F0 slot 3
std::uint32_t ToggleMountedSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F500 slot 3
std::uint32_t ToggleStatusSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F550 slot 3
std::uint32_t WeaponFireSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F600 slot 3
std::uint32_t AnimalAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;   // 0x0073F6D0 slot 3
std::uint32_t AssaultTransportAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x0073F8A0 slot 3
std::uint32_t LaserStateModule_fillOutput(std::uint8_t* self, std::uint8_t* out) noexcept; // 0x00720200 slot 23
std::uint32_t SpawnBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;    // 0x007212B0 slot 3
std::uint32_t WeaponScatterEffectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept; // 0x007331A0 slot 3
std::uint32_t RadarScanMapSpecialAbilityUpdate_clearScan(std::uint8_t* self, std::uint32_t a,
                                                         std::uint32_t b) noexcept;  // 0x00729980 slot 36
std::uint32_t TransportContain_refreshPassengers(std::uint8_t* self) noexcept;       // 0x007280F0 slot 42

}  // namespace ra3::gamelogic
