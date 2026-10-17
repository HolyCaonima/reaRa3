#pragma once

// Reconstructed binary-asset relocation (.relo) and ReloDetokenizers runtime routines.
//
// Binary anchors:
// - RtsXmlPipeline\0.0-dev\include\RtsXmlPipeline\ReloDetokenizers.h
//   (assert string at 0x00BCACA8, referenced by 0x0043C630, 0x0045E9A0, 0x00477510, 0x00477A50)
// - c:\bf_qabuild_ra3_eala-build108\ra3\code\libraries\source\assetmanager\resourcemanager.cpp
//   (assert string at 0x00BC8F00, referenced by 0x00417220 right after 0x00417200)
// - Ra3Source/docs/design/asset_layout.md (§2.1 relocation fixup loop 0x00417200 -> 0x0043A1D0)
//
// Each entry-lost function below was recovered from its runtime body in the run35 capture
// (snapshot_3_240s) via securom_sites.tsv (resolution=body, function_securom=entry-lost).

#include <cstddef>
#include <cstdint>

namespace ra3::asset {

// 12-byte field descriptor entry in a binary asset's ReloDetokenizers field table.
// Terminated by a sentinel entry with fieldId == 0 && typeHash == 0.
struct ReloFieldDescriptor {
    std::uint32_t fieldId = 0;
    std::uint32_t typeHash = 0;
    std::uint32_t offset = 0;
};
static_assert(sizeof(ReloFieldDescriptor) == 12, "ReloFieldDescriptor must be 12 bytes");

// 8-byte growable scratch buffer used by the asset stream loader (0x00417380 -> 0x0043E450).
struct ReloScratchBuffer {
    void* data = nullptr;
    std::uint32_t capacity = 0;

    // 0x00417380 -> 0x0043E450 (.text, slot 0x018F51BC)
    void* ensureCapacity(
        std::uint32_t requiredBytes,
        void* (*allocRaw)(std::uint32_t bytes, void* user),
        void (*freeRaw)(void* ptr, void* user),
        void* user = nullptr) noexcept;
};
static_assert(sizeof(ReloScratchBuffer) == 8, "ReloScratchBuffer must be 8 bytes");

// 32-byte asset stream header state cleared by 0x00418090 -> 0x0043B110 (.text, slot 0x018F6020).
struct ReloStreamHeader {
    std::uint32_t words[8]{};

    // 0x00418090 -> 0x0043B110 (.text, slot 0x018F6020)
    ReloStreamHeader* clear() noexcept;
};
static_assert(sizeof(ReloStreamHeader) == 32, "ReloStreamHeader must be 32 bytes");

// 8-byte [begin, end) byte span compared by 0x00447740 -> 0x00491B00 (.text, slot 0x018F6F50).
struct AssetByteSpan {
    const std::uint8_t* begin = nullptr;
    const std::uint8_t* end = nullptr;
};
static_assert(sizeof(AssetByteSpan) == 8, "AssetByteSpan must be 8 bytes");

// Callback hooks used when executing ReloClone_<T> and ReloDetokenizeFields_<T> routines
// in standalone verification or host environments.
struct ReloRuntimeHooks {
    // 0x004165A0: tagged operator new(size, flags, tag)
    void* (*allocTagged)(std::uint32_t size, void* user) = nullptr;
    // External leaf/base detokenizer callback:
    // void callee(const void* instanceBase, const void* srcField, void* dstField, void* context)
    void (*invokeExternalDetokenizer)(
        std::uint32_t calleeVa,
        const void* instanceBase,
        const void* srcField,
        void* dstField,
        void* context,
        void* user) = nullptr;
    void* user = nullptr;
};

// ============================================================================
// Group 1: Core .relo & Asset Memory/Buffer/Span Primitives (5 functions)
// ============================================================================

// 0x00416ED0 -> 0x004416E0 (.text, slot 0x018FB790)
// Word-first, byte-fallback memory comparison returning -1, 0, or +1.
int assetMemoryCompare(const void* lhs, const void* rhs, std::uint32_t byteCount) noexcept;

// 0x00417200 -> 0x0043A1D0 (.text, slot 0x018F502C)
// In-place .relo pointer fixup loop terminated by 0xFFFFFFFFu.
void reloApplyPointerOffsets(const std::uint32_t* relocationTable, void* instanceBuffer) noexcept;

// 0x00447740 -> 0x00491B00 (.text, slot 0x018F6F50)
// Lexicographical '<' comparator over two AssetByteSpan ranges using 0x00416ED0.
bool assetByteSpanLess(const AssetByteSpan* lhs, const AssetByteSpan* rhs) noexcept;

// ============================================================================
// Group 2: ReloClone_<Type> Nullable Struct/Field Cloners (20 functions)
// ============================================================================

// 0x00417550 -> 0x0047A780 (.text, slot 0x018F6724): 12-byte Coord3D/Vector3 direct cloner
void reloCloneCoord3D(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004176B0 -> 0x00424730 (.text, slot 0x018FA7A4): 8-byte String cloner (calls 0x004174E0)
void reloCloneString(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0043BAD0 -> 0x0043FBC0 (.text, slot 0x018FB8A0): 8-byte value direct cloner
void reloCloneValue8(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0043BF60 -> 0x00455D70 (.text, slot 0x018F5A18): 28-byte ObjectStatusBitFlags direct cloner
void reloCloneObjectStatusBitFlags(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0043BFB0 -> 0x0048F3F0 (.text, slot 0x018F6E64): 60-byte value direct cloner
void reloCloneValue60(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0043D830 -> 0x0045EA50 (.text, slot 0x018F5BF4): 60-byte struct cloner (calls 0x00419580)
void reloCloneStruct60_419580(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0043FC60 -> 0x00490490 (.text, slot 0x018F6EB0): 20-byte struct cloner (calls 0x00423860)
void reloCloneStruct20_423860(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004404A0 -> 0x0047BB20 (.text, slot 0x018F6FE0): 4-byte scalar direct cloner
void reloCloneScalar32(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004428C0 -> 0x0046DF20 (.text, slot 0x018F6100): 12-byte struct cloner (calls 0x0041D1E0)
void reloCloneStruct12_41D1E0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004430E0 -> 0x0045CCA0 (.text, slot 0x018F57DC): 56-byte ObjectStatusValidationDataType cloner (calls 0x0041D0B0)
void reloCloneObjectStatusValidationDataType(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00445C10 -> 0x00490690 (.text, slot 0x018F6F14): 36-byte struct cloner (calls 0x004323B0)
void reloCloneStruct36_4323B0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0045D9F0 -> 0x00450950 (.text, slot 0x018F55B4): 8-byte SoundOrEvaEvent cloner (calls 0x00447E30)
void reloCloneSoundOrEvaEvent(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0045E570 -> 0x0040B970 (.text, slot 0x018FA2F8): 44-byte struct cloner (calls 0x0044A230)
void reloCloneStruct44_44A230(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0045F460 -> 0x004724A0 (.text, slot 0x018F6224): 8-byte struct cloner (calls 0x00455B10)
void reloCloneStruct8_455B10(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0045F5D0 -> 0x00446480 (.text, slot 0x018F511C): 8-byte struct cloner (calls 0x00448070)
void reloCloneStruct8_448070(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004617B0 -> 0x0047A6F0 (.text, slot 0x018F666C): 8-byte struct cloner (calls 0x0045B590)
void reloCloneStruct8_45B590(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00478BA0 -> 0x0047AC00 (.text, slot 0x018F6584): 24-byte struct cloner (calls 0x00467520)
void reloCloneStruct24_467520(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004812D0 -> 0x0043BD10 (.text, slot 0x018FB6DC): 8-byte struct cloner (calls 0x0047EF30)
void reloCloneStruct8_47EF30(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00495180 -> 0x004290B0 (.text, slot 0x018FA8B0): 88-byte struct cloner (calls 0x004944E0)
void reloCloneStruct88_4944E0(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A9AB0 -> 0x0048FE70 (.text, slot 0x018F6BCC): 40-byte struct cloner (calls 0x004A7120)
void reloCloneStruct40_4A7120(
    const void* instanceBase,
    const void* srcField,
    void** dstPtrSlot,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// ============================================================================
// Group 3: ReloDetokenizeFields_<Type> Field-Descriptor Walkers (21 functions)
// ============================================================================

// 0x0041CF20 -> 0x0190C480 (.securom, slot 0x018F556C): ModuleData (BaseModules.xsd)
void reloDetokenizeModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0041D0B0 -> 0x0046DC80 (.text, slot 0x018FB250): ObjectStatusValidationDataType (BaseModules.xsd)
void reloDetokenizeObjectStatusValidationDataType(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0042C120 -> 0x01918630 (.securom, slot 0x018F62C0): AttackAssistResponseBehaviorModuleData
void reloDetokenizeAttackAssistResponseBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0042C8B0 -> 0x00492CA0 (.text, slot 0x018FAA0C): QueueDockModuleData
void reloDetokenizeQueueDockModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0042CDC0 -> 0x0191BDE0 (.securom, slot 0x018F6710): StructurePlacementBehaviorModuleData
void reloDetokenizeStructurePlacementBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00448170 -> 0x0045CF90 (.text, slot 0x018F594C): AudioVoiceEntry
void reloDetokenizeAudioVoiceEntry(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00454BF0 -> 0x004AF880 (.text, slot 0x018F7350): HighlanderBodyModuleData
void reloDetokenizeHighlanderBodyModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00459520 -> 0x01907EB0 (.securom, slot 0x018F5010): LaserStateModuleData
void reloDetokenizeLaserStateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00472180 -> 0x01908CA0 (.securom, slot 0x018F5114): SubObjectsUpgradeModuleData
void reloDetokenizeSubObjectsUpgradeModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00473100 -> 0x00446370 (.text, slot 0x018F5104): ShipSlowDeathBehaviorModuleData
void reloDetokenizeShipSlowDeathBehaviorModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0047C190 -> 0x01909340 (.securom, slot 0x018F5184): FXShaderMaterial
void reloDetokenizeFXShaderMaterial(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0048A430 -> 0x0044F800 (.text, slot 0x018FA7A8): GarrisonContainModuleData
void reloDetokenizeGarrisonContainModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0048A520 -> 0x018FE520 (.securom, slot 0x018FB9E0): HordeGarrisonContainModuleData
void reloDetokenizeHordeGarrisonContainModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x0048B940 -> 0x00487B30 (.text, slot 0x018F6E88): CaptureSpecialAbilityUpdateModuleData
void reloDetokenizeCaptureSpecialAbilityUpdateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00493870 -> 0x01903AA0 (.securom, slot 0x018FBFF8): W3DTankDrawModuleData
void reloDetokenizeW3DTankDrawModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x00494430 -> 0x019104B0 (.securom, slot 0x018F591C): ToggleStatusSpecialAbilityUpdateModuleData
void reloDetokenizeToggleStatusSpecialAbilityUpdateModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A8280 -> 0x0044D880 (.text, slot 0x018FAB2C): RecallUnitsSpecialPowerModuleData
void reloDetokenizeRecallUnitsSpecialPowerModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A8A50 -> 0x00440590 (.text, slot 0x018FBB14): AttackTargetSpecialPowerModuleData
void reloDetokenizeAttackTargetSpecialPowerModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A8EA0 -> 0x01912C20 (.securom, slot 0x018F5C00): ActivateModuleSpecialPower
void reloDetokenizeActivateModuleSpecialPower(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A94E0 -> 0x0045DC60 (.text, slot 0x018F5B08): MoneyCrateCollideModuleData
void reloDetokenizeMoneyCrateCollideModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

// 0x004A9600 -> 0x0049FDA0 (.text, slot 0x018FAF74): VeterancyCrateCollideModuleData
void reloDetokenizeVeterancyCrateCollideModuleData(
    const void* instanceBase,
    const ReloFieldDescriptor* fields,
    void* dstStruct,
    void* context,
    const ReloRuntimeHooks& hooks) noexcept;

}  // namespace ra3::asset
