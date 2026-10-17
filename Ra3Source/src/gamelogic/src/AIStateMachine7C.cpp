#include <ra3/gamelogic/AIStateMachine7C.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

inline std::uint8_t* bytesOf(void* p) noexcept
{
    return static_cast<std::uint8_t*>(p);
}

inline const std::uint8_t* bytesOf(const void* p) noexcept
{
    return static_cast<const std::uint8_t*>(p);
}

inline std::uint32_t readU32(const void* base, std::ptrdiff_t offset) noexcept
{
    std::uint32_t v = 0;
    std::memcpy(&v, bytesOf(base) + offset, sizeof(v));
    return v;
}

inline void writeU32(void* base, std::ptrdiff_t offset, std::uint32_t v) noexcept
{
    std::memcpy(bytesOf(base) + offset, &v, sizeof(v));
}

inline std::uint8_t readU8(const void* base, std::ptrdiff_t offset) noexcept
{
    return *(bytesOf(base) + offset);
}

inline void writeU8(void* base, std::ptrdiff_t offset, std::uint8_t v) noexcept
{
    *(bytesOf(base) + offset) = v;
}

inline float readF32(const void* base, std::ptrdiff_t offset) noexcept
{
    float v = 0.0f;
    std::memcpy(&v, bytesOf(base) + offset, sizeof(v));
    return v;
}

inline void writeF32(void* base, std::ptrdiff_t offset, float v) noexcept
{
    std::memcpy(bytesOf(base) + offset, &v, sizeof(v));
}

inline void* readPtr(const void* base, std::ptrdiff_t offset) noexcept
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(readU32(base, offset)));
}

inline void writePtr(void* base, std::ptrdiff_t offset, const void* ptr) noexcept
{
    writeU32(base, offset, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ptr)));
}

inline void* addrToPtr(std::uint32_t addr) noexcept
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr));
}

inline void copyCoord3D(void* dstBase, std::ptrdiff_t dstOff, const void* srcBase,
                        std::ptrdiff_t srcOff) noexcept
{
    writeU32(dstBase, dstOff + 0x00, readU32(srcBase, srcOff + 0x00));
    writeU32(dstBase, dstOff + 0x04, readU32(srcBase, srcOff + 0x04));
    writeU32(dstBase, dstOff + 0x08, readU32(srcBase, srcOff + 0x08));
}

// Virtual dispatch helpers on 32-bit x86 objects
template <typename Ret>
inline Ret callVtable0(void* obj, std::size_t slotOffset) noexcept
{
    const std::uint32_t vt = readU32(obj, 0);
    const std::uint32_t fnAddr = readU32(addrToPtr(vt), static_cast<std::ptrdiff_t>(slotOffset));
    using Fn = Ret(__thiscall*)(void*);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(fnAddr))(obj);
}

template <typename Ret, typename A0>
inline Ret callVtable1(void* obj, std::size_t slotOffset, A0 a0) noexcept
{
    const std::uint32_t vt = readU32(obj, 0);
    const std::uint32_t fnAddr = readU32(addrToPtr(vt), static_cast<std::ptrdiff_t>(slotOffset));
    using Fn = Ret(__thiscall*)(void*, A0);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(fnAddr))(obj, a0);
}

template <typename Ret, typename A0, typename A1>
inline Ret callVtable2(void* obj, std::size_t slotOffset, A0 a0, A1 a1) noexcept
{
    const std::uint32_t vt = readU32(obj, 0);
    const std::uint32_t fnAddr = readU32(addrToPtr(vt), static_cast<std::ptrdiff_t>(slotOffset));
    using Fn = Ret(__thiscall*)(void*, A0, A1);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(fnAddr))(obj, a0, a1);
}

// Default no-op implementations for GameDispatch
void* noOpOperatorNew(std::size_t, AIStateMachine7CMemoryView&) { return nullptr; }
void* noOpGetGoalObject(void* machinePtr, AIStateMachine7CMemoryView& view)
{
    if (machinePtr == nullptr) {
        return nullptr;
    }
    const std::uint32_t mode = readU32(machinePtr, 0x6C);
    if (mode == 1u) {
        const std::uint32_t goalId = readU32(machinePtr, 0x28);
        if (goalId == 0 || view.theGameLogic == 0) {
            return nullptr;
        }
        return readPtr(addrToPtr(view.theGameLogic), 0xC4);
    }
    return nullptr;
}
std::uint32_t noOpGetTargetId(void* aiUpdatePtr, AIStateMachine7CMemoryView&)
{
    return aiUpdatePtr != nullptr ? readU32(aiUpdatePtr, 0x184) : 0u;
}
float noOpQueryHeight(void*, std::int32_t, AIStateMachine7CMemoryView&) { return 0.0f; }
std::uint32_t noOpStartMove(void*, AIStateMachine7CMemoryView&) { return 0u; }
bool noOpCheckMoveOut(void*, std::int32_t, AIStateMachine7CMemoryView&) { return false; }
void noOpSetStatusHelper(void*, std::int32_t, std::int32_t, AIStateMachine7CMemoryView&) {}
void noOpPostStatusHelper(void*, AIStateMachine7CMemoryView&) {}
void noOpOwnerHelper(void*, AIStateMachine7CMemoryView&) {}
void noOpRepulsorHelper(void*, void*, void*, AIStateMachine7CMemoryView&) {}
bool noOpAiUpdateMode(void*, std::int32_t, AIStateMachine7CMemoryView&) { return true; }
void noOpOwnerAction(void*, std::int32_t, AIStateMachine7CMemoryView&) {}
void noOpAiUpdateSetTargetId(void*, std::uint32_t, AIStateMachine7CMemoryView&) {}
float noOpQueryOwnerRange(void*, AIStateMachine7CMemoryView&) { return 0.0f; }
void* noOpFindTargetInRange(void*, void*, float, AIStateMachine7CMemoryView&) { return nullptr; }
void* noOpCtor0E80(void* memPtr, void*, void*, std::uint32_t, AIStateMachine7CMemoryView&)
{
    return memPtr;
}
std::uint32_t noOpDelayedFallback(void*, AIStateMachine7CMemoryView&) { return 0u; }
void noOpLockMachine(void* machinePtr, std::uint32_t, AIStateMachine7CMemoryView&)
{
    if (machinePtr != nullptr) {
        writeU8(machinePtr, 0x78, 1u);
    }
}
std::uint32_t noOpBaseMoveOnEnter(void*, AIStateMachine7CMemoryView&) { return 0u; }
void* noOpCtorPtrOwner(void* memPtr, void*, AIStateMachine7CMemoryView&) { return memPtr; }
void* noOpCheckGuardTarget(void*, AIStateMachine7CMemoryView&) { return nullptr; }
void noOpSetGoalPosition(void* machinePtr, const void* posPtr, float radius,
                         AIStateMachine7CMemoryView&)
{
    if (machinePtr == nullptr || readU8(machinePtr, 0x78) != 0) {
        return;
    }
    if (posPtr != nullptr) {
        copyCoord3D(machinePtr, 0x2C, posPtr, 0x00);
        copyCoord3D(machinePtr, 0x48, posPtr, 0x00);
        writeU8(machinePtr, 0x44, 0u);
    }
    writeF32(machinePtr, 0x54, radius);
}
std::uint32_t noOpFollowPathOnEnter(void*, AIStateMachine7CMemoryView&) { return 0u; }
void* noOpCtor2Int(void* memPtr, std::int32_t, std::int32_t, AIStateMachine7CMemoryView&)
{
    return memPtr;
}
void* noOpCtor1Int(void* memPtr, std::int32_t, AIStateMachine7CMemoryView&) { return memPtr; }
void* noOpFindObjectById(void*, std::uint32_t, AIStateMachine7CMemoryView&) { return nullptr; }
void* noOpLookupEntry(void*, std::uint32_t, std::int32_t, AIStateMachine7CMemoryView&)
{
    return nullptr;
}
void noOpNotifyDefaultState(void*, std::int32_t, AIStateMachine7CMemoryView&) {}
std::uint32_t noOpBaseInitDefault(void*, AIStateMachine7CMemoryView&) { return 0u; }
void noOpOwnerTargetCleanup(void*, void*, AIStateMachine7CMemoryView&) {}
void* noOpGetContainModule(void*, void*, std::int32_t, AIStateMachine7CMemoryView&)
{
    return nullptr;
}
std::uint32_t noOpResolveObjectId(void*, std::uint32_t, AIStateMachine7CMemoryView&) { return 0u; }
void* noOpQueryAIUpdateSub(void*, AIStateMachine7CMemoryView&) { return nullptr; }
void noOpRecomputeSub(void*, AIStateMachine7CMemoryView&) {}
void noOpSetGoalObject(void* machinePtr, void* goalObjPtr, AIStateMachine7CMemoryView&)
{
    if (machinePtr == nullptr || readU8(machinePtr, 0x78) != 0) {
        return;
    }
    writeU32(machinePtr, 0x28, goalObjPtr != nullptr ? readU32(goalObjPtr, 0xDC) : 0u);
}
std::uint32_t noOpBaseUpdateMachine(void*, AIStateMachine7CMemoryView&) { return 0u; }
void noOpNotifyTempExit(void*, AIStateMachine7CMemoryView&) {}
void noOpBaseClear(void*, AIStateMachine7CMemoryView&) {}
std::uint32_t noOpBaseResetDefault(void*, AIStateMachine7CMemoryView&) { return 0u; }
std::uint32_t noOpBaseSetState(void*, std::uint32_t, AIStateMachine7CMemoryView&) { return 0u; }
void noOpBaseStateOnExit(void*, std::int32_t, AIStateMachine7CMemoryView&) {}
void noOpAiUpdateHelper(void*, std::int32_t, AIStateMachine7CMemoryView&) {}
void* noOpGetOpenContainModule(void*, AIStateMachine7CMemoryView&) { return nullptr; }

}  // namespace

const GameDispatch& noOpDispatch() noexcept
{
    static const GameDispatch kNoOp = [] {
        GameDispatch d{};
        d.operatorNew_00416820 = &noOpOperatorNew;
        d.getGoalObject_005B9F30 = &noOpGetGoalObject;
        d.getAIUpdateTargetId_00683630 = &noOpGetTargetId;
        d.queryHeight_00981070 = &noOpQueryHeight;
        d.startMove_007C08A0 = &noOpStartMove;
        d.checkMoveOut_006FFB00 = &noOpCheckMoveOut;
        d.setStatusHelper_006A4F80 = &noOpSetStatusHelper;
        d.postStatusHelper_006A4FB0 = &noOpPostStatusHelper;
        d.ownerHelper_00686E00 = &noOpOwnerHelper;
        d.repulsorHelper_007526C0 = &noOpRepulsorHelper;
        d.aiUpdateMode_007527C0 = &noOpAiUpdateMode;
        d.ownerAction_004D9AF0 = &noOpOwnerAction;
        d.aiUpdateSetTargetId_00752720 = &noOpAiUpdateSetTargetId;
        d.queryOwnerRange_006A6510 = &noOpQueryOwnerRange;
        d.findTargetInRange_00798BE0 = &noOpFindTargetInRange;
        d.ctor_007B1E90 = &noOpCtor0E80;
        d.delayedMoveFallback_007B12D0 = &noOpDelayedFallback;
        d.lockMachine_005AA240 = &noOpLockMachine;
        d.baseMoveOnEnter_007C3740 = &noOpBaseMoveOnEnter;
        d.ctor_007BD4D0 = &noOpCtorPtrOwner;
        d.checkGuardTarget_00687810 = &noOpCheckGuardTarget;
        d.ctor_007BE020 = &noOpCtorPtrOwner;
        d.setGoalPosition_005AC4F0 = &noOpSetGoalPosition;
        d.ctor_007BDF30 = &noOpCtorPtrOwner;
        d.followPathOnEnter_007C2CA0 = &noOpFollowPathOnEnter;
        d.ctor_007BA5E0 = &noOpCtor2Int;
        d.ctor_007BADC0 = &noOpCtor2Int;
        d.ctor_007BACA0 = &noOpCtor2Int;
        d.ctor_007BB180 = &noOpCtor1Int;
        d.ctor_007BB070 = &noOpCtor1Int;
        d.findObjectById_0050B5F0 = &noOpFindObjectById;
        d.lookupEntry_004DF7C0 = &noOpLookupEntry;
        d.notifyDefaultState_00A78070 = &noOpNotifyDefaultState;
        d.baseInitDefaultState_005B25C0 = &noOpBaseInitDefault;
        d.ownerTargetCleanup_006C0F40 = &noOpOwnerTargetCleanup;
        d.getContainModule_006A7620 = &noOpGetContainModule;
        d.resolveObjectId_004DE030 = &noOpResolveObjectId;
        d.queryAIUpdateSub_0069A030 = &noOpQueryAIUpdateSub;
        d.recomputeSub_007ADAB0 = &noOpRecomputeSub;
        d.ctor_007C9070 = &noOpCtorPtrOwner;
        d.ctor_007C91E0 = &noOpCtorPtrOwner;
        d.ctor_007C8780 = &noOpCtor2Int;
        d.ctor_007C8E40 = &noOpCtor1Int;
        d.setGoalObject_005AC4C0 = &noOpSetGoalObject;
        d.baseUpdateStateMachine_005AC420 = &noOpBaseUpdateMachine;
        d.notifyTempExit_006838B0 = &noOpNotifyTempExit;
        d.baseClear_005C0840 = &noOpBaseClear;
        d.baseResetToDefaultState_005C08C0 = &noOpBaseResetDefault;
        d.baseSetState_005B6230 = &noOpBaseSetState;
        d.baseStateOnExit_007B1190 = &noOpBaseStateOnExit;
        d.aiUpdateHelper_006E6360 = &noOpAiUpdateHelper;
        d.getOpenContainModule_006A7600 = &noOpGetOpenContainModule;
        return d;
    }();
    return kNoOp;
}

// 1. 0x007C0AD0 - AIInternalMoveToState::onEnter
std::uint32_t aiMoveToStateOnEnter_007C0AD0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 1u);
    void* const owner0 = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner0, 0x374);

    void* const goal0 = dispatch.getGoalObject_005B9F30(machine0, view);
    if (goal0 != nullptr && aiUpdate != nullptr) {
        void* const goal1 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        const std::uint32_t goalId = readU32(goal1, 0xDC);
        const std::uint32_t targetId = dispatch.getAIUpdateTargetId_00683630(aiUpdate, view);
        if (goalId == targetId) {
            writeU8(statePtr, 0x4C, 0u);
        }
    }

    void* const goal2 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    if (goal2 != nullptr) {
        void* const goal3 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        copyCoord3D(statePtr, 0x20, goal3, 0x38);

        void* const machine1 = readPtr(statePtr, 0x18);
        void* const owner1 = readPtr(machine1, 0x24);
        void* const thingTemplate = readPtr(owner1, 0x04);
        const std::uint32_t tmplFlags = readU32(thingTemplate, 0xC0);
        if (((tmplFlags >> 26) & 1u) != 0u) {
            void* const goal4 = dispatch.getGoalObject_005B9F30(machine1, view);
            const float h = dispatch.queryHeight_00981070(bytesOf(goal4) + 0x110, 0, view);
            const float halfH =
                static_cast<float>(static_cast<double>(h) * static_cast<double>(view.halfScale));
            const float newZ = static_cast<float>(
                static_cast<double>(halfH) + static_cast<double>(readF32(statePtr, 0x28)));
            writeF32(statePtr, 0x28, newZ);

            void* const goal5 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
            const float goalZ = readF32(goal5, 0x40);
            if (newZ > goalZ) {
                writeF32(statePtr, 0x28, newZ + halfH);
            }
        }
    } else {
        copyCoord3D(statePtr, 0x20, readPtr(statePtr, 0x18), 0x2C);
    }

    return dispatch.startMove_007C08A0(statePtr, view);
}

// 2. 0x007C0BD0 - AIMoveOutOfTheWayState::onEnter
std::uint32_t aiMoveToStateSAOnEnter_007C0BD0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 1u);
    void* const owner0 = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner0, 0x374);

    void* const goal0 = dispatch.getGoalObject_005B9F30(machine0, view);
    if (goal0 != nullptr && aiUpdate != nullptr) {
        void* const goal1 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        const std::uint32_t goalId = readU32(goal1, 0xDC);
        const std::uint32_t targetId = dispatch.getAIUpdateTargetId_00683630(aiUpdate, view);
        if (goalId == targetId) {
            writeU8(statePtr, 0x4C, 0u);
        }
    }

    void* const goal2 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    if (goal2 != nullptr) {
        void* const goal3 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        copyCoord3D(statePtr, 0x20, goal3, 0x38);
    } else {
        copyCoord3D(statePtr, 0x20, readPtr(statePtr, 0x18), 0x2C);
    }

    const std::uint32_t result = dispatch.startMove_007C08A0(statePtr, view);
    if (readU8(statePtr, 0x54) != 0 && aiUpdate != nullptr) {
        if (dispatch.checkMoveOut_006FFB00(aiUpdate, 0, view)) {
            callVtable0<void>(aiUpdate, 0x270);
        }
    }
    return result;
}

// 3. 0x007C0C80 - AIMoveAndTightenState::onEnter
std::uint32_t aiMoveOutOfTheWayStateOnEnter_007C0C80(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 1u);
    void* const owner = readPtr(machine, 0x24);
    const std::uint32_t statusBits = readU32(owner, 0xB0);
    void* const aiUpdate = readPtr(owner, 0x374);

    if (((statusBits >> 22) & 1u) == 0u) {
        writeU32(owner, 0xB0, statusBits | 0x00400000u);
        dispatch.setStatusHelper_006A4F80(owner, 0x96, 1, view);
        dispatch.postStatusHelper_006A4FB0(owner, view);
    }

    void* const subObj = readPtr(aiUpdate, 0x150);
    if (subObj == nullptr) {
        return 0xFFFFFFFEu;
    }

    void* const inner = readPtr(subObj, 0x14);
    copyCoord3D(statePtr, 0x20, inner, 0x0C);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 4. 0x007C0D00 - AIMoveAwayFromRepulsorsState::onEnter
std::uint32_t aiMoveAndTightenStateOnEnter_007C0D00(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 0u);
    void* const owner = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    writeU32(statePtr, 0x50, 1u);
    writeU8(statePtr, 0x54, 1u);
    dispatch.ownerHelper_00686E00(owner, view);

    void* const machine1 = readPtr(statePtr, 0x18);
    copyCoord3D(statePtr, 0x20, machine1, 0x2C);
    void* const statePos = bytesOf(statePtr) + 0x20;
    void* const refPos =
        (readU8(machine1, 0x44) != 0) ? (bytesOf(machine1) + 0x48) : statePos;
    dispatch.repulsorHelper_007526C0(aiUpdate, statePos, refPos, view);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 5. 0x007C0D70 - AIAttackApproachTargetState::onEnter
std::uint32_t aiMoveAwayPanicStateOnEnter_007C0D70(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 0u);
    void* const owner = readPtr(machine0, 0x24);
    void* const goalObj = dispatch.getGoalObject_005B9F30(machine0, view);
    void* const machine1 = readPtr(statePtr, 0x18);
    void* const owner1 = readPtr(machine1, 0x24);
    void* const aiUpdate = readPtr(owner1, 0x374);

    if (goalObj == nullptr || aiUpdate == nullptr || owner == nullptr) {
        return 0xFFFFFFFEu;
    }

    dispatch.aiUpdateMode_007527C0(aiUpdate, 4, view);
    dispatch.ownerAction_004D9AF0(owner, 0xAE, view);
    writeU32(statePtr, 0x50, 1u);
    writeU8(statePtr, 0x54, 1u);
    dispatch.ownerHelper_00686E00(owner, view);
    dispatch.aiUpdateSetTargetId_00752720(aiUpdate, readU32(goalObj, 0xDC), view);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 6. 0x007C0DF0 - AIAttackPursueTargetState::onEnter
std::uint32_t aiMoveAwayFromRepulsorsStateOnEnter_007C0DF0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 0u);
    void* const owner = readPtr(machine0, 0x24);
    const float range = dispatch.queryOwnerRange_006A6510(owner, view);
    void* const goalObj =
        dispatch.findTargetInRange_00798BE0(addrToPtr(view.thePartitionMgr), owner, range, view);

    void* const machine1 = readPtr(statePtr, 0x18);
    void* const owner1 = readPtr(machine1, 0x24);
    void* const aiUpdate = readPtr(owner1, 0x374);

    if (goalObj == nullptr || aiUpdate == nullptr || owner == nullptr) {
        return 0xFFFFFFFEu;
    }

    dispatch.aiUpdateMode_007527C0(aiUpdate, 4, view);
    dispatch.ownerAction_004D9AF0(owner, 0xAE, view);
    writeU32(statePtr, 0x50, 1u);
    writeU8(statePtr, 0x54, 1u);
    dispatch.ownerHelper_00686E00(owner, view);
    dispatch.aiUpdateSetTargetId_00752720(aiUpdate, readU32(goalObj, 0xDC), view);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 7. 0x007C0E80 - AIStateSubMachine0E80::onEnter
std::uint32_t aiBackAwayAndCowerStateOnEnter_007C0E80(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    if (readU32(statePtr, 0x20) != 0) {
        return 0xFFFFFFFEu;
    }

    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    void* subMach = nullptr;
    if (rawMem != nullptr) {
        void* const goalObj = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
        subMach = dispatch.ctor_007B1E90(rawMem, owner, goalObj, 0xDE67B322u, view);
    }

    writePtr(statePtr, 0x20, subMach);
    if (subMach == nullptr) {
        return 0xFFFFFFFEu;
    }
    return callVtable0<std::uint32_t>(subMach, 0x14);
}

// 8. 0x007C0EE0 - AIMoveToTargetObjectState0EE0::onEnter
std::uint32_t aiMoveAwayAndCowerStateOnEnter_007C0EE0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const goalObj = dispatch.getGoalObject_005B9F30(machine0, view);
    void* const machine1 = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine1, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);

    if (goalObj == nullptr || aiUpdate == nullptr) {
        return 0xFFFFFFFEu;
    }

    dispatch.aiUpdateMode_007527C0(aiUpdate, 4, view);
    writeU32(statePtr, 0x50, 1u);
    writeU8(statePtr, 0x54, 1u);
    dispatch.aiUpdateSetTargetId_00752720(aiUpdate, readU32(goalObj, 0xDC), view);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 9. 0x007C2C80 - AIDelayedMoveToState2C80::update
std::uint32_t aiPickUpCrateStateUpdate_007C2C80(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    const std::uint32_t count = readU32(statePtr, 0x50);
    if (count == 0) {
        return dispatch.delayedMoveFallback_007B12D0(statePtr, view);
    }
    const std::uint32_t next = count - 1u;
    writeU32(statePtr, 0x50, next);
    if (next == 0) {
        return dispatch.startMove_007C08A0(statePtr, view);
    }
    return 0u;
}

// 10. 0x007C3520 - AIMoveAndEvacuateState::onEnter
std::uint32_t aiMoveAndEvacuateStateOnEnter_007C3520(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine0, 0x24);
    copyCoord3D(statePtr, 0x50, owner, 0x38);
    writeU8(statePtr, 0x4C, 1u);

    void* const goal0 = dispatch.getGoalObject_005B9F30(machine0, view);
    if (goal0 != nullptr) {
        void* const goal1 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        copyCoord3D(statePtr, 0x20, goal1, 0x38);
    } else {
        copyCoord3D(statePtr, 0x20, readPtr(statePtr, 0x18), 0x2C);
    }
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 11. 0x007C3680 - AIMoveWithStatusFlagsState3680::onEnter
std::uint32_t aiMoveAndDeleteStateOnEnter_007C3680(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 0u);
    dispatch.lockMachine_005AA240(machine0, 0x00C338D8u, view);

    void* const goal0 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    if (goal0 != nullptr) {
        void* const goal1 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        copyCoord3D(statePtr, 0x20, goal1, 0x38);
    } else {
        copyCoord3D(statePtr, 0x20, readPtr(statePtr, 0x18), 0x2C);
    }

    writeU8(statePtr, 0x50, 1u);
    void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
    if (owner != nullptr) {
        const std::uint32_t flags0 = readU32(owner, 0xB0);
        if (((flags0 >> 15) & 1u) == 0u) {
            writeU32(owner, 0xB0, flags0 | 0x00008000u);
            dispatch.setStatusHelper_006A4F80(owner, 0x8F, 1, view);
            dispatch.postStatusHelper_006A4FB0(owner, view);
        }
        const std::uint32_t flags1 = readU32(owner, 0xB0);
        if (((flags1 >> 23) & 1u) == 0u) {
            writeU32(owner, 0xB0, flags1 | 0x00800000u);
            dispatch.setStatusHelper_006A4F80(owner, 0x97, 1, view);
            dispatch.postStatusHelper_006A4FB0(owner, view);
        }
    }
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 12. 0x007C39C0 - AIInternalMoveWrapperState39C0::onEnter
std::uint32_t aiFollowWaypointPathStateAndEvacuateOnEnter_007C39C0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    dispatch.aiUpdateMode_007527C0(aiUpdate, 5, view);
    return dispatch.baseMoveOnEnter_007C3740(statePtr, view);
}

// 13. 0x007C3B50 - AISubMachineResetMoveState3B50::onEnter
std::uint32_t aiAttackFollowWaypointPathStateOnEnter_007C3B50(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const subMach0 = readPtr(statePtr, 0x6C);
    callVtable0<void>(subMach0, 0x34);
    void* const subMach1 = readPtr(statePtr, 0x6C);
    callVtable1<std::uint32_t>(subMach1, 0x1C, 0u);
    return dispatch.baseMoveOnEnter_007C3740(statePtr, view);
}

// 14. 0x007C42E0 - AIGuardState::xfer
void aiDockStateXfer_007C42E0(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    if (((readU32(xferPtr, 0x04) >> 3) & 1u) != 0u) {
        return;
    }

    std::uint8_t ver[2] = {1u, 1u};
    callVtable1<void>(xferPtr, 0x18, static_cast<void*>(ver));

    std::uint8_t hasSub = (readU32(statePtr, 0x20) != 0) ? 1u : 0u;
    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(&hasSub));
    if (hasSub != 0) {
        if (readU32(statePtr, 0x20) == 0) {
            void* const rawMem = dispatch.operatorNew_00416820(0x84, view);
            void* subMach = nullptr;
            if (rawMem != nullptr) {
                void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
                subMach = dispatch.ctor_007BD4D0(rawMem, owner, view);
            }
            writePtr(statePtr, 0x20, subMach);
        }
        if (hasSub != 0) {
            callVtable1<void>(xferPtr, 0x20, readPtr(statePtr, 0x20));
        }
    }

    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(bytesOf(statePtr) + 0x24));
    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(bytesOf(statePtr) + 0x25));
}

// 15. 0x007C43A0 - AIGuardState::onEnter
std::uint32_t aiDockStateOnEnter_007C43A0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const owner0 = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner0, 0x374);
    if (aiUpdate != nullptr) {
        writeU8(statePtr, 0x25, readU8(aiUpdate, 0x434));
        void* const subMod = callVtable0<void*>(aiUpdate, 0x1C4);
        if (subMod != nullptr) {
            const std::uint8_t flag =
                static_cast<std::uint8_t>(callVtable0<std::uint32_t>(subMod, 0x84) & 0xFFu);
            if (flag != 0) {
                writeU8(aiUpdate, 0x434, 1u);
            }
        }
    }

    void* const goalObj = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    if (goalObj == nullptr) {
        return 0xFFFFFFFEu;
    }
    if (dispatch.checkGuardTarget_00687810(goalObj, view) == nullptr) {
        return 0xFFFFFFFEu;
    }

    void* const rawMem = dispatch.operatorNew_00416820(0x84, view);
    void* subMach = nullptr;
    if (rawMem != nullptr) {
        void* const owner1 = readPtr(readPtr(statePtr, 0x18), 0x24);
        subMach = dispatch.ctor_007BD4D0(rawMem, owner1, view);
    }
    writePtr(statePtr, 0x20, subMach);
    callVtable1<void>(subMach, 0x38, goalObj);
    return callVtable0<std::uint32_t>(readPtr(statePtr, 0x20), 0x14);
}

// 16. 0x007C4450 - AIGuardRetaliateState::xfer
void aiHarvestStateXfer_007C4450(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    if (((readU32(xferPtr, 0x04) >> 3) & 1u) != 0u) {
        return;
    }

    std::uint8_t ver[2] = {1u, 1u};
    callVtable1<void>(xferPtr, 0x18, static_cast<void*>(ver));

    std::uint8_t hasSub = (readU32(statePtr, 0x20) != 0) ? 1u : 0u;
    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(&hasSub));
    if (hasSub != 0) {
        if (readU32(statePtr, 0x20) == 0) {
            void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
            void* subMach = nullptr;
            if (rawMem != nullptr) {
                void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
                subMach = dispatch.ctor_007BE020(rawMem, owner, view);
            }
            writePtr(statePtr, 0x20, subMach);
        }
        if (hasSub != 0) {
            callVtable1<void>(xferPtr, 0x20, readPtr(statePtr, 0x20));
        }
    }

    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(bytesOf(statePtr) + 0x24));
    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(bytesOf(statePtr) + 0x25));
}

// 17. 0x007C4510 - AIGuardRetaliateState::onEnter
std::uint32_t aiHarvestStateOnEnter_007C4510(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    void* subMach = nullptr;
    if (rawMem != nullptr) {
        void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
        subMach = dispatch.ctor_007BE020(rawMem, owner, view);
    }

    void* const machine0 = readPtr(statePtr, 0x18);
    writePtr(statePtr, 0x20, subMach);
    dispatch.setGoalPosition_005AC4F0(
        subMach, bytesOf(machine0) + 0x2C, view.defaultGoalRadius, view);

    void* const goalObj = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    callVtable1<void>(readPtr(statePtr, 0x20), 0x38, goalObj);

    void* const owner1 = readPtr(readPtr(statePtr, 0x18), 0x24);
    void* const aiUpdate = readPtr(owner1, 0x374);
    if (aiUpdate != nullptr) {
        writeU8(statePtr, 0x25, readU8(aiUpdate, 0x434));
        void* const subMod = callVtable0<void*>(aiUpdate, 0x1C4);
        if (subMod != nullptr) {
            const std::uint8_t flag =
                static_cast<std::uint8_t>(callVtable0<std::uint32_t>(subMod, 0x84) & 0xFFu);
            if (flag != 0) {
                writeU8(aiUpdate, 0x434, 1u);
            }
        }
    }

    return callVtable0<std::uint32_t>(readPtr(statePtr, 0x20), 0x14);
}

// 18. 0x007C4820 - AIAreaGuardSubMachineState4820::xfer
void aiGuardRetaliateStateXfer_007C4820(
    void* statePtr, void* xferPtr, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    if (((readU32(xferPtr, 0x04) >> 3) & 1u) != 0u) {
        return;
    }

    std::uint8_t ver[2] = {1u, 1u};
    callVtable1<void>(xferPtr, 0x18, static_cast<void*>(ver));

    std::uint8_t hasSub = (readU32(statePtr, 0x20) != 0) ? 1u : 0u;
    callVtable1<void>(xferPtr, 0x84, static_cast<void*>(&hasSub));
    if (hasSub != 0) {
        if (readU32(statePtr, 0x20) == 0) {
            void* const rawMem = dispatch.operatorNew_00416820(0x90, view);
            void* subMach = nullptr;
            if (rawMem != nullptr) {
                void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
                subMach = dispatch.ctor_007BDF30(rawMem, owner, view);
            }
            writePtr(statePtr, 0x20, subMach);
        }
        if (hasSub != 0) {
            callVtable1<void>(xferPtr, 0x20, readPtr(statePtr, 0x20));
        }
    }
}

// 19. 0x007C48C0 - AIAreaGuardSubMachineState4820::onEnter
std::uint32_t aiGuardRetaliateStateOnEnter_007C48C0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const owner0 = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner0, 0x374);

    void* const rawMem = dispatch.operatorNew_00416820(0x90, view);
    void* subMach = nullptr;
    if (rawMem != nullptr) {
        void* const owner1 = readPtr(readPtr(statePtr, 0x18), 0x24);
        subMach = dispatch.ctor_007BDF30(rawMem, owner1, view);
    }
    writePtr(statePtr, 0x20, subMach);

    // 0x00683570: returns [aiUpdate + 0x3C] + 0x2C
    void* const unitMachine = readPtr(aiUpdate, 0x3C);
    copyCoord3D(readPtr(statePtr, 0x20), 0x80, unitMachine, 0x2C);

    // 0x00683550: calls getGoalObject_005B9F30([aiUpdate + 0x3C])
    void* const goalObj = dispatch.getGoalObject_005B9F30(readPtr(aiUpdate, 0x3C), view);
    if (goalObj != nullptr) {
        writeU32(readPtr(statePtr, 0x20), 0x8C, readU32(goalObj, 0xDC));
    }

    return callVtable0<std::uint32_t>(readPtr(statePtr, 0x20), 0x14);
}

// 20. 0x007C4BE0 - AIMoveValidatedState4BE0::onEnter
std::uint32_t aiMoveToPositionAndDieStateOnEnter_007C4BE0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(statePtr, 0x4C, 0u);
    dispatch.lockMachine_005AA240(machine0, 0x00C338D8u, view);

    void* const goal0 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
    if (goal0 != nullptr) {
        void* const goal1 = dispatch.getGoalObject_005B9F30(readPtr(statePtr, 0x18), view);
        copyCoord3D(statePtr, 0x20, goal1, 0x38);
    } else {
        copyCoord3D(statePtr, 0x20, readPtr(statePtr, 0x18), 0x2C);
    }

    writeU8(statePtr, 0x50, 1u);
    return dispatch.startMove_007C08A0(statePtr, view);
}

// 21. 0x007C4D30 - AIMoveIfBeyondDistanceState4D30::onEnter
std::uint32_t aiMoveToAndEvacuateStateOnEnter_007C4D30(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    const float dx = readF32(statePtr, 0x20) - readF32(owner, 0x38);
    const float dy = readF32(statePtr, 0x24) - readF32(owner, 0x3C);
    const float dz = readF32(statePtr, 0x28) - readF32(owner, 0x40);

    void* const aiUpdate = readPtr(owner, 0x374);
    dispatch.aiUpdateMode_007527C0(aiUpdate, 5, view);

    const double distSq = static_cast<double>(dz) * static_cast<double>(dz) +
                          static_cast<double>(dy) * static_cast<double>(dy) +
                          static_cast<double>(dx) * static_cast<double>(dx);
    const double dist = std::sqrt(distSq);
    if (dist > static_cast<double>(view.minMoveDistance)) {
        return aiMoveToStateOnEnter_007C0AD0(statePtr, view, dispatch);
    }
    return 0u;
}

// 22. 0x007C4DC0 - AIStopAndFollowPathState4DC0::onEnter
std::uint32_t aiFollowPathAndEvacuateStateOnEnter_007C4DC0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    dispatch.aiUpdateMode_007527C0(aiUpdate, 5, view);
    return dispatch.followPathOnEnter_007C2CA0(statePtr, view);
}

// 23. 0x007C4DE0 - AIStopAndInternalMoveState4DE0::onEnter
std::uint32_t aiMoveForBoardingOnEnter_007C4DE0(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    dispatch.aiUpdateMode_007527C0(aiUpdate, 5, view);
    return aiMoveToStateOnEnter_007C0AD0(statePtr, view, dispatch);
}

// 24. 0x007C6B80 - AIStateMachineDerived6B80::createSubMachine19
void* createSubMachine19_007C6B80(
    void* /*machinePtr*/, std::int32_t a1, std::int32_t a2, std::int32_t /*a3*/,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007BA5E0(rawMem, a1, a2, view);
}

// 25. 0x007C6BB0 - AIStateMachineDerived6B80::createSubMachine20
void* createSubMachine20_007C6BB0(
    void* /*machinePtr*/, std::int32_t a1, std::int32_t /*a2*/,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007BA5E0(rawMem, a1, 1, view);
}

// 26. 0x007C6BE0 - AIStateMachineDerived6B80::createSubMachine21
void* createSubMachine21_007C6BE0(
    void* /*machinePtr*/, std::int32_t a1, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007BA5E0(rawMem, a1, 1, view);
}

// 27. 0x007C6C10 - AIStateMachineDerived6C10::createSubMachine19
void* createSubMachine19_007C6C10(
    void* /*machinePtr*/, std::int32_t a1, std::int32_t a2, const void* a3,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    if (a3 != nullptr && ((readU32(a3, 0x80) >> 7) & 1u) != 0u) {
        void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
        if (rawMem == nullptr) {
            return nullptr;
        }
        return dispatch.ctor_007BADC0(rawMem, a1, a2, view);
    }

    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007BACA0(rawMem, a1, a2, view);
}

// 28. 0x007C6C80 - AIStateMachineDerived6C10::createSubMachine20
void* createSubMachine20_007C6C80(
    void* /*machinePtr*/, std::int32_t a1, const void* a2,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    if (a2 != nullptr && ((readU32(a2, 0x80) >> 7) & 1u) != 0u) {
        void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
        if (rawMem == nullptr) {
            return nullptr;
        }
        return dispatch.ctor_007BB180(rawMem, a1, view);
    }

    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007BB070(rawMem, a1, view);
}

// 29. 0x007C6E90 - AIStateMachineDerived6E90::initDefaultState
std::uint32_t initDefaultStateWithProducerCheck_007C6E90(
    void* machinePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const owner = readPtr(machinePtr, 0x24);
    void* const thingTemplate = readPtr(owner, 0x04);
    if (((readU32(thingTemplate, 0xDC) >> 20) & 1u) != 0u) {
        const std::uint32_t producerId = readU32(owner, 0xE4);
        if (producerId != 0) {
            void* const producer =
                dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), producerId, view);
            if (producer != nullptr) {
                void* const prodTemplate = readPtr(producer, 0x04);
                if (((readU32(prodTemplate, 0xD0) >> 3) & 1u) != 0u) {
                    void* const sub1 = readPtr(owner, 0x418);
                    void* const entry1 = dispatch.lookupEntry_004DF7C0(
                        addrToPtr(view.theWeaponStore), readU32(sub1, 0x20), 0x14, view);
                    const std::uint32_t val1 = readU32(entry1, 0x14);

                    void* const sub2 = readPtr(owner, 0x418);
                    void* const entry2 = dispatch.lookupEntry_004DF7C0(
                        addrToPtr(view.theWeaponStore), readU32(sub2, 0x20), 0x27, view);
                    const std::uint32_t val2 = readU32(entry2, 0x14);

                    if (val1 >= val2) {
                        dispatch.notifyDefaultState_00A78070(machinePtr, 0, view);
                    }
                } else {
                    dispatch.notifyDefaultState_00A78070(machinePtr, 0, view);
                }
            }
        }
    }
    return dispatch.baseInitDefaultState_005B25C0(machinePtr, view);
}

// 30. 0x007C86E0 - AIStateWithTargetObject86E0::onExit
void unitAIAttackBlockingObjectStateOnExit_007C86E0(
    void* statePtr, std::int32_t /*exitType*/, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    writeU8(machine0, 0x7D, 1u);

    const std::uint32_t targetId = readU32(statePtr, 0x2C);
    void* targetObj = nullptr;
    if (targetId != 0) {
        targetObj = dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), targetId, view);
    }

    void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
    dispatch.ownerTargetCleanup_006C0F40(owner, targetObj, view);

    void* const subMach = readPtr(statePtr, 0x20);
    if (subMach != nullptr) {
        callVtable1<void>(subMach, 0x00, 1);
    }
    writeU32(statePtr, 0x20, 0u);
}

// 31. 0x007C8C90 - AIExitInstantlyState::onEnter
std::uint32_t unitAIEnterAndDieStateOnEnter_007C8C90(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    const std::uint32_t containerId = readU32(owner, 0xE4);
    if (containerId != 0) {
        void* const containerObj =
            dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), containerId, view);
        if (containerObj != nullptr) {
            void* const containMod =
                dispatch.getContainModule_006A7620(containerObj, owner, 1, view);
            if (containMod != nullptr) {
                callVtable1<void>(containMod, 0x98, owner);
                callVtable2<void>(containMod, 0x40, owner, 2);
                return 0xFFFFFFFFu;
            }
        }
    }
    return 0xFFFFFFFEu;
}

// 32. 0x007C8D40 - AIResolveGoalTargetState8D40::onEnter
std::uint32_t unitAIFindReplacementStateOnEnter_007C8D40(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    const std::uint32_t rawKey = readU32(machine0, 0x28);
    const std::uint32_t resolvedId =
        dispatch.resolveObjectId_004DE030(addrToPtr(view.theGameLogic), rawKey, view);
    if (resolvedId == 0) {
        return 0xFFFFFFFEu;
    }

    void* const targetObj =
        dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), resolvedId, view);
    void* const machine1 = readPtr(statePtr, 0x18);
    callVtable1<void>(machine1, 0x38, targetObj);

    void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
    dispatch.ownerTargetCleanup_006C0F40(owner, targetObj, view);
    return 0xFFFFFFFFu;
}

// 33. 0x007C8DE0 - AIStateQueryWeaponClip8DE0::queryOffset
std::uint32_t* queryWeaponClipOffset_007C8DE0(
    void* statePtr, std::uint32_t* outPair, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const owner0 = readPtr(machine0, 0x24);
    void* const aiUpdate = readPtr(owner0, 0x374);
    void* const sub = dispatch.queryAIUpdateSub_0069A030(aiUpdate, view);
    if (sub != nullptr) {
        void* const target = readPtr(statePtr, 0x20);
        if (readU8(sub, 0x14) != 0) {
            dispatch.recomputeSub_007ADAB0(sub, view);
        }
        callVtable1<void>(target, 0x40, readF32(sub, 0x10));
    }

    void* const owner1 = readPtr(readPtr(statePtr, 0x18), 0x24);
    outPair[0] = readU32(owner1, 0x45C);
    outPair[1] = readU32(owner1, 0x460);
    return outPair;
}

// 34. 0x007C9150 - AIStateFactory9150::createSubMachine
void* unitAIGarrisonedStateCreateSubMachine_007C9150(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
    return dispatch.ctor_007C9070(rawMem, owner, view);
}

// 35. 0x007C9280 - AIStateFactory9280::createSubMachine
void* unitAIHoldGroundStateCreateSubMachine_007C9280(
    void* statePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    void* const owner = readPtr(readPtr(statePtr, 0x18), 0x24);
    return dispatch.ctor_007C91E0(rawMem, owner, view);
}

// 36. 0x007CC8A0 - AIStateMachine::createSubMachine19
void* aiStateMachineCreateSubMachine19_007CC8A0(
    void* /*machinePtr*/, std::int32_t a1, std::int32_t a2, std::int32_t /*a3*/,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007C8780(rawMem, a1, a2, view);
}

// 37. 0x007CC8D0 - AIStateMachine::createSubMachine20
void* aiStateMachineCreateSubMachine20_007CC8D0(
    void* /*machinePtr*/, std::int32_t a1, std::int32_t /*a2*/,
    AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const rawMem = dispatch.operatorNew_00416820(0x80, view);
    if (rawMem == nullptr) {
        return nullptr;
    }
    return dispatch.ctor_007C8E40(rawMem, a1, view);
}

// 38. 0x007CC920 - AIStateMachine::updateStateMachine
std::uint32_t aiStateMachineUpdate_007CC920(
    void* machinePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const tempState0 = readPtr(machinePtr, 0x88);
    if (tempState0 != nullptr) {
        std::uint32_t status = callVtable0<std::uint32_t>(tempState0, 0x14);
        const std::uint32_t tempFrameEnd = readU32(machinePtr, 0x8C);
        const std::uint32_t currentFrame =
            view.theGameLogic != 0 ? readU32(addrToPtr(view.theGameLogic), 0x58) : 0u;

#if defined(RA3_GAMELOGIC7C_ORACLE_MUTANT)
        const bool expired = (tempFrameEnd > currentFrame);
#else
        const bool expired = (tempFrameEnd < currentFrame);
#endif
        if (expired) {
            if (status == 0u) {
                status = 0xFFFFFFFFu;
            } else if (static_cast<std::int32_t>(status) >= 0) {
                return status;
            }
        } else {
            if (static_cast<std::int32_t>(status) >= 0) {
                return status;
            }
        }

        void* const tempState1 = readPtr(machinePtr, 0x88);
        if (tempState1 == nullptr) {
            return status;
        }

        callVtable1<void>(tempState1, 0x18, 0);
        const std::uint32_t savedGoalId = readU32(machinePtr, 0x90);
        void* savedGoalObj = nullptr;
        if (savedGoalId != 0) {
            savedGoalObj =
                dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), savedGoalId, view);
        }
        dispatch.setGoalObject_005AC4C0(machinePtr, savedGoalObj, view);
        dispatch.setGoalPosition_005AC4F0(
            machinePtr, bytesOf(machinePtr) + 0x94, view.defaultGoalRadius, view);
        writeU32(machinePtr, 0x90, 0u);
        writeU32(machinePtr, 0x88, 0u);

        void* const curState = readPtr(machinePtr, 0x04);
        if (curState != nullptr) {
            callVtable0<void>(curState, 0x1C);
        }
    }
    return dispatch.baseUpdateStateMachine_005AC420(machinePtr, view);
}

// 39. 0x007CCA30 - AIStateMachine::exitTemporaryState
void aiStateMachineExitTemporaryState_007CCA30(
    void* machinePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    void* const tempState = readPtr(machinePtr, 0x88);
    if (tempState == nullptr) {
        return;
    }

    callVtable1<void>(tempState, 0x18, 1);
    const std::uint32_t savedGoalId = readU32(machinePtr, 0x90);
    void* savedGoalObj = nullptr;
    if (savedGoalId != 0) {
        savedGoalObj =
            dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), savedGoalId, view);
    }
    dispatch.setGoalObject_005AC4C0(machinePtr, savedGoalObj, view);
    dispatch.setGoalPosition_005AC4F0(
        machinePtr, bytesOf(machinePtr) + 0x94, view.defaultGoalRadius, view);
    writeU32(machinePtr, 0x90, 0u);
    writeU32(machinePtr, 0x88, 0u);

    void* const owner = readPtr(machinePtr, 0x24);
    if (owner != nullptr) {
        void* const comp = readPtr(owner, 0x324);
        if (comp != nullptr) {
            dispatch.notifyTempExit_006838B0(comp, view);
        }
    }
}

// 40. 0x007CCB00 - AIStateMachine::clear
void aiStateMachineClear_007CCB00(
    void* machinePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    if (readU32(machinePtr, 0x88) != 0) {
        if (readU32(machinePtr, 0x8C) == 0xFFFFFFFFu) {
            return;
        }
        aiStateMachineExitTemporaryState_007CCA30(machinePtr, view, dispatch);
    }

    dispatch.baseClear_005C0840(machinePtr, view);
    void* const subMach = readPtr(machinePtr, 0x84);
    writeU32(machinePtr, 0x80, 0u);
    if (subMach != nullptr) {
        callVtable1<void>(subMach, 0x00, 1);
    }

    void* const owner = readPtr(machinePtr, 0x24);
    writeU32(machinePtr, 0x84, 0u);
    void* const aiUpdate = readPtr(owner, 0x374);
    if (aiUpdate != nullptr) {
        callVtable0<void>(aiUpdate, 0x2B0);
    }
}

// 41. 0x007CCB70 - AIStateMachine::resetToDefaultState
std::uint32_t aiStateMachineResetToDefaultState_007CCB70(
    void* machinePtr, AIStateMachine7CMemoryView& view, const GameDispatch& dispatch) noexcept
{
    if (readU32(machinePtr, 0x88) != 0) {
        if (readU32(machinePtr, 0x8C) == 0xFFFFFFFFu) {
            return 0u;
        }
        aiStateMachineExitTemporaryState_007CCA30(machinePtr, view, dispatch);
    }

    const std::uint32_t result = dispatch.baseResetToDefaultState_005C08C0(machinePtr, view);
    void* const owner = readPtr(machinePtr, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    if (aiUpdate != nullptr) {
        callVtable0<void>(aiUpdate, 0x2B0);
    }
    return result;
}

// 42. 0x007CCBC0 - AIStateMachine::setState
std::uint32_t aiStateMachineSetState_007CCBC0(
    void* machinePtr, std::uint32_t newStateId, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    if (readU32(machinePtr, 0x88) != 0) {
        if (readU32(machinePtr, 0x8C) == 0xFFFFFFFFu) {
            return 0u;
        }
        aiStateMachineExitTemporaryState_007CCA30(machinePtr, view, dispatch);
    }

    void* const curState = readPtr(machinePtr, 0x04);
    const std::uint32_t oldStateId = (curState != nullptr) ? readU32(curState, 0x04) : 0x000F423Fu;
    const std::uint32_t result = dispatch.baseSetState_005B6230(machinePtr, newStateId, view);

    void* const owner = readPtr(machinePtr, 0x24);
    void* const aiUpdate = readPtr(owner, 0x374);
    if (aiUpdate != nullptr && oldStateId != newStateId) {
        callVtable0<void>(aiUpdate, 0x2B0);
    }
    return result;
}

// 43. 0x007CE280 - AIEnterState::onExit
void aiEnterStateOnExit_007CE280(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    void* const machine = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine, 0x24);
    dispatch.baseStateOnExit_007B1190(statePtr, exitType, view);

    void* const aiUpdate = readPtr(owner, 0x374);
    if (aiUpdate != nullptr) {
        dispatch.aiUpdateHelper_006E6360(aiUpdate, 0, view);
        void* const sub = readPtr(aiUpdate, 0x214);
        if (sub != nullptr) {
            writeU32(sub, 0x40, readU32(sub, 0x40) & 0xFFFFFFFDu);
        }
    }

    const std::uint32_t targetId = readU32(statePtr, 0x50);
    if (targetId != 0) {
        void* const targetObj =
            dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), targetId, view);
        if (targetObj != nullptr) {
            void* const containMod = dispatch.getOpenContainModule_006A7600(targetObj, view);
            if (containMod != nullptr) {
                callVtable2<void>(containMod, 0x40, owner, 2);
            }
        }
    }
}

// 44. 0x007CE460 - AIExitState::onExit
void aiExitStateOnExit_007CE460(
    void* statePtr, std::int32_t /*exitType*/, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    void* const machine0 = readPtr(statePtr, 0x18);
    void* const owner = readPtr(machine0, 0x24);
    const std::uint32_t targetId = readU32(statePtr, 0x20);
    if (targetId != 0) {
        void* const targetObj =
            dispatch.findObjectById_0050B5F0(addrToPtr(view.theGameLogic), targetId, view);
        if (targetObj != nullptr) {
            void* const containMod = dispatch.getOpenContainModule_006A7600(targetObj, view);
            if (containMod != nullptr) {
                callVtable2<void>(containMod, 0x40, owner, 2);
            }
        }
    }

    void* const machine1 = readPtr(statePtr, 0x18);
    callVtable1<void>(machine1, 0x38, static_cast<void*>(nullptr));
}

// 45. 0x007CE500 - AIExitInstantlyState::onExit
void aiExitSiegeEngineStateOnExit_007CE500(
    void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView& view,
    const GameDispatch& dispatch) noexcept
{
    aiExitStateOnExit_007CE460(statePtr, exitType, view, dispatch);
    void* const machine = readPtr(statePtr, 0x18);
    callVtable1<void>(machine, 0x38, static_cast<void*>(nullptr));
}

}  // namespace ra3::gamelogic
