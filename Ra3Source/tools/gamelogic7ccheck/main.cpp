#include <ra3/gamelogic/AIStateMachine7C.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace ra3::gamelogic;

static_assert(sizeof(void*) == 4, "gamelogic7ccheck models 32-bit x86 pointers");

constexpr std::size_t kArenaSize = 0x2000;

template <typename T>
T load(const std::uint8_t* data, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

template <typename T>
void store(std::uint8_t* data, std::size_t offset, T value) noexcept
{
    std::memcpy(data + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> decodeHex(const std::string& text)
{
    if ((text.size() & 1u) != 0u) {
        throw std::runtime_error("odd hex length");
    }
    auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return unsigned(c - '0');
        if (c >= 'a' && c <= 'f') return unsigned(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return unsigned(c - 'A' + 10);
        throw std::runtime_error("invalid hex");
    };
    std::vector<std::uint8_t> out(text.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>((nibble(text[i * 2]) << 4u) | nibble(text[i * 2 + 1]));
    }
    return out;
}

std::string encodeHex(const std::vector<std::uint8_t>& data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint8_t b : data) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

std::vector<std::size_t> parseOffsets(const std::string& text)
{
    std::vector<std::size_t> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    std::istringstream iss(text);
    for (std::string item; std::getline(iss, item, ',');) {
        out.push_back(static_cast<std::size_t>(std::stoul(item, nullptr, 16)));
    }
    return out;
}

struct RuntimeContext {
    std::uint8_t* base = nullptr;
    std::uint32_t eventHash = 0x811C9DC5u;
    std::uint32_t xferByteCallIndex = 0;

    std::uint32_t rel(const void* ptr) const noexcept
    {
        const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
        const auto start = reinterpret_cast<std::uintptr_t>(base);
        if (addr >= start && addr < start + kArenaSize) {
            return static_cast<std::uint32_t>(addr - start);
        }
        return static_cast<std::uint32_t>(addr);
    }

    std::uint32_t relU32(std::uint32_t addr) const noexcept
    {
        const auto start = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base));
        if (addr >= start && addr < start + kArenaSize) {
            return addr - start;
        }
        return addr;
    }

    void mix(std::uint32_t tag, std::uint32_t a = 0, std::uint32_t b = 0,
             std::uint32_t c = 0) noexcept
    {
        const std::uint32_t vals[4] = {tag, a, b, c};
        for (std::uint32_t v : vals) {
            eventHash = (eventHash ^ v) * 16777619u;
        }
    }

    void* ptrAtOffset(std::uint32_t off) const noexcept
    {
        return off == 0 ? nullptr : (base + off);
    }
};

RuntimeContext* g_ctx = nullptr;

// Synthetic vtable callbacks for C++ checker
void __fastcall vcallSubMachDtor(void* self, void*, int flags)
{
    g_ctx->mix(0x90000000u, g_ctx->rel(self), static_cast<std::uint32_t>(flags));
}

std::uint32_t __fastcall vcallSubMachInitDefault(void* self, void*)
{
    g_ctx->mix(0x90000014u, g_ctx->rel(self));
    return load<std::uint32_t>(g_ctx->base, 0x0038);
}

std::uint32_t __fastcall vcallSubMachSetState(void* self, void*, std::uint32_t stateId)
{
    g_ctx->mix(0x9000001Cu, g_ctx->rel(self), stateId);
    return load<std::uint32_t>(g_ctx->base, 0x0038);
}

void __fastcall vcallSubMachClear(void* self, void*)
{
    g_ctx->mix(0x90000034u, g_ctx->rel(self));
}

void __fastcall vcallSubMachSetGoalObject(void* self, void*, void* goalObj)
{
    g_ctx->mix(0x90000038u, g_ctx->rel(self), g_ctx->rel(goalObj));
}

std::uint32_t __fastcall vcallStateUpdate(void* self, void*)
{
    g_ctx->mix(0x91000014u, g_ctx->rel(self));
    return load<std::uint32_t>(g_ctx->base, 0x0038);
}

void __fastcall vcallStateOnExit(void* self, void*, int exitType)
{
    g_ctx->mix(0x91000018u, g_ctx->rel(self), static_cast<std::uint32_t>(exitType));
}

void __fastcall vcallStateResume(void* self, void*)
{
    g_ctx->mix(0x9100001Cu, g_ctx->rel(self));
}

void* __fastcall vcallAIUpdateGetSubMod(void* self, void*)
{
    g_ctx->mix(0x920001C4u, g_ctx->rel(self));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x003C));
}

void __fastcall vcallAIUpdate270(void* self, void*)
{
    g_ctx->mix(0x92000270u, g_ctx->rel(self));
}

void __fastcall vcallAIUpdate2B0(void* self, void*)
{
    g_ctx->mix(0x920002B0u, g_ctx->rel(self));
}

std::uint32_t __fastcall vcallSubModCheck84(void* self, void*)
{
    g_ctx->mix(0x93000084u, g_ctx->rel(self));
    return load<std::uint32_t>(g_ctx->base, 0x0040);
}

void __fastcall vcallXferVersion(void* self, void*, std::uint8_t* verPair)
{
    const std::uint32_t packed =
        static_cast<std::uint32_t>(verPair[0]) | (static_cast<std::uint32_t>(verPair[1]) << 8u);
    g_ctx->mix(0x94000018u, g_ctx->rel(self), packed);
}

void __fastcall vcallXferObject(void* self, void*, void* subMach)
{
    g_ctx->mix(0x94000020u, g_ctx->rel(self), g_ctx->rel(subMach));
}

void __fastcall vcallXferByte(void* self, void*, std::uint8_t* bytePtr)
{
    const std::uint32_t callIdx = g_ctx->xferByteCallIndex++;
    if (callIdx == 0u) {
        const std::uint32_t overrideVal = load<std::uint32_t>(g_ctx->base, 0x0044);
        if (overrideVal != 0xFFFFFFFFu) {
            *bytePtr = static_cast<std::uint8_t>(overrideVal & 0xFFu);
        }
    }
    g_ctx->mix(0x94000084u, g_ctx->rel(self), *bytePtr, callIdx);
}

void __fastcall vcallContain40(void* self, void*, void* owner, int arg)
{
    g_ctx->mix(0x95000040u, g_ctx->rel(self), g_ctx->rel(owner), static_cast<std::uint32_t>(arg));
}

void __fastcall vcallContain98(void* self, void*, void* owner)
{
    g_ctx->mix(0x95000098u, g_ctx->rel(self), g_ctx->rel(owner));
}

void __fastcall vcallWeaponTarget40(void* self, void*, float val)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &val, sizeof(bits));
    g_ctx->mix(0x96000040u, g_ctx->rel(self), bits);
}

// Install synthetic vtable function pointers in arena [0x1900..0x1FFF]
void installSyntheticVtables(std::uint8_t* base) noexcept
{
    // 0x1900: SubMachine / AIStateMachine synthetic vtable
    store(base, 0x1900 + 0x00, reinterpret_cast<std::uint32_t>(&vcallSubMachDtor));
    store(base, 0x1900 + 0x14, reinterpret_cast<std::uint32_t>(&vcallSubMachInitDefault));
    store(base, 0x1900 + 0x1C, reinterpret_cast<std::uint32_t>(&vcallSubMachSetState));
    store(base, 0x1900 + 0x34, reinterpret_cast<std::uint32_t>(&vcallSubMachClear));
    store(base, 0x1900 + 0x38, reinterpret_cast<std::uint32_t>(&vcallSubMachSetGoalObject));

    // 0x1980: State synthetic vtable
    store(base, 0x1980 + 0x14, reinterpret_cast<std::uint32_t>(&vcallStateUpdate));
    store(base, 0x1980 + 0x18, reinterpret_cast<std::uint32_t>(&vcallStateOnExit));
    store(base, 0x1980 + 0x1C, reinterpret_cast<std::uint32_t>(&vcallStateResume));

    // 0x1A00: AIUpdateInterface synthetic vtable
    store(base, 0x1A00 + 0x1C4, reinterpret_cast<std::uint32_t>(&vcallAIUpdateGetSubMod));
    store(base, 0x1A00 + 0x270, reinterpret_cast<std::uint32_t>(&vcallAIUpdate270));
    store(base, 0x1A00 + 0x2B0, reinterpret_cast<std::uint32_t>(&vcallAIUpdate2B0));

    // 0x1D00: SubModule synthetic vtable
    store(base, 0x1D00 + 0x84, reinterpret_cast<std::uint32_t>(&vcallSubModCheck84));

    // 0x1DA0: Xfer synthetic vtable
    store(base, 0x1DA0 + 0x18, reinterpret_cast<std::uint32_t>(&vcallXferVersion));
    store(base, 0x1DA0 + 0x20, reinterpret_cast<std::uint32_t>(&vcallXferObject));
    store(base, 0x1DA0 + 0x84, reinterpret_cast<std::uint32_t>(&vcallXferByte));

    // 0x1E40: ContainModule synthetic vtable
    store(base, 0x1E40 + 0x40, reinterpret_cast<std::uint32_t>(&vcallContain40));
    store(base, 0x1E40 + 0x98, reinterpret_cast<std::uint32_t>(&vcallContain98));

    // 0x1F00: WeaponTarget synthetic vtable
    store(base, 0x1F00 + 0x40, reinterpret_cast<std::uint32_t>(&vcallWeaponTarget40));
}

// GameDispatch callbacks backed by RuntimeContext
void* cbOperatorNew(std::size_t size, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00416820u, static_cast<std::uint32_t>(size));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x0000));
}

void* cbGetGoalObject(void* machinePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005B9F30u, g_ctx->rel(machinePtr));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x000C));
}

std::uint32_t cbGetAIUpdateTargetId(void* aiUpdatePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00683630u, g_ctx->rel(aiUpdatePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0010);
}

float cbQueryHeight(void* geomPtr, std::int32_t arg, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00981070u, g_ctx->rel(geomPtr), static_cast<std::uint32_t>(arg));
    return load<float>(g_ctx->base, 0x0014);
}

std::uint32_t cbStartMove(void* statePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C08A0u, g_ctx->rel(statePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0004);
}

bool cbCheckMoveOut(void* aiUpdatePtr, std::int32_t arg, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006FFB00u, g_ctx->rel(aiUpdatePtr), static_cast<std::uint32_t>(arg));
    return load<std::uint32_t>(g_ctx->base, 0x0008) != 0u;
}

void cbSetStatusHelper(void* ownerPtr, std::int32_t bitIndex, std::int32_t val,
                       AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006A4F80u, g_ctx->rel(ownerPtr), static_cast<std::uint32_t>(bitIndex),
               static_cast<std::uint32_t>(val));
}

void cbPostStatusHelper(void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006A4FB0u, g_ctx->rel(ownerPtr));
}

void cbOwnerHelper(void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00686E00u, g_ctx->rel(ownerPtr));
}

void cbRepulsorHelper(void* aiUpdatePtr, void* ioPos, void* refPos, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007526C0u, g_ctx->rel(aiUpdatePtr), g_ctx->rel(ioPos), g_ctx->rel(refPos));
}

bool cbAiUpdateMode(void* aiUpdatePtr, std::int32_t mode, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007527C0u, g_ctx->rel(aiUpdatePtr), static_cast<std::uint32_t>(mode));
    return true;
}

void cbOwnerAction(void* ownerPtr, std::int32_t bit, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x004D9AF0u, g_ctx->rel(ownerPtr), static_cast<std::uint32_t>(bit));
}

void cbAiUpdateSetTargetId(void* aiUpdatePtr, std::uint32_t objId, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00752720u, g_ctx->rel(aiUpdatePtr), objId);
}

float cbQueryOwnerRange(void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006A6510u, g_ctx->rel(ownerPtr));
    return load<float>(g_ctx->base, 0x0018);
}

void* cbFindTargetInRange(void* mgrPtr, void* ownerPtr, float range, AIStateMachine7CMemoryView&)
{
    std::uint32_t rangeBits = 0;
    std::memcpy(&rangeBits, &range, sizeof(rangeBits));
    g_ctx->mix(0x00798BE0u, g_ctx->rel(mgrPtr), g_ctx->rel(ownerPtr), rangeBits);
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x000C));
}

void* cbCtor0E80(void* memPtr, void* ownerPtr, void* goalObj, std::uint32_t tag,
                 AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007B1E90u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr), g_ctx->rel(goalObj));
    g_ctx->mix(tag);
    return memPtr;
}

std::uint32_t cbDelayedFallback(void* statePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007B12D0u, g_ctx->rel(statePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void cbLockMachine(void* machinePtr, std::uint32_t tagAddr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005AA240u, g_ctx->rel(machinePtr), tagAddr);
    if (machinePtr != nullptr) {
        *(static_cast<std::uint8_t*>(machinePtr) + 0x78) = 1u;
    }
}

std::uint32_t cbBaseMoveOnEnter(void* statePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C3740u, g_ctx->rel(statePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void* cbCtor7BD4D0(void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BD4D0u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr));
    return memPtr;
}

void* cbCheckGuardTarget(void* goalObjPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00687810u, g_ctx->rel(goalObjPtr));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x001C));
}

void* cbCtor7BE020(void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BE020u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr));
    return memPtr;
}

void cbSetGoalPosition(void* machinePtr, const void* posPtr, float radius,
                       AIStateMachine7CMemoryView& view)
{
    std::uint32_t radBits = 0;
    std::memcpy(&radBits, &radius, sizeof(radBits));
    g_ctx->mix(0x005AC4F0u, g_ctx->rel(machinePtr), g_ctx->rel(posPtr), radBits);
    noOpDispatch().setGoalPosition_005AC4F0(machinePtr, posPtr, radius, view);
}

void* cbCtor7BDF30(void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BDF30u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr));
    return memPtr;
}

std::uint32_t cbFollowPathOnEnter(void* statePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C2CA0u, g_ctx->rel(statePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void* cbCtor7BA5E0(void* memPtr, std::int32_t a1, std::int32_t a2, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BA5E0u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)),
               g_ctx->relU32(static_cast<std::uint32_t>(a2)));
    return memPtr;
}

void* cbCtor7BADC0(void* memPtr, std::int32_t a1, std::int32_t a2, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BADC0u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)),
               g_ctx->relU32(static_cast<std::uint32_t>(a2)));
    return memPtr;
}

void* cbCtor7BACA0(void* memPtr, std::int32_t a1, std::int32_t a2, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BACA0u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)),
               g_ctx->relU32(static_cast<std::uint32_t>(a2)));
    return memPtr;
}

void* cbCtor7BB180(void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BB180u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)));
    return memPtr;
}

void* cbCtor7BB070(void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007BB070u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)));
    return memPtr;
}

void* cbFindObjectById(void* gameLogicPtr, std::uint32_t objId, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x0050B5F0u, g_ctx->rel(gameLogicPtr), objId);
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x0020));
}

void* cbLookupEntry(void* tablePtr, std::uint32_t id, std::int32_t kind,
                    AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x004DF7C0u, g_ctx->rel(tablePtr), id, static_cast<std::uint32_t>(kind));
    const std::size_t slot = (kind == 0x14) ? 0x0024u : 0x0028u;
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, slot));
}

void cbNotifyDefaultState(void* machinePtr, std::int32_t arg, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x00A78070u, g_ctx->rel(machinePtr), static_cast<std::uint32_t>(arg));
}

std::uint32_t cbBaseInitDefault(void* machinePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005B25C0u, g_ctx->rel(machinePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void cbOwnerTargetCleanup(void* ownerPtr, void* targetObjPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006C0F40u, g_ctx->rel(ownerPtr), g_ctx->rel(targetObjPtr));
}

void* cbGetContainModule(void* containerObjPtr, void* passengerPtr, std::int32_t arg,
                         AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006A7620u, g_ctx->rel(containerObjPtr), g_ctx->rel(passengerPtr),
               static_cast<std::uint32_t>(arg));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x002C));
}

std::uint32_t cbResolveObjectId(void* gameLogicPtr, std::uint32_t rawKey,
                                AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x004DE030u, g_ctx->rel(gameLogicPtr), rawKey);
    return load<std::uint32_t>(g_ctx->base, 0x0030);
}

void* cbQueryAIUpdateSub(void* aiUpdatePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x0069A030u, g_ctx->rel(aiUpdatePtr));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x0034));
}

void cbRecomputeSub(void* subPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007ADAB0u, g_ctx->rel(subPtr));
}

void* cbCtor7C9070(void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C9070u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr));
    return memPtr;
}

void* cbCtor7C91E0(void* memPtr, void* ownerPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C91E0u, g_ctx->rel(memPtr), g_ctx->rel(ownerPtr));
    return memPtr;
}

void* cbCtor7C8780(void* memPtr, std::int32_t a1, std::int32_t a2, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C8780u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)),
               g_ctx->relU32(static_cast<std::uint32_t>(a2)));
    return memPtr;
}

void* cbCtor7C8E40(void* memPtr, std::int32_t a1, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007C8E40u, g_ctx->rel(memPtr), g_ctx->relU32(static_cast<std::uint32_t>(a1)));
    return memPtr;
}

void cbSetGoalObject(void* machinePtr, void* goalObjPtr, AIStateMachine7CMemoryView& view)
{
    g_ctx->mix(0x005AC4C0u, g_ctx->rel(machinePtr), g_ctx->rel(goalObjPtr));
    noOpDispatch().setGoalObject_005AC4C0(machinePtr, goalObjPtr, view);
}

std::uint32_t cbBaseUpdateMachine(void* machinePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005AC420u, g_ctx->rel(machinePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void cbNotifyTempExit(void* compPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006838B0u, g_ctx->rel(compPtr));
}

void cbBaseClear(void* machinePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005C0840u, g_ctx->rel(machinePtr));
}

std::uint32_t cbBaseResetDefault(void* machinePtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005C08C0u, g_ctx->rel(machinePtr));
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

std::uint32_t cbBaseSetState(void* machinePtr, std::uint32_t newStateId,
                             AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x005B6230u, g_ctx->rel(machinePtr), newStateId);
    return load<std::uint32_t>(g_ctx->base, 0x0048);
}

void cbBaseStateOnExit(void* statePtr, std::int32_t exitType, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x007B1190u, g_ctx->rel(statePtr), static_cast<std::uint32_t>(exitType));
}

void cbAiUpdateHelper(void* aiUpdatePtr, std::int32_t arg, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006E6360u, g_ctx->rel(aiUpdatePtr), static_cast<std::uint32_t>(arg));
}

void* cbGetOpenContainModule(void* objPtr, AIStateMachine7CMemoryView&)
{
    g_ctx->mix(0x006A7600u, g_ctx->rel(objPtr));
    return g_ctx->ptrAtOffset(load<std::uint32_t>(g_ctx->base, 0x002C));
}

const GameDispatch& checkerDispatch() noexcept
{
    static const GameDispatch kDisp = [] {
        GameDispatch d{};
        d.operatorNew_00416820 = &cbOperatorNew;
        d.getGoalObject_005B9F30 = &cbGetGoalObject;
        d.getAIUpdateTargetId_00683630 = &cbGetAIUpdateTargetId;
        d.queryHeight_00981070 = &cbQueryHeight;
        d.startMove_007C08A0 = &cbStartMove;
        d.checkMoveOut_006FFB00 = &cbCheckMoveOut;
        d.setStatusHelper_006A4F80 = &cbSetStatusHelper;
        d.postStatusHelper_006A4FB0 = &cbPostStatusHelper;
        d.ownerHelper_00686E00 = &cbOwnerHelper;
        d.repulsorHelper_007526C0 = &cbRepulsorHelper;
        d.aiUpdateMode_007527C0 = &cbAiUpdateMode;
        d.ownerAction_004D9AF0 = &cbOwnerAction;
        d.aiUpdateSetTargetId_00752720 = &cbAiUpdateSetTargetId;
        d.queryOwnerRange_006A6510 = &cbQueryOwnerRange;
        d.findTargetInRange_00798BE0 = &cbFindTargetInRange;
        d.ctor_007B1E90 = &cbCtor0E80;
        d.delayedMoveFallback_007B12D0 = &cbDelayedFallback;
        d.lockMachine_005AA240 = &cbLockMachine;
        d.baseMoveOnEnter_007C3740 = &cbBaseMoveOnEnter;
        d.ctor_007BD4D0 = &cbCtor7BD4D0;
        d.checkGuardTarget_00687810 = &cbCheckGuardTarget;
        d.ctor_007BE020 = &cbCtor7BE020;
        d.setGoalPosition_005AC4F0 = &cbSetGoalPosition;
        d.ctor_007BDF30 = &cbCtor7BDF30;
        d.followPathOnEnter_007C2CA0 = &cbFollowPathOnEnter;
        d.ctor_007BA5E0 = &cbCtor7BA5E0;
        d.ctor_007BADC0 = &cbCtor7BADC0;
        d.ctor_007BACA0 = &cbCtor7BACA0;
        d.ctor_007BB180 = &cbCtor7BB180;
        d.ctor_007BB070 = &cbCtor7BB070;
        d.findObjectById_0050B5F0 = &cbFindObjectById;
        d.lookupEntry_004DF7C0 = &cbLookupEntry;
        d.notifyDefaultState_00A78070 = &cbNotifyDefaultState;
        d.baseInitDefaultState_005B25C0 = &cbBaseInitDefault;
        d.ownerTargetCleanup_006C0F40 = &cbOwnerTargetCleanup;
        d.getContainModule_006A7620 = &cbGetContainModule;
        d.resolveObjectId_004DE030 = &cbResolveObjectId;
        d.queryAIUpdateSub_0069A030 = &cbQueryAIUpdateSub;
        d.recomputeSub_007ADAB0 = &cbRecomputeSub;
        d.ctor_007C9070 = &cbCtor7C9070;
        d.ctor_007C91E0 = &cbCtor7C91E0;
        d.ctor_007C8780 = &cbCtor7C8780;
        d.ctor_007C8E40 = &cbCtor7C8E40;
        d.setGoalObject_005AC4C0 = &cbSetGoalObject;
        d.baseUpdateStateMachine_005AC420 = &cbBaseUpdateMachine;
        d.notifyTempExit_006838B0 = &cbNotifyTempExit;
        d.baseClear_005C0840 = &cbBaseClear;
        d.baseResetToDefaultState_005C08C0 = &cbBaseResetDefault;
        d.baseSetState_005B6230 = &cbBaseSetState;
        d.baseStateOnExit_007B1190 = &cbBaseStateOnExit;
        d.aiUpdateHelper_006E6360 = &cbAiUpdateHelper;
        d.getOpenContainModule_006A7600 = &cbGetOpenContainModule;
        return d;
    }();
    return kDisp;
}

std::uint32_t runFunction(std::uint32_t va, std::uint8_t* base, AIStateMachine7CMemoryView& view)
{
    void* const self = base + 0x0100;
    const GameDispatch& disp = checkerDispatch();
    const std::uint32_t arg0 = load<std::uint32_t>(base, 0x004C);
    const std::uint32_t arg1 = load<std::uint32_t>(base, 0x0050);
    const std::uint32_t arg2 = load<std::uint32_t>(base, 0x0054);

    switch (va) {
    case 0x007C0AD0:
        return aiMoveToStateOnEnter_007C0AD0(self, view, disp);
    case 0x007C0BD0:
        return aiMoveToStateSAOnEnter_007C0BD0(self, view, disp);
    case 0x007C0C80:
        return aiMoveOutOfTheWayStateOnEnter_007C0C80(self, view, disp);
    case 0x007C0D00:
        return aiMoveAndTightenStateOnEnter_007C0D00(self, view, disp);
    case 0x007C0D70:
        return aiMoveAwayPanicStateOnEnter_007C0D70(self, view, disp);
    case 0x007C0DF0:
        return aiMoveAwayFromRepulsorsStateOnEnter_007C0DF0(self, view, disp);
    case 0x007C0E80:
        return aiBackAwayAndCowerStateOnEnter_007C0E80(self, view, disp);
    case 0x007C0EE0:
        return aiMoveAwayAndCowerStateOnEnter_007C0EE0(self, view, disp);
    case 0x007C2C80:
        return aiPickUpCrateStateUpdate_007C2C80(self, view, disp);
    case 0x007C3520:
        return aiMoveAndEvacuateStateOnEnter_007C3520(self, view, disp);
    case 0x007C3680:
        return aiMoveAndDeleteStateOnEnter_007C3680(self, view, disp);
    case 0x007C39C0:
        return aiFollowWaypointPathStateAndEvacuateOnEnter_007C39C0(self, view, disp);
    case 0x007C3B50:
        return aiAttackFollowWaypointPathStateOnEnter_007C3B50(self, view, disp);
    case 0x007C42E0:
        aiDockStateXfer_007C42E0(self, base + arg0, view, disp);
        return 0u;
    case 0x007C43A0:
        return aiDockStateOnEnter_007C43A0(self, view, disp);
    case 0x007C4450:
        aiHarvestStateXfer_007C4450(self, base + arg0, view, disp);
        return 0u;
    case 0x007C4510:
        return aiHarvestStateOnEnter_007C4510(self, view, disp);
    case 0x007C4820:
        aiGuardRetaliateStateXfer_007C4820(self, base + arg0, view, disp);
        return 0u;
    case 0x007C48C0:
        return aiGuardRetaliateStateOnEnter_007C48C0(self, view, disp);
    case 0x007C4BE0:
        return aiMoveToPositionAndDieStateOnEnter_007C4BE0(self, view, disp);
    case 0x007C4D30:
        return aiMoveToAndEvacuateStateOnEnter_007C4D30(self, view, disp);
    case 0x007C4DC0:
        return aiFollowPathAndEvacuateStateOnEnter_007C4DC0(self, view, disp);
    case 0x007C4DE0:
        return aiMoveForBoardingOnEnter_007C4DE0(self, view, disp);
    case 0x007C6B80:
        return g_ctx->rel(createSubMachine19_007C6B80(
            self, static_cast<std::int32_t>(arg0), static_cast<std::int32_t>(arg1),
            static_cast<std::int32_t>(arg2), view, disp));
    case 0x007C6BB0:
        return g_ctx->rel(createSubMachine20_007C6BB0(
            self, static_cast<std::int32_t>(arg0), static_cast<std::int32_t>(arg1), view, disp));
    case 0x007C6BE0:
        return g_ctx->rel(createSubMachine21_007C6BE0(
            self, static_cast<std::int32_t>(arg0), view, disp));
    case 0x007C6C10:
        return g_ctx->rel(createSubMachine19_007C6C10(
            self, static_cast<std::int32_t>(arg0), static_cast<std::int32_t>(arg1),
            arg2 == 0 ? nullptr : (base + arg2), view, disp));
    case 0x007C6C80:
        return g_ctx->rel(createSubMachine20_007C6C80(
            self, static_cast<std::int32_t>(arg0), arg1 == 0 ? nullptr : (base + arg1), view,
            disp));
    case 0x007C6E90:
        return initDefaultStateWithProducerCheck_007C6E90(self, view, disp);
    case 0x007C86E0:
        unitAIAttackBlockingObjectStateOnExit_007C86E0(self, static_cast<std::int32_t>(arg0), view, disp);
        return 0u;
    case 0x007C8C90:
        return unitAIEnterAndDieStateOnEnter_007C8C90(self, view, disp);
    case 0x007C8D40:
        return unitAIFindReplacementStateOnEnter_007C8D40(self, view, disp);
    case 0x007C8DE0:
        return g_ctx->rel(queryWeaponClipOffset_007C8DE0(
            self, reinterpret_cast<std::uint32_t*>(base + arg0), view, disp));
    case 0x007C9150:
        return g_ctx->rel(unitAIGarrisonedStateCreateSubMachine_007C9150(self, view, disp));
    case 0x007C9280:
        return g_ctx->rel(unitAIHoldGroundStateCreateSubMachine_007C9280(self, view, disp));
    case 0x007CC8A0:
        return g_ctx->rel(aiStateMachineCreateSubMachine19_007CC8A0(
            self, static_cast<std::int32_t>(arg0), static_cast<std::int32_t>(arg1),
            static_cast<std::int32_t>(arg2), view, disp));
    case 0x007CC8D0:
        return g_ctx->rel(aiStateMachineCreateSubMachine20_007CC8D0(
            self, static_cast<std::int32_t>(arg0), static_cast<std::int32_t>(arg1), view, disp));
    case 0x007CC920:
        return aiStateMachineUpdate_007CC920(self, view, disp);
    case 0x007CCA30:
        aiStateMachineExitTemporaryState_007CCA30(self, view, disp);
        return 0u;
    case 0x007CCB00:
        aiStateMachineClear_007CCB00(self, view, disp);
        return 0u;
    case 0x007CCB70:
        return aiStateMachineResetToDefaultState_007CCB70(self, view, disp);
    case 0x007CCBC0:
        return aiStateMachineSetState_007CCBC0(self, arg0, view, disp);
    case 0x007CE280:
        aiEnterStateOnExit_007CE280(self, static_cast<std::int32_t>(arg0), view, disp);
        return 0u;
    case 0x007CE460:
        aiExitStateOnExit_007CE460(self, static_cast<std::int32_t>(arg0), view, disp);
        return 0u;
    case 0x007CE500:
        aiExitSiegeEngineStateOnExit_007CE500(self, static_cast<std::int32_t>(arg0), view, disp);
        return 0u;
    default:
        throw std::runtime_error("unknown VA");
    }
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line.empty()) {
            break;
        }
        std::istringstream iss(line);
        std::string vaHex;
        std::string relocText;
        std::string arenaHex;
        if (!(iss >> vaHex >> relocText >> arenaHex)) {
            std::cout << "error\n" << std::flush;
            continue;
        }

        const std::uint32_t va = static_cast<std::uint32_t>(std::stoul(vaHex, nullptr, 16));
        const std::vector<std::size_t> relocs = parseOffsets(relocText);
        std::vector<std::uint8_t> arena = decodeHex(arenaHex);
        if (arena.size() != kArenaSize) {
            std::cout << "error\n" << std::flush;
            continue;
        }

        const auto baseAddr =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(arena.data()));
        for (std::size_t off : relocs) {
            const std::uint32_t val = load<std::uint32_t>(arena.data(), off);
            if (val != 0 && val < kArenaSize) {
                store<std::uint32_t>(arena.data(), off, baseAddr + val);
            }
        }

        installSyntheticVtables(arena.data());

        RuntimeContext ctx{};
        ctx.base = arena.data();
        g_ctx = &ctx;

        AIStateMachine7CMemoryView view{};
        const std::uint32_t glOff = load<std::uint32_t>(arena.data(), 0x0058);
        const std::uint32_t wsOff = load<std::uint32_t>(arena.data(), 0x005C);
        const std::uint32_t pmOff = load<std::uint32_t>(arena.data(), 0x0060);
        view.theGameLogic = glOff != 0 ? (baseAddr + glOff) : 0u;
        view.theWeaponStore = wsOff != 0 ? (baseAddr + wsOff) : 0u;
        view.thePartitionMgr = pmOff != 0 ? (baseAddr + pmOff) : 0u;

        const std::uint32_t result = runFunction(va, arena.data(), view);
        g_ctx = nullptr;

        for (std::size_t off : relocs) {
            const std::uint32_t val = load<std::uint32_t>(arena.data(), off);
            if (val >= baseAddr && val < baseAddr + kArenaSize) {
                store<std::uint32_t>(arena.data(), off, val - baseAddr);
            }
        }
        std::memset(arena.data() + 0x1900, 0, kArenaSize - 0x1900);

        std::cout << std::hex << std::setfill('0') << std::setw(8) << result << ' '
                  << std::setw(8) << ctx.eventHash << ' ' << encodeHex(arena) << '\n'
                  << std::flush;
    }
    return 0;
}
