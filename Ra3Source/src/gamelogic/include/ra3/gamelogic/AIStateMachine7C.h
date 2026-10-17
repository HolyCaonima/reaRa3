#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// Memory view for the 0x007C0000-0x007CFFFF AIStateMachine & AI State hierarchy
// cluster (aistates.cpp, aiguard.cpp, aiguardretaliate.cpp).
struct AIStateMachine7CMemoryView {
    std::uint32_t theGameLogic = 0;      // [0x00DEBA64] TheGameLogic
    std::uint32_t theWeaponStore = 0;    // [0x00DEBA88] subsystem table
    std::uint32_t thePartitionMgr = 0;   // [0x00DFB9D8] target search manager
    float halfScale = 0.5f;              // [0x00BD9528] 0.5f (0x3F000000)
    float minMoveDistance = 0.1f;        // [0x00BD9520] 0.1f (0x3DCCCCCD)
    float defaultGoalRadius = 3.402823466e+38f; // [0x00BE700C] FLT_MAX (0x7F7FFFFF)
};

// Dispatch table for external engine helpers called by the 0x007C AI State
// and AIStateMachine methods.
struct GameDispatch {
    // 0x00416820: void* __cdecl operatorNew(size_t size)
    void* (*operatorNew_00416820)(std::size_t size, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005B9F30: void* __thiscall StateMachine::getGoalObject(void* machine)
    void* (*getGoalObject_005B9F30)(void* machinePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00683630: uint32_t __thiscall AIUpdateInterface::getTargetObjectID(void* aiUpdate)
    std::uint32_t (*getAIUpdateTargetId_00683630)(
        void* aiUpdatePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00981070: float __thiscall GeometryInfo::getMaxHeightAbovePosition(void* geom, int arg)
    float (*queryHeight_00981070)(
        void* geomPtr, std::int32_t arg, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C08A0: uint32_t __thiscall AIInternalMoveToState::startMove(void* state)
    std::uint32_t (*startMove_007C08A0)(
        void* statePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006FFB00: bool __thiscall AIUpdateInterface::checkMoveOut(void* aiUpdate, int arg)
    bool (*checkMoveOut_006FFB00)(
        void* aiUpdatePtr, std::int32_t arg, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006A4F80: void __thiscall Object::setStatusBit(void* owner, int bitIndex, int val)
    void (*setStatusHelper_006A4F80)(
        void* ownerPtr, std::int32_t bitIndex, std::int32_t val,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006A4FB0: void __thiscall Object::postStatusChange(void* owner)
    void (*postStatusHelper_006A4FB0)(
        void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00686E00: void __thiscall Object::notifySubHelper(void* owner)
    void (*ownerHelper_00686E00)(void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007526C0: void __thiscall AIUpdateInterface::computeRepulsorDest(void* aiUpdate, void* ioPos, void* refPos)
    void (*repulsorHelper_007526C0)(
        void* aiUpdatePtr, void* ioPos, void* refPos,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007527C0: bool __thiscall AIUpdateInterface::setLocomotorGoalMode(void* aiUpdate, int mode)
    bool (*aiUpdateMode_007527C0)(
        void* aiUpdatePtr, std::int32_t mode, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x004D9AF0: void __thiscall Object::setModelConditionState(void* owner, int bit)
    void (*ownerAction_004D9AF0)(
        void* ownerPtr, std::int32_t bit, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00752720: void __thiscall AIUpdateInterface::setTargetObjectID(void* aiUpdate, uint32_t objId)
    void (*aiUpdateSetTargetId_00752720)(
        void* aiUpdatePtr, std::uint32_t objId, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006A6510: float __thiscall Object::getVisionRange(void* owner)
    float (*queryOwnerRange_006A6510)(
        void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00798BE0: void* __thiscall PartitionManager::findTargetInRange(void* mgr, void* owner, float range)
    void* (*findTargetInRange_00798BE0)(
        void* mgrPtr, void* ownerPtr, float range, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007B1E90: void* __thiscall SubMachine0E80::ctor(void* mem, void* owner, void* goalObj, uint32_t tag)
    void* (*ctor_007B1E90)(
        void* memPtr, void* ownerPtr, void* goalObj, std::uint32_t tag,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007B12D0: uint32_t __thiscall AIDelayedMoveToState::fallbackMove(void* state)
    std::uint32_t (*delayedMoveFallback_007B12D0)(
        void* statePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005AA240: void __thiscall StateMachine::lock(void* machine, uint32_t tagAddr)
    void (*lockMachine_005AA240)(
        void* machinePtr, std::uint32_t tagAddr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C3740: uint32_t __thiscall AIInternalMoveBase::onEnter(void* state)
    std::uint32_t (*baseMoveOnEnter_007C3740)(
        void* statePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BD4D0: void* __thiscall GuardSubMachine::ctor(void* mem, void* owner)
    void* (*ctor_007BD4D0)(
        void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00687810: void* __thiscall Object::findGuardTargetModule(void* goalObj)
    void* (*checkGuardTarget_00687810)(
        void* goalObjPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BE020: void* __thiscall GuardRetaliateSubMachine::ctor(void* mem, void* owner)
    void* (*ctor_007BE020)(
        void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005AC4F0: void __thiscall StateMachine::setGoalPosition(void* machine, const void* pos, float radius)
    void (*setGoalPosition_005AC4F0)(
        void* machinePtr, const void* posPtr, float radius,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BDF30: void* __thiscall AreaGuardSubMachine::ctor(void* mem, void* owner)
    void* (*ctor_007BDF30)(
        void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C2CA0: uint32_t __thiscall AIFollowPathState::onEnter(void* state)
    std::uint32_t (*followPathOnEnter_007C2CA0)(
        void* statePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BA5E0: void* __thiscall SubMachine6B80::ctor(void* mem, int a1, int a2)
    void* (*ctor_007BA5E0)(
        void* memPtr, std::int32_t a1, std::int32_t a2,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BADC0: void* __thiscall SubMachine6C10A::ctor(void* mem, int a1, int a2)
    void* (*ctor_007BADC0)(
        void* memPtr, std::int32_t a1, std::int32_t a2,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BACA0: void* __thiscall SubMachine6C10B::ctor(void* mem, int a1, int a2)
    void* (*ctor_007BACA0)(
        void* memPtr, std::int32_t a1, std::int32_t a2,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BB180: void* __thiscall SubMachine6C80A::ctor(void* mem, int a1)
    void* (*ctor_007BB180)(
        void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007BB070: void* __thiscall SubMachine6C80B::ctor(void* mem, int a1)
    void* (*ctor_007BB070)(
        void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x0050B5F0: void* __thiscall GameLogicObjectMap::findObject(void* logic, uint32_t objId)
    void* (*findObjectById_0050B5F0)(
        void* gameLogicPtr, std::uint32_t objId, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x004DF7C0: void* __thiscall SubsystemTable::lookupEntry(void* table, uint32_t id, int kind)
    void* (*lookupEntry_004DF7C0)(
        void* tablePtr, std::uint32_t id, std::int32_t kind,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x00A78070: void __thiscall AIStateMachine::notifyProducerDefaultState(void* machine, int arg)
    void (*notifyDefaultState_00A78070)(
        void* machinePtr, std::int32_t arg, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005B25C0: uint32_t __thiscall StateMachine::initDefaultState(void* machine)
    std::uint32_t (*baseInitDefaultState_005B25C0)(
        void* machinePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006C0F40: void __thiscall Object::cleanupTargetRef(void* owner, void* targetObj)
    void (*ownerTargetCleanup_006C0F40)(
        void* ownerPtr, void* targetObjPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006A7620: void* __thiscall Object::getContainModuleFor(void* containerObj, void* passenger, int arg)
    void* (*getContainModule_006A7620)(
        void* containerObjPtr, void* passengerPtr, std::int32_t arg,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x004DE030: uint32_t __thiscall GameLogic::resolveObjectId(void* gameLogic, uint32_t rawKey)
    std::uint32_t (*resolveObjectId_004DE030)(
        void* gameLogicPtr, std::uint32_t rawKey, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x0069A030: void* __thiscall AIUpdateInterface::queryWeaponSubObject(void* aiUpdate)
    void* (*queryAIUpdateSub_0069A030)(
        void* aiUpdatePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007ADAB0: void __thiscall WeaponSubObject::recompute(void* sub)
    void (*recomputeSub_007ADAB0)(void* subPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C9070: void* __thiscall SubMachine9150::ctor(void* mem, void* owner)
    void* (*ctor_007C9070)(
        void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C91E0: void* __thiscall SubMachine9280::ctor(void* mem, void* owner)
    void* (*ctor_007C91E0)(
        void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C8780: void* __thiscall SubMachineCC8A0::ctor(void* mem, int a1, int a2)
    void* (*ctor_007C8780)(
        void* memPtr, std::int32_t a1, std::int32_t a2,
        AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007C8E40: void* __thiscall SubMachineCC8D0::ctor(void* mem, int a1)
    void* (*ctor_007C8E40)(
        void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005AC4C0: void __thiscall StateMachine::setGoalObject(void* machine, void* goalObj)
    void (*setGoalObject_005AC4C0)(
        void* machinePtr, void* goalObjPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005AC420: uint32_t __thiscall StateMachine::updateStateMachine(void* machine)
    std::uint32_t (*baseUpdateStateMachine_005AC420)(
        void* machinePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006838B0: void __thiscall OwnerComponent324::onTemporaryStateExit(void* comp)
    void (*notifyTempExit_006838B0)(void* compPtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005C0840: void __thiscall StateMachine::clear(void* machine)
    void (*baseClear_005C0840)(void* machinePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005C08C0: uint32_t __thiscall StateMachine::resetToDefaultState(void* machine)
    std::uint32_t (*baseResetToDefaultState_005C08C0)(
        void* machinePtr, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x005B6230: uint32_t __thiscall StateMachine::setState(void* machine, uint32_t newStateId)
    std::uint32_t (*baseSetState_005B6230)(
        void* machinePtr, std::uint32_t newStateId, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x007B1190: void __thiscall State::onExit(void* state, int exitType)
    void (*baseStateOnExit_007B1190)(
        void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006E6360: void __thiscall AIUpdateInterface::clearEnterStateFlag(void* aiUpdate, int arg)
    void (*aiUpdateHelper_006E6360)(
        void* aiUpdatePtr, std::int32_t arg, AIStateMachine7CMemoryView& view) = nullptr;

    // 0x006A7600: void* __thiscall Object::getOpenContainModule(void* obj)
    void* (*getOpenContainModule_006A7600)(
        void* objPtr, AIStateMachine7CMemoryView& view) = nullptr;
};

const GameDispatch& noOpDispatch() noexcept;

// --- 45 Reconstructed Functions in 0x007C0AD0..0x007CE500 ---

// 1. 0x007C0AD0 - AIInternalMoveToState::onEnter (vt 0x00C2F388[4])
std::uint32_t internalMoveToStateOnEnter_007C0AD0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 2. 0x007C0BD0 - AIMoveOutOfTheWayState::onEnter (vt 0x00C2F3E8[4])
std::uint32_t moveOutOfTheWayStateOnEnter_007C0BD0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 3. 0x007C0C80 - AIMoveAndTightenState::onEnter (vt 0x00C2F448[4])
std::uint32_t moveAndTightenStateOnEnter_007C0C80(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 4. 0x007C0D00 - AIMoveAwayFromRepulsorsState::onEnter (vt 0x00C2F4B0[4])
std::uint32_t moveAwayFromRepulsorsStateOnEnter_007C0D00(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 5. 0x007C0D70 - AIAttackApproachTargetState::onEnter (vt 0x00C2F518[4])
std::uint32_t attackApproachTargetStateOnEnter_007C0D70(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 6. 0x007C0DF0 - AIAttackPursueTargetState::onEnter (vt 0x00C2F580[4])
std::uint32_t attackPursueTargetStateOnEnter_007C0DF0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 7. 0x007C0E80 - AIStateSubMachine0E80::onEnter (vt 0x00C2C638[4])
std::uint32_t subMachineStateOnEnter_007C0E80(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 8. 0x007C0EE0 - AIMoveToTargetObjectState0EE0::onEnter (vt 0x00C2F650[4])
std::uint32_t moveToTargetObjectStateOnEnter_007C0EE0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 9. 0x007C2C80 - AIDelayedMoveToState2C80::update (vt 0x00C2FAA8[5])
std::uint32_t delayedMoveToStateUpdate_007C2C80(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 10. 0x007C3520 - AIMoveAndEvacuateState::onEnter (vt 0x00C2FBE0[4])
std::uint32_t moveAndEvacuateStateOnEnter_007C3520(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 11. 0x007C3680 - AIMoveWithStatusFlagsState3680::onEnter (vt 0x00C2FC48[4])
std::uint32_t moveWithStatusFlagsStateOnEnter_007C3680(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 12. 0x007C39C0 - AIInternalMoveWrapperState39C0::onEnter (vt 0x00C324D0[4])
std::uint32_t internalMoveWrapperStateOnEnter_007C39C0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 13. 0x007C3B50 - AISubMachineResetMoveState3B50::onEnter (vt 0x00C321E8[4])
std::uint32_t subMachineResetMoveStateOnEnter_007C3B50(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 14. 0x007C42E0 - AIGuardState::xfer (vt 0x00C2CDE8[3])
void guardStateXfer_007C42E0(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 15. 0x007C43A0 - AIGuardState::onEnter (vt 0x00C2CDE8[4])
std::uint32_t guardStateOnEnter_007C43A0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 16. 0x007C4450 - AIGuardRetaliateState::xfer (vt 0x00C2CE40[3])
void guardRetaliateStateXfer_007C4450(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 17. 0x007C4510 - AIGuardRetaliateState::onEnter (vt 0x00C2CE40[4])
std::uint32_t guardRetaliateStateOnEnter_007C4510(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 18. 0x007C4820 - AIAreaGuardSubMachineState4820::xfer (vt 0x00C2D1C8[3])
void areaGuardSubMachineStateXfer_007C4820(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 19. 0x007C48C0 - AIAreaGuardSubMachineState4820::onEnter (vt 0x00C2D1C8[4])
std::uint32_t areaGuardSubMachineStateOnEnter_007C48C0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 20. 0x007C4BE0 - AIMoveValidatedState4BE0::onEnter (vt 0x00C2FFB8[4])
std::uint32_t moveValidatedStateOnEnter_007C4BE0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 21. 0x007C4D30 - AIMoveIfBeyondDistanceState4D30::onEnter (vt 0x00C30228[4])
std::uint32_t moveIfBeyondDistanceStateOnEnter_007C4D30(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 22. 0x007C4DC0 - AIStopAndFollowPathState4DC0::onEnter (vt 0x00C30028[4])
std::uint32_t stopAndFollowPathStateOnEnter_007C4DC0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 23. 0x007C4DE0 - AIStopAndInternalMoveState4DE0::onEnter (vt 0x00C30310[4])
std::uint32_t stopAndInternalMoveStateOnEnter_007C4DE0(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 24. 0x007C6B80 - AIStateMachineDerived6B80::createSubMachine19 (vt 0x00C326D0[19])
void* createSubMachine19_007C6B80(
    void* machinePtr, std::int32_t a1, std::int32_t a2, std::int32_t a3,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 25. 0x007C6BB0 - AIStateMachineDerived6B80::createSubMachine20 (vt 0x00C326D0[20])
void* createSubMachine20_007C6BB0(
    void* machinePtr, std::int32_t a1, std::int32_t a2,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 26. 0x007C6BE0 - AIStateMachineDerived6B80::createSubMachine21 (vt 0x00C326D0[21])
void* createSubMachine21_007C6BE0(
    void* machinePtr, std::int32_t a1,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 27. 0x007C6C10 - AIStateMachineDerived6C10::createSubMachine19 (vt 0x00C303D0[19])
void* createSubMachine19_007C6C10(
    void* machinePtr, std::int32_t a1, std::int32_t a2, const void* a3,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 28. 0x007C6C80 - AIStateMachineDerived6C10::createSubMachine20 (vt 0x00C303D0[20])
void* createSubMachine20_007C6C80(
    void* machinePtr, std::int32_t a1, const void* a2,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 29. 0x007C6E90 - AIStateMachineDerived6E90::initDefaultState (vt 0x00C30428[5])
std::uint32_t initDefaultStateWithProducerCheck_007C6E90(
    void* machinePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 30. 0x007C86E0 - AIStateWithTargetObject86E0::onExit (vt 0x00C30EE0[6])
void stateWithTargetObjectOnExit_007C86E0(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 31. 0x007C8C90 - AIExitInstantlyState::onEnter (vt 0x00C2AF38[4])
std::uint32_t exitInstantlyStateOnEnter_007C8C90(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 32. 0x007C8D40 - AIResolveGoalTargetState8D40::onEnter (vt 0x00C2B0D8[4])
std::uint32_t resolveGoalTargetStateOnEnter_007C8D40(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 33. 0x007C8DE0 - AIStateQueryWeaponClip8DE0::queryOffset (vt 0x00C2E8C8[20])
std::uint32_t* queryWeaponClipOffset_007C8DE0(
    void* statePtr, std::uint32_t* outPair, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 34. 0x007C9150 - AIStateFactory9150::createSubMachine (vt 0x00C2EA28[19])
void* stateCreateSubMachine_007C9150(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 35. 0x007C9280 - AIStateFactory9280::createSubMachine (vt 0x00C2EB68[19])
void* stateCreateSubMachine_007C9280(
    void* statePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 36. 0x007CC8A0 - AIStateMachine::createSubMachine19 (vt 0x00C2C4C8[19])
void* aiStateMachineCreateSubMachine19_007CC8A0(
    void* machinePtr, std::int32_t a1, std::int32_t a2, std::int32_t a3,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 37. 0x007CC8D0 - AIStateMachine::createSubMachine20 (vt 0x00C2C4C8[20])
void* aiStateMachineCreateSubMachine20_007CC8D0(
    void* machinePtr, std::int32_t a1, std::int32_t a2,
    AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 38. 0x007CC920 - AIStateMachine::updateStateMachine (vt 0x00C2C4C8[4])
std::uint32_t aiStateMachineUpdate_007CC920(
    void* machinePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 39. 0x007CCA30 - AIStateMachine::exitTemporaryState (internal helper)
void aiStateMachineExitTemporaryState_007CCA30(
    void* machinePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 40. 0x007CCB00 - AIStateMachine::clear (vt 0x00C2C4C8[13])
void aiStateMachineClear_007CCB00(
    void* machinePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 41. 0x007CCB70 - AIStateMachine::resetToDefaultState (vt 0x00C2C4C8[6])
std::uint32_t aiStateMachineResetToDefaultState_007CCB70(
    void* machinePtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 42. 0x007CCBC0 - AIStateMachine::setState (vt 0x00C2C4C8[7])
std::uint32_t aiStateMachineSetState_007CCBC0(
    void* machinePtr, std::uint32_t newStateId, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 43. 0x007CE280 - AIEnterState::onExit (vt 0x00C2FE00[6])
void enterStateOnExit_007CE280(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 44. 0x007CE460 - AIExitState::onExit (vt 0x00C2CF60[6])
void exitStateOnExit_007CE460(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

// 45. 0x007CE500 - AIExitInstantlyState::onExit (vt 0x00C2D8E8[6])
void exitInstantlyStateOnExit_007CE500(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch = noOpDispatch()) noexcept;

}  // namespace ra3::gamelogic
