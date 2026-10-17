#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Shipped callees for the 0x00700000–0x0071FFFF module cluster. The checker and
// the offline oracle install the same side effects. Pointers are thiscall
// (__fastcall with an unused edx) unless the comment says cdecl.
struct Object70Calls {
    std::uint32_t(__cdecl* freePtr)(std::uint32_t p);                              // 0x004166B0
    std::uint32_t(__cdecl* allocNode)(std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t); // 0x00416860
    std::uint32_t(__fastcall* itemDtor)(void*, void*);                             // 0x004DA760
    std::uint32_t(__fastcall* playerHas)(void*, void*, std::uint32_t);             // 0x004E2140 ret 4
    std::uint32_t(__fastcall* releasePair)(void*, void*, std::uint32_t, std::uint32_t); // 0x004E41B0 ret 8
    std::uint32_t(__fastcall* releaseOne)(void*, void*);                           // 0x004ED380
    std::uint32_t(__fastcall* playerCount)(void*, void*, std::uint32_t, std::uint32_t); // 0x004F6850 ret 8
    std::uint32_t(__fastcall* seedDisguise)(void*, void*, std::uint32_t, std::uint32_t,
                                            std::uint32_t, std::uint32_t);         // 0x00506C20 ret 0x10
    std::uint32_t(__fastcall* finishPower)(void*, void*, std::uint32_t, std::uint32_t); // 0x0052EFC0 ret 8
    std::uint32_t(__fastcall* powerReady)(void*, void*);                           // 0x00538530
    std::uint32_t(__cdecl* xferBlock)(std::uint32_t, std::uint32_t);               // 0x00539DF0
    std::uint32_t(__cdecl* projectCoord)(std::uint32_t);                           // 0x0063ED00
    std::uint32_t(__fastcall* xferNode)(void*, void*, std::uint32_t);              // 0x00683E70 ret 4
    std::uint32_t(__fastcall* xferAt10)(void*, void*, std::uint32_t);              // 0x00685080 ret 4
    std::uint32_t(__fastcall* findModule)(void*, void*, std::uint32_t);            // 0x00687460 ret 4
    std::uint32_t(__fastcall* bridgeCount)(void*, void*);                          // 0x0068B180
    std::uint32_t(__fastcall* liftPrepare)(void*, void*);                          // 0x0069D000
    std::uint32_t(__fastcall* upgradeCheck)(void*, void*, std::uint32_t, std::uint32_t); // 0x0069FA90 ret 8
    std::uint32_t(__fastcall* upgradeApply)(void*, void*, std::uint32_t, std::uint32_t); // 0x006FD820 ret 8
    std::uint32_t(__fastcall* effectOn)(void*, void*);                             // 0x006A56A0
    std::uint32_t(__fastcall* ammoNotify)(void*, void*);                           // 0x006A8400
    std::uint32_t(__fastcall* enterState)(void*, void*, std::uint32_t);            // 0x006BCF30 ret 4
    std::uint32_t(__fastcall* giveTemplate)(void*, void*, std::uint32_t, std::uint32_t); // 0x006C1C50 ret 8
    std::uint32_t(__fastcall* recallScan)(void*, void*, std::uint32_t, std::uint32_t); // 0x007ED750 ret 8
    std::uint32_t(__fastcall* stancePulse)(void*, void*);                          // 0x006C8560
    std::uint32_t(__fastcall* recallTest)(void*, void*, std::uint32_t, std::uint32_t,
                                          std::uint32_t);                          // 0x006CACE0 ret 0xC
    std::uint32_t(__fastcall* setStatus)(void*, void*, std::uint32_t, std::uint32_t); // 0x006D1ED0 ret 8
    std::uint32_t(__fastcall* stanceFlag)(void*, void*, std::uint32_t);            // 0x006D23F0 ret 4
    std::uint32_t(__fastcall* proneRefresh)(void*, void*);                         // 0x006E96E0
    std::uint32_t(__fastcall* oclForward)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t,
                                          std::uint32_t, std::uint32_t, std::uint32_t); // 0x006EA540 ret 0x18
    std::uint32_t(__fastcall* pointTest)(void*, void*, std::uint32_t, std::uint32_t,
                                         std::uint32_t);                           // 0x006EA9D0 ret 0xC
    std::uint32_t(__fastcall* subRelay)(void*, void*, std::uint32_t);              // 0x006ECE70 ret 4
    std::uint32_t(__fastcall* fireWeapon)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t,
                                          std::uint32_t, std::uint32_t);           // 0x006EE8F0 ret 0x14
    std::uint32_t(__fastcall* setPosition)(void*, void*, std::uint32_t, std::uint32_t); // 0x006F0910 ret 8
    std::uint32_t(__fastcall* clearSlot)(void*, void*, std::uint32_t);             // 0x006F0A20 ret 4
    std::uint32_t(__fastcall* weaponQuery)(void*, void*);                          // 0x006F2050
    std::uint32_t(__fastcall* weaponLink)(void*, void*, std::uint32_t, std::uint32_t); // 0x006F2730 ret 8
    std::uint32_t(__fastcall* vectorGrow)(void*, void*, std::uint32_t, std::uint32_t); // 0x006F3E20 ret 8
    std::uint32_t(__fastcall* liftAttach)(void*, void*, std::uint32_t, std::uint32_t); // 0x006F42D0 ret 8
    std::uint32_t(__fastcall* stancePush)(void*, void*, std::uint32_t, std::uint32_t); // 0x006F8B80 ret 8
    std::uint32_t(__fastcall* hordeBase)(void*, void*);                            // 0x006FBBD0
    std::uint32_t(__fastcall* specialReady)(void*, void*);                         // 0x00702070
    std::uint32_t(__fastcall* doorTest)(void*, void*, std::uint32_t);              // 0x00707050 ret 4
    std::uint32_t(__cdecl* xferTail30)(std::uint32_t, std::uint32_t);              // 0x00708190
    std::uint32_t(__cdecl* xferTail2c)(std::uint32_t, std::uint32_t);              // 0x007087D0
    std::uint32_t(__fastcall* stopEffect)(void*, void*);                           // 0x00700E50
    std::uint32_t(__fastcall* oclFinish)(void*, void*);                            // 0x007010D0
    std::uint32_t(__fastcall* clearDamage)(void*, void*);                          // 0x00701330
    std::uint32_t(__fastcall* sphereProbe)(void*, void*);                          // 0x007174C0
    std::uint32_t(__fastcall* sphereAlt)(void*, void*);                            // 0x007176B0
    std::uint32_t(__fastcall* sphereMain)(void*, void*);                           // 0x00717B40
    std::uint32_t(__cdecl* assignField)(std::uint32_t, std::uint32_t);             // 0x0071D020
    std::uint32_t(__fastcall* bezierTail)(void*, void*, std::uint32_t, std::uint32_t); // 0x0071F0F0 ret 8
    std::uint32_t(__fastcall* push8)(void*, void*, std::uint32_t, std::uint32_t);  // 0x007F17C0 ret 8
    std::uint32_t(__fastcall* doorAlt)(void*, void*, std::uint32_t);               // 0x007F3120 ret 4
    std::uint32_t(__fastcall* doorScan)(void*, void*, std::uint32_t);              // 0x0078DA40 ret 4
    std::uint32_t(__fastcall* ghostFind)(void*, void*, std::uint32_t);             // 0x0071ABE0 ret 4
    std::uint32_t(__cdecl* xferPair)(std::uint32_t, std::uint32_t);                // 0x0054A090
    std::uint32_t(__fastcall* bridgePlace)(void*, void*, std::uint32_t);           // 0x00860630 ret 4
    std::uint32_t(__fastcall* xferBase)(void*, void*, std::uint32_t);              // 0x008D82F0 ret 4
    std::uint32_t(__cdecl* moveRange)(std::uint32_t, std::uint32_t, std::uint32_t); // 0x009279F0
    std::uint32_t(__fastcall* nameKey)(void*, void*, std::uint32_t);               // 0x00986150 ret 4
    std::uint32_t(__cdecl* moveBytes)(std::uint32_t, std::uint32_t, std::uint32_t); // import [0x00BBA500]
};

// Globals the shipped bodies load by absolute address. The checker publishes the
// same values the oracle writes into the image before each call.
struct Object70Env {
    std::uint32_t ghostRoot;      // [0x00DF8918]
    std::uint32_t namekeyReady;   // dword at 0x00DF98F4 (tested as a byte, ored as a dword)
    std::uint32_t namekeyValue;   // [0x00DF98F0]
    std::uint32_t namekeyThis;    // [0x00E1A078]
    std::uint32_t bridgeWorld;    // [0x00DFC190]
    std::uint32_t recallBits;     // f32 at 0x00BE700C
    std::uint32_t stanceFloorBits;// f32 at 0x00BF6060
    std::uint32_t prodLimitBits;  // f32 at 0x00BD94EC
    std::uint32_t player;         // [0x00DEBA5C]
};

void setObject70(const Object70Env& env, const Object70Calls& calls) noexcept;
void object70Env(Object70Env& out) noexcept;

// BeamSpecialAbilityUpdate secondary 0x00C1B918 slot 2. ret 4. Writes 0x46 into *arg.
std::uint32_t BeamSpecialAbilityUpdate_setMask46(std::uint32_t arg) noexcept;
// 0x00C1B928 slots 7 and 16. Null-checked interior pointers.
std::uint32_t BeamSpecialAbilityUpdate_adjustB4(std::uint8_t* self) noexcept;
std::uint32_t BeamSpecialAbilityUpdate_adjustB0(std::uint8_t* self) noexcept;
// 0x00C1B8D4 slot 2. ret 4.
std::uint32_t BeamSpecialAbilityUpdate_sameOwner(std::uint8_t* self, std::uint32_t other) noexcept;

// OCLMonitorSpecialAbilityUpdate 0x00C1EE08 slot 42. Partial EAX keeps the high bytes of [self+0xB8].
std::uint32_t OCLMonitorSpecialAbilityUpdate_spanOpen(std::uint8_t* self) noexcept;
// Secondary 0x00C1ECA4 slot 0. ret 0x18.
std::uint32_t OCLMonitorSpecialAbilityUpdate_forward(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                     std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                                                     std::uint32_t a5) noexcept;

// WeaponFireSpecialAbilityUpdate secondary 0x00C1E7A0 slot 2. ret 4.
std::uint32_t WeaponFireSpecialAbilityUpdate_sameOwner(std::uint8_t* self, std::uint32_t other) noexcept;

// LinearDamageUpdate secondary 0x00C0D538 slots 7 and 5. ret 4.
std::uint32_t LinearDamageUpdate_relayIfArmed(std::uint8_t* self, std::uint32_t arg) noexcept;
std::uint32_t LinearDamageUpdate_clearIfUnset(std::uint8_t* self, std::uint32_t bit) noexcept;

// SubStructureBuildUpgrade secondary 0x00C10754 slot 2. ret 4.
std::uint32_t SubStructureBuildUpgrade_maskIf(std::uint8_t* self, std::uint32_t arg) noexcept;
// SubObjectsUpgrade secondary 0x00C1F388 slot 2. ret 4.
std::uint32_t SubObjectsUpgrade_relay(std::uint8_t* self, std::uint32_t arg) noexcept;

// SpecialAbilityUpdate secondary 0x00C14184 slot 7. ret 8. Returns 1 when the point test is false.
std::uint32_t SpecialAbilityUpdate_pointInside(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1) noexcept;

// StoreTemplateIDBehavior secondary 0x00C099E8 slot 0. ret 4. Vector at this+4.
std::uint32_t StoreTemplateIDBehavior_pushId(std::uint8_t* self, std::uint32_t id) noexcept;

// StancesBehavior 0x00C11840 slot 33. Clears the 16-byte record span at +0xF0/+0xF4.
std::uint32_t StancesBehavior_clearRecords(std::uint8_t* self) noexcept;
// Slot 58. ret 8. SSE accumulate into +4, clamped by +0xC and the floor at 0x00BF6060.
std::uint32_t StancesBehavior_accumulate(std::uint8_t* self, std::uint32_t deltaBits, std::uint32_t source) noexcept;

// SwitchLocomotorsSpecialAbilityUpdate 0x00C1D960 slot 22.
std::uint32_t SwitchLocomotorsSpecialAbilityUpdate_latch(std::uint8_t* self) noexcept;

// BehaviorModule secondary 0x00BF8DA8 slot 2. ret 4.
std::uint32_t BehaviorModule_clearMotion(std::uint8_t* self, std::uint32_t arg) noexcept;

// WeaponReadyState 0x00BF9708 slot 4.
std::uint32_t WeaponReadyState_poll(std::uint8_t* self) noexcept;
// WeaponFireState 0x00BF97D8 slot 6. ret 4.
std::uint32_t WeaponFireState_enter(std::uint8_t* self, std::uint32_t arg) noexcept;
// WeaponOutOfAmmoState 0x00BF98A0 slot 4.
std::uint32_t WeaponOutOfAmmoState_enter(std::uint8_t* self) noexcept;

// ProductionUpdate secondary 0x00C19580 slots 1 and 13.
std::uint32_t ProductionUpdate_doorCode(std::uint8_t* self, std::uint32_t arg) noexcept;
std::uint32_t ProductionUpdate_scanOrPush(std::uint8_t* self, std::uint32_t record, std::uint32_t flag) noexcept;

// BoobyTrapUpdate 0x00C20660 slot 25. ret 4.
std::uint32_t BoobyTrapUpdate_capture(std::uint8_t* self, std::uint32_t arg) noexcept;

// MagneticEffectUpdate secondary 0x00C193AC slot 1. ret 4.
std::uint32_t MagneticEffectUpdate_onBit(std::uint8_t* self, std::uint32_t bit) noexcept;
// MagneticEffectUpdate 0x00C19510 slot 3. ret 4.
std::uint32_t MagneticEffectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// HordeContain 0x00C21A98 slot 1.
std::uint32_t HordeContain_refresh(std::uint8_t* self) noexcept;
// HordeContain secondary 0x00C21520 slot 10. ret 4.
std::uint32_t HordeContain_collectIds(std::uint8_t* self, std::uint8_t* out) noexcept;

// GhostObject secondary 0x00C1F574 slot 1. Links a node from [0x00DF8918]+0x14.
std::uint32_t GhostObject_linkWatcher(std::uint8_t* self) noexcept;

// SpecialDisguiseUpdate secondary 0x00C1D5F8 slot 2. ret 4. Returns the argument pointer.
std::uint32_t SpecialDisguiseUpdate_seed(std::uint8_t* self, std::uint32_t arg) noexcept;

// Object secondary 0x00C27530 slot 2.
std::uint32_t Object_maskedFind(std::uint8_t* self) noexcept;

// RecallUnitsSpecialPower secondary 0x00C183F0 slot 20. ret 0xC.
std::uint32_t RecallUnitsSpecialPower_filter(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                             std::uint32_t a2) noexcept;

// GiveOrRestoreUpgradeSpecialPower secondary 0x00C1C874 slot 6. ret 8.
std::uint32_t GiveOrRestoreUpgradeSpecialPower_try(std::uint8_t* self, std::uint32_t target,
                                                   std::uint32_t bits) noexcept;

// BridgeBuilderBehavior 0x00C1AAB8 slot 3. ret 4.
std::uint32_t BridgeBuilderBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
// EncloseBehavior 0x00C08A48 slot 3. ret 4.
std::uint32_t EncloseBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
// ShieldSphereUpdate 0x00C14120 slot 3. ret 4.
std::uint32_t ShieldSphereUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
// BombardAreaUpdate 0x00C0CAD0 slot 3. ret 4.
std::uint32_t BombardAreaUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
// BeamTargetModule 0x00C08480 slot 3. ret 4.
std::uint32_t BeamTargetModule_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
// LiftObjectUpdate 0x00C0D338 slot 3. ret 4.
std::uint32_t LiftObjectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// EmotionTrackerUpdate secondary 0x00C13A70 slot 0.
std::uint32_t EmotionTrackerUpdate_release(std::uint8_t* self) noexcept;
// NotifyWhenAttackingBehavior secondary 0x00C08C94 slot 0. ret 4.
std::uint32_t NotifyWhenAttackingBehavior_clearIds(std::uint8_t* self, std::uint32_t arg) noexcept;

// SpecialPowerModule 0x00C01108 slot 23. ret 4.
std::uint32_t SpecialPowerModule_trigger(std::uint8_t* self, std::uint8_t* object) noexcept;
// AIUpdateInterface 0x00C22118 slot 79. ret 8. "ProneUpdate" name key at 0x00C053B8.
std::uint32_t AIUpdateInterface_applyProne(std::uint8_t* self, std::uint8_t* source) noexcept;

// BezierProjectileBehavior secondary 0x00C10F20 slot 7. ret 8.
std::uint32_t BezierProjectileBehavior_copyAim(std::uint8_t* self, std::uint32_t src, std::uint32_t extra) noexcept;
// ShieldSphereManager secondary 0x00C05CB4 slot 5.
std::uint32_t ShieldSphereManager_retarget(std::uint8_t* self) noexcept;

}  // namespace ra3::gamelogic
