#include <ra3/asset/ReloDetokenizers.h>

#include <cstring>

namespace ra3::asset {

namespace {

inline std::uint8_t* bytePtr(void* base, std::size_t offset = 0) noexcept
{
    return static_cast<std::uint8_t*>(base) + offset;
}

inline const std::uint8_t* bytePtr(const void* base, std::size_t offset = 0) noexcept
{
    return static_cast<const std::uint8_t*>(base) + offset;
}

inline std::uint32_t readU32(const void* src, std::size_t offset = 0) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytePtr(src, offset), sizeof(value));
    return value;
}

inline void writeU32(void* dst, std::size_t offset, std::uint32_t value) noexcept
{
    std::memcpy(bytePtr(dst, offset), &value, sizeof(value));
}

inline std::uint8_t readU8(const void* src, std::size_t offset = 0) noexcept
{
    return *bytePtr(src, offset);
}

inline void writeU8(void* dst, std::size_t offset, std::uint8_t value) noexcept
{
    *bytePtr(dst, offset) = value;
}

inline const void* resolveFieldSource(const void* instanceBase, std::uint32_t offset) noexcept
{
    if (offset == 0) {
        return nullptr;
    }
    return bytePtr(instanceBase, offset);
}

void cloneDirectBytes(
    const void* srcField,
    void** dstPtrSlot,
    std::uint32_t allocBytes,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (srcField == nullptr) {
        *dstPtrSlot = nullptr;
        return;
    }
    void* newObj = hooks.allocTagged(allocBytes, hooks.user);
    std::memset(newObj, 0, allocBytes);
    *dstPtrSlot = newObj;
    std::memcpy(newObj, srcField, allocBytes);
}

void cloneWithExternalDetokenizer(
    std::uint32_t calleeVa,
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    std::uint32_t allocBytes,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (srcField == nullptr) {
        *dstPtrSlot = nullptr;
        return;
    }
    void* newObj = hooks.allocTagged(allocBytes, hooks.user);
    std::memset(newObj, 0, allocBytes);
    *dstPtrSlot = newObj;
    hooks.invokeExternalDetokenizer(calleeVa, instanceBase, srcField, newObj, context, hooks.user);
}

}  // namespace

// ============================================================================
// Group 1: Core .relo & Asset Memory/Buffer/Span Primitives (5 functions)
// ============================================================================

// 0x00416ED0 -> 0x004416E0 (.text, slot 0x018FB790)
int assetMemoryCompare(const void* lhs, const void* rhs, std::uint32_t byteCount) noexcept
{
    const auto* left = static_cast<const std::uint8_t*>(lhs);
    const auto* right = static_cast<const std::uint8_t*>(rhs);
    std::uint32_t remaining = byteCount;

    while (remaining >= 4) {
        if (readU32(left) != readU32(right)) {
            break;
        }
        remaining -= 4;
        left += 4;
        right += 4;
    }

    if (remaining == 0) {
        return 0;
    }

    for (int step = 0; step < 4 && remaining != 0; ++step) {
        const int diff = static_cast<int>(*left) - static_cast<int>(*right);
        if (diff != 0) {
            return (diff > 0) ? 1 : -1;
        }
        if (step == 3) {
            break;
        }
        --remaining;
        ++left;
        ++right;
    }

    return 0;
}

// 0x00417200 -> 0x0043A1D0 (.text, slot 0x018F502C)
void reloApplyPointerOffsets(const std::uint32_t* relocationTable, void* instanceBuffer) noexcept
{
    const auto baseAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instanceBuffer));
    for (const std::uint32_t* cursor = relocationTable; *cursor != 0xFFFFFFFFu; ++cursor) {
        const std::uint32_t siteOffset = *cursor;
        const std::uint32_t current = readU32(instanceBuffer, siteOffset);
        writeU32(instanceBuffer, siteOffset, current + baseAddress);
    }
}

// 0x00417380 -> 0x0043E450 (.text, slot 0x018F51BC)
void* ReloScratchBuffer::ensureCapacity(
    std::uint32_t requiredBytes,
    void* (*allocRaw)(std::uint32_t bytes, void* user),
    void (*freeRaw)(void* ptr, void* user),
    void* user) noexcept
{
    if (requiredBytes > capacity) {
        capacity = requiredBytes;
        freeRaw(data, user);
        data = allocRaw(capacity, user);
    }
    return data;
}

// 0x00418090 -> 0x0043B110 (.text, slot 0x018F6020)
ReloStreamHeader* ReloStreamHeader::clear() noexcept
{
    for (std::uint32_t& word : words) {
        word = 0;
    }
    return this;
}

// 0x00447740 -> 0x00491B00 (.text, slot 0x018F6F50)
bool assetByteSpanLess(const AssetByteSpan* lhs, const AssetByteSpan* rhs) noexcept
{
    const auto lhsLen = static_cast<std::int32_t>(lhs->end - lhs->begin);
    const auto rhsLen = static_cast<std::int32_t>(rhs->end - rhs->begin);
    const std::int32_t minLen = (rhsLen < lhsLen) ? rhsLen : lhsLen;

    const int cmp = assetMemoryCompare(
        lhs->begin,
        rhs->begin,
        static_cast<std::uint32_t>(minLen));
    if (cmp != 0) {
        return cmp < 0;
    }
    return lhsLen < rhsLen;
}

// ============================================================================
// Group 2: ReloClone_<Type> Nullable Struct/Field Cloners (20 functions)
// ============================================================================

// 0x00417550 -> 0x0047A780 (.text, slot 0x018F50E4)
void reloCloneCoord3D(
    const void* /*instanceBase*/,
    const void* srcField,
    void** dstPtrSlot,
    void* /*context*/,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneDirectBytes(srcField, dstPtrSlot, 0x0Cu, hooks);
}

// 0x004176B0 -> 0x00424730 (.text, slot 0x018F5104)
void reloCloneString(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x004174E0u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x0043BAD0 -> 0x0043FBC0 (.text, slot 0x018F50A0)
void reloCloneValue8(
    const void* /*instanceBase*/,
    const void* srcField,
    void** dstPtrSlot,
    void* /*context*/,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneDirectBytes(srcField, dstPtrSlot, 0x08u, hooks);
}

// 0x0043BF60 -> 0x00455D70 (.text, slot 0x018F6CC0)
void reloCloneObjectStatusBitFlags(
    const void* /*instanceBase*/,
    const void* srcField,
    void** dstPtrSlot,
    void* /*context*/,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneDirectBytes(srcField, dstPtrSlot, 0x1Cu, hooks);
}

// 0x0043BFB0 -> 0x0048F3F0 (.text, slot 0x018FA648)
void reloCloneValue60(
    const void* /*instanceBase*/,
    const void* srcField,
    void** dstPtrSlot,
    void* /*context*/,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneDirectBytes(srcField, dstPtrSlot, 0x3Cu, hooks);
}

// 0x0043D830 -> 0x0045EA50 (.text, slot 0x018F5FD8)
void reloCloneStruct60_419580(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00419580u, instanceBase, srcField, dstPtrSlot, context, 0x3Cu, hooks);
}

// 0x0043FC60 -> 0x00490490 (.text, slot 0x018F90D0)
void reloCloneStruct20_423860(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00423860u, instanceBase, srcField, dstPtrSlot, context, 0x14u, hooks);
}

// 0x004404A0 -> 0x0047BB20 (.text, slot 0x018FB410)
void reloCloneScalar32(
    const void* /*instanceBase*/,
    const void* srcField,
    void** dstPtrSlot,
    void* /*context*/,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneDirectBytes(srcField, dstPtrSlot, 0x04u, hooks);
}

// 0x004428C0 -> 0x0046DF20 (.text, slot 0x018FAF70)
void reloCloneStruct12_41D1E0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x0041D1E0u, instanceBase, srcField, dstPtrSlot, context, 0x0Cu, hooks);
}

// 0x004430E0 -> 0x0045CCA0 (.text, slot 0x018FAF78)
void reloCloneObjectStatusValidationDataType(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (srcField == nullptr) {
        *dstPtrSlot = nullptr;
        return;
    }
    void* newObj = hooks.allocTagged(0x38u, hooks.user);
    std::memset(newObj, 0, 0x38u);
    *dstPtrSlot = newObj;
    reloDetokenizeObjectStatusValidationDataType(
        instanceBase,
        static_cast<const ReloFieldDescriptor*>(srcField),
        newObj,
        context,
        hooks);
}

// 0x00445C10 -> 0x00490690 (.text, slot 0x018FA668)
void reloCloneStruct36_4323B0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x004323B0u, instanceBase, srcField, dstPtrSlot, context, 0x24u, hooks);
}

// 0x0045D9F0 -> 0x00450950 (.text, slot 0x018F97B0)
void reloCloneSoundOrEvaEvent(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00447E30u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x0045E570 -> 0x0040B970 (.text, slot 0x018FAFAC)
void reloCloneStruct44_44A230(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x0044A230u, instanceBase, srcField, dstPtrSlot, context, 0x2Cu, hooks);
}

// 0x0045F460 -> 0x004724A0 (.text, slot 0x018FAFBC)
void reloCloneStruct8_455B10(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00455B10u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x0045F5D0 -> 0x00446480 (.text, slot 0x018F8120)
void reloCloneStruct8_448070(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00448070u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x004617B0 -> 0x0047A6F0 (.text, slot 0x018FAFC8)
void reloCloneStruct8_45B590(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x0045B590u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x00478BA0 -> 0x0047AC00 (.text, slot 0x018FA6AC)
void reloCloneStruct24_467520(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x00467520u, instanceBase, srcField, dstPtrSlot, context, 0x18u, hooks);
}

// 0x004812D0 -> 0x0043BD10 (.text, slot 0x018F9A60)
void reloCloneStruct8_47EF30(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x0047EF30u, instanceBase, srcField, dstPtrSlot, context, 0x08u, hooks);
}

// 0x00495180 -> 0x004290B0 (.text, slot 0x018FAF28)
void reloCloneStruct88_4944E0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x004944E0u, instanceBase, srcField, dstPtrSlot, context, 0x58u, hooks);
}

// 0x004A9AB0 -> 0x0048FE70 (.text, slot 0x018FB100)
void reloCloneStruct40_4A7120(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    cloneWithExternalDetokenizer(0x004A7120u, instanceBase, srcField, dstPtrSlot, context, 0x28u, hooks);
}

// ============================================================================
// Group 3: ReloDetokenizeFields_<Type> Field-Descriptor Walkers (21 functions)
// ============================================================================

// 0x0041CF20 -> 0x0190C480 (.securom, slot 0x018F5FC0): ModuleData
void reloDetokenizeModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* /*context*/,
    const ReloRuntimeHooks& /*hooks*/) noexcept
{
    writeU32(dstStruct, 0x00, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x8CDC9898u) {
            if (d->typeHash == 0xBDF07965u) {
                writeU32(dstStruct, 0x04, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x19B042A0u) {
            if (d->typeHash == 0x23E650B8u) {
                writeU32(dstStruct, 0x00, readU32(fieldSrc));
            }
        }
    }
}

// 0x0041D0B0 -> 0x0046DC80 (.text, slot 0x018F5FE8): ObjectStatusValidationDataType
void reloDetokenizeObjectStatusValidationDataType(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* /*context*/,
    const ReloRuntimeHooks& /*hooks*/) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x239EB201u) {
            if (d->typeHash == 0xA7BD811Eu) {
                std::memcpy(bytePtr(dstStruct, 0x1C), fieldSrc, 28);
            }
        } else if (d->fieldId == 0x312281FDu) {
            if (d->typeHash == 0xA7BD811Eu) {
                std::memcpy(bytePtr(dstStruct, 0x00), fieldSrc, 28);
            }
        }
    }
}

// 0x0042C120 -> 0x01918630 (.securom, slot 0x018F9040): AttackAssistResponseBehaviorModuleData
void reloDetokenizeAttackAssistResponseBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xA50D729Au) {
                hooks.invokeExternalDetokenizer(0x0041D270u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xEC430B3Au) {
            if (d->typeHash == 0xA7BD811Eu) {
                std::memcpy(bytePtr(dstStruct, 0x08), fieldSrc, 28);
            }
        } else if (d->fieldId == 0x1848C549u) {
            if (d->typeHash == 0x1848C549u) {
                writeU32(dstStruct, 0x28, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x5E259CB2u) {
            if (d->typeHash == 0x763177A4u) {
                writeU32(dstStruct, 0x24, readU32(fieldSrc));
            }
        }
    }
}

// 0x0042C8B0 -> 0x00492CA0 (.text, slot 0x018F9080): QueueDockModuleData
void reloDetokenizeQueueDockModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0x30A36B8Du) {
                hooks.invokeExternalDetokenizer(0x00431F70u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xBC613612u) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x08), context, hooks.user);
            }
        } else if (d->fieldId == 0x3EFFF3BDu) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x10), context, hooks.user);
            }
        } else if (d->fieldId == 0xC17D6193u) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x18), context, hooks.user);
            }
        } else if (d->fieldId == 0x7D16400Eu) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x20), context, hooks.user);
            }
        } else if (d->fieldId == 0xD4A2D1ADu) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x28), context, hooks.user);
            }
        } else if (d->fieldId == 0x4BB8F560u) {
            if (d->typeHash == 0x15723E0Bu) {
                writeU32(dstStruct, 0x30, readU32(fieldSrc));
            }
        }
    }
}

// 0x0042CDC0 -> 0x0191BDE0 (.securom, slot 0x018F90BC): StructurePlacementBehaviorModuleData
void reloDetokenizeStructurePlacementBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xA50D729Au) {
                hooks.invokeExternalDetokenizer(0x0041D270u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xCAFDC008u) {
            if (d->typeHash == 0xE7210555u) {
                hooks.invokeExternalDetokenizer(0x00418D60u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x08), context, hooks.user);
            }
        }
    }
}

// 0x00448170 -> 0x0045CF90 (.text, slot 0x018F97B4): AudioVoiceEntry
void reloDetokenizeAudioVoiceEntry(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0x1AE6B6B7u) {
                hooks.invokeExternalDetokenizer(0x00447E30u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x88F12FFBu) {
            if (d->typeHash == 0x4DA708B0u) {
                writeU32(dstStruct, 0x08, readU32(fieldSrc));
            }
        }
    }
}

// 0x00454BF0 -> 0x004AF880 (.text, slot 0x018F991C): HighlanderBodyModuleData
void reloDetokenizeHighlanderBodyModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u && d->typeHash == 0x5A0E3D9Bu) {
            hooks.invokeExternalDetokenizer(0x00454690u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
        }
    }
}

// 0x00459520 -> 0x01907EB0 (.securom, slot 0x018F9974): LaserStateModuleData
void reloDetokenizeLaserStateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU32(dstStruct, 0x08, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xF1002505u) {
                hooks.invokeExternalDetokenizer(0x0042A360u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xC48AD38Eu) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x0C), context, hooks.user);
            }
        } else if (d->fieldId == 0xC58EA518u) {
            if (d->typeHash == 0x29368077u) {
                reloCloneCoord3D(
                    instanceBase,
                    fieldSrc,
                    reinterpret_cast<void**>(bytePtr(dstStruct, 0x24)),
                    context,
                    hooks);
            }
        } else if (d->fieldId == 0xEA73EC5Fu) {
            if (d->typeHash == 0x3E8F938Cu) {
                reloCloneObjectStatusValidationDataType(
                    instanceBase,
                    fieldSrc,
                    reinterpret_cast<void**>(bytePtr(dstStruct, 0x28)),
                    context,
                    hooks);
            }
        } else if (d->fieldId == 0x42A4EC4Fu) {
            if (d->typeHash == 0x23E650B8u) {
                writeU32(dstStruct, 0x08, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x7120BE4Eu) {
            if (d->typeHash == 0x43F1CE36u) {
                hooks.invokeExternalDetokenizer(0x00417B70u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x14), context, hooks.user);
            }
        } else if (d->fieldId == 0x7E5D901Au) {
            if (d->typeHash == 0x43F1CE36u) {
                hooks.invokeExternalDetokenizer(0x00417B70u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x1C), context, hooks.user);
            }
        }
    }
}

// 0x00472180 -> 0x01908CA0 (.securom, slot 0x018F99E8): SubObjectsUpgradeModuleData
void reloDetokenizeSubObjectsUpgradeModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xAE60AE98u) {
                hooks.invokeExternalDetokenizer(0x00453ED0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xBE41637Du) {
            if (d->typeHash == 0x7F67C232u) {
                hooks.invokeExternalDetokenizer(0x0045F4B0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x24), context, hooks.user);
            }
        }
    }
}

// 0x00473100 -> 0x00446370 (.text, slot 0x018F9A00): ShipSlowDeathBehaviorModuleData
void reloDetokenizeShipSlowDeathBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u && d->typeHash == 0x98769794u) {
            hooks.invokeExternalDetokenizer(0x0046FFE0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
        }
    }
}

// 0x0047C190 -> 0x01909340 (.securom, slot 0x018F9A1C): FXShaderMaterial
void reloDetokenizeFXShaderMaterial(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU8(dstStruct, 0x18, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0xDB4C0154u) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x4CC1CA67u) {
            if (d->typeHash == 0x1A2933AAu) {
                hooks.invokeExternalDetokenizer(0x004174E0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x08), context, hooks.user);
            }
        } else if (d->fieldId == 0x66B329ADu) {
            if (d->typeHash == 0x19842B2Fu) {
                hooks.invokeExternalDetokenizer(0x00476A50u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x10), context, hooks.user);
            }
        } else if (d->fieldId == 0x7A39D303u) {
            if (d->typeHash == 0x30916B96u) {
                writeU8(dstStruct, 0x18, readU8(fieldSrc));
            }
        }
    }
}

// 0x0048A430 -> 0x0044F800 (.text, slot 0x018F9A9C): GarrisonContainModuleData
void reloDetokenizeGarrisonContainModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU8(dstStruct, 0x9E, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xD7CC2E35u) {
                hooks.invokeExternalDetokenizer(0x004890D0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x201C212Bu) {
            if (d->typeHash == 0x201C212Bu) {
                hooks.invokeExternalDetokenizer(0x0042B1A0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x94), context, hooks.user);
            }
        } else if (d->fieldId == 0x5B9A8331u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x9C, readU8(fieldSrc));
            }
        } else if (d->fieldId == 0xB6CAC298u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x9D, readU8(fieldSrc));
            }
        } else if (d->fieldId == 0xFA3BFE5Cu) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x9E, readU8(fieldSrc));
            }
        }
    }
}

// 0x0048A520 -> 0x018FE520 (.securom, slot 0x018F9AA0): HordeGarrisonContainModuleData
void reloDetokenizeHordeGarrisonContainModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU32(dstStruct, 0xA0, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xBAE3BC7Du) {
                reloDetokenizeGarrisonContainModuleData(
                    instanceBase,
                    static_cast<const ReloFieldDescriptor*>(fieldSrc),
                    dstStruct,
                    context,
                    hooks);
            }
        } else if (d->fieldId == 0xF3D211A1u) {
            if (d->typeHash == 0x763177A4u) {
                writeU32(dstStruct, 0xA0, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x88D1B434u) {
            if (d->typeHash == 0x29368077u) {
                reloCloneCoord3D(
                    instanceBase,
                    fieldSrc,
                    reinterpret_cast<void**>(bytePtr(dstStruct, 0xA4)),
                    context,
                    hooks);
            }
        } else if (d->fieldId == 0x9019CE65u) {
            if (d->typeHash == 0x29368077u) {
                reloCloneCoord3D(
                    instanceBase,
                    fieldSrc,
                    reinterpret_cast<void**>(bytePtr(dstStruct, 0xA8)),
                    context,
                    hooks);
            }
        } else if (d->fieldId == 0x294B04ABu) {
            if (d->typeHash == 0x29368077u) {
                reloCloneCoord3D(
                    instanceBase,
                    fieldSrc,
                    reinterpret_cast<void**>(bytePtr(dstStruct, 0xAC)),
                    context,
                    hooks);
            }
        }
    }
}

// 0x0048B940 -> 0x00487B30 (.text, slot 0x018F9AAC): CaptureSpecialAbilityUpdateModuleData
void reloDetokenizeCaptureSpecialAbilityUpdateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u && d->typeHash == 0xAC598F82u) {
            hooks.invokeExternalDetokenizer(0x0048AB00u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
        }
    }
}

// 0x00493870 -> 0x01903AA0 (.securom, slot 0x018F9AE8): W3DTankDrawModuleData
void reloDetokenizeW3DTankDrawModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0x4ECF2A4Bu) {
                hooks.invokeExternalDetokenizer(0x00492EA0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x2A350DD4u) {
            if (d->typeHash == 0xA6FF4551u) {
                hooks.invokeExternalDetokenizer(0x00442ED0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0xD8), context, hooks.user);
            }
        } else if (d->fieldId == 0xC4037FCBu) {
            if (d->typeHash == 0xC4037FCBu) {
                hooks.invokeExternalDetokenizer(0x00442F20u, instanceBase, fieldSrc, bytePtr(dstStruct, 0xDC), context, hooks.user);
            }
        }
    }
}

// 0x00494430 -> 0x019104B0 (.securom, slot 0x018F9B00): ToggleStatusSpecialAbilityUpdateModuleData
void reloDetokenizeToggleStatusSpecialAbilityUpdateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU8(dstStruct, 0xF8, 1);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xAC598F82u) {
                hooks.invokeExternalDetokenizer(0x0048AB00u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x07DAB6F7u) {
            if (d->typeHash == 0xFC294B1Du) {
                hooks.invokeExternalDetokenizer(0x00490830u, instanceBase, fieldSrc, bytePtr(dstStruct, 0xF0), context, hooks.user);
            }
        } else if (d->fieldId == 0xB3C22701u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0xF8, readU8(fieldSrc));
            }
        }
    }
}

// 0x004A8280 -> 0x0044D880 (.text, slot 0x018FB078): RecallUnitsSpecialPowerModuleData
void reloDetokenizeRecallUnitsSpecialPowerModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU8(dstStruct, 0x1C4, 1);
    writeU32(dstStruct, 0x1C0, 4);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xCEFE1113u) {
                hooks.invokeExternalDetokenizer(0x004A71D0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xC1BAA310u) {
            if (d->typeHash == 0x23E650B8u) {
                writeU32(dstStruct, 0x1C0, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x995C7C88u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x1C4, readU8(fieldSrc));
            }
        }
    }
}

// 0x004A8A50 -> 0x00440590 (.text, slot 0x018FB0A4): AttackTargetSpecialPowerModuleData
void reloDetokenizeAttackTargetSpecialPowerModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u && d->typeHash == 0xCEFE1113u) {
            hooks.invokeExternalDetokenizer(0x004A71D0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
        }
    }
}

// 0x004A8EA0 -> 0x01912C20 (.securom, slot 0x018FB0D0): ActivateModuleSpecialPower
void reloDetokenizeActivateModuleSpecialPower(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0xCEFE1113u) {
                hooks.invokeExternalDetokenizer(0x004A71D0u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0x6C48FC6Eu) {
            if (d->typeHash == 0x58F6065Cu) {
                hooks.invokeExternalDetokenizer(0x004475F0u, instanceBase, fieldSrc, bytePtr(dstStruct, 0x1C0), context, hooks.user);
            }
        }
    }
}

// 0x004A94E0 -> 0x0045DC60 (.text, slot 0x018FB0EC): MoneyCrateCollideModuleData
void reloDetokenizeMoneyCrateCollideModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU32(dstStruct, 0x6C, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0x2027962Au) {
                hooks.invokeExternalDetokenizer(0x004A8F30u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xAB6ABBF0u) {
            if (d->typeHash == 0x763177A4u) {
                writeU32(dstStruct, 0x6C, readU32(fieldSrc));
            }
        }
    }
}

// 0x004A9600 -> 0x0049FDA0 (.text, slot 0x018FB0F4): VeterancyCrateCollideModuleData
void reloDetokenizeVeterancyCrateCollideModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept
{
    writeU32(dstStruct, 0x6C, 0);
    writeU8(dstStruct, 0x74, 0);
    writeU8(dstStruct, 0x75, 0);
    writeU32(dstStruct, 0x70, 0);
    if (fields == nullptr) {
        return;
    }
    for (const ReloFieldDescriptor* d = fields; d->typeHash != 0 || d->fieldId != 0; ++d) {
        const void* fieldSrc = resolveFieldSource(instanceBase, d->offset);
        if (d->fieldId == 0x00000001u) {
            if (d->typeHash == 0x2027962Au) {
                hooks.invokeExternalDetokenizer(0x004A8F30u, instanceBase, fieldSrc, dstStruct, context, hooks.user);
            }
        } else if (d->fieldId == 0xA249AAF4u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x74, readU8(fieldSrc));
            }
        } else if (d->fieldId == 0xA50E58A0u) {
            if (d->typeHash == 0x77E3F4F7u) {
                writeU8(dstStruct, 0x75, readU8(fieldSrc));
            }
        } else if (d->fieldId == 0x62D48EA3u) {
            if (d->typeHash == 0x763177A4u) {
                writeU32(dstStruct, 0x6C, readU32(fieldSrc));
            }
        } else if (d->fieldId == 0x3E4F1C74u) {
            if (d->typeHash == 0x23E650B8u) {
                writeU32(dstStruct, 0x70, readU32(fieldSrc));
            }
        }
    }
}

}  // namespace ra3::asset
