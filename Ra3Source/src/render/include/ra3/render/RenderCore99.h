#pragma once

#include <cstdint>

namespace ra3::render {

// PartitionManager (vtable 0x00C5D6C0, secondary 0x00C5D6D0 stored by 0x00990F20).
void removePartitionObject(void* partition, void* object) noexcept;
void clearPartitionBuckets(void* partition) noexcept;
void setPartitionWorldBounds(void* partition, const float* bounds6) noexcept;
void resetPartitionBounds(void* partition) noexcept;

// CollisionManager (vtable 0x00C5D764, secondary 0x00C5D774 stored by 0x00994300).
void unlinkCollisionEntry(void* entry) noexcept;
void sweepIdleCollisionNodes(void* collision) noexcept;
void retireCollisionHashNode(void* table, const std::uint32_t* key) noexcept;
void retireCollisionObjectList(void* collision, void* owner) noexcept;
void updateCollisionRegistration(void* collision, void* drawable) noexcept;

// Shader parameter records (0x48 bytes) and the rigid transforms that feed them.
void matrix3x3ToQuaternion(float* quaternion, const float* matrix) noexcept;
void* fillShaderParamRange(void* first, void* last, const void* source) noexcept;
void* copyShaderParamsForward(const void* first, const void* last, void* dest) noexcept;
void* copyShaderParamsBackward(const void* first, const void* last, void* destEnd) noexcept;
void* defaultShaderParam(void* param) noexcept;
void* assignShaderParamRelaxed(void* param, const void* source) noexcept;
void readNestedNameWord(const void* shader, std::uint32_t* out) noexcept;
void* assignRefCountedPointer(void* destination, const void* source) noexcept;
void copySubObjectRecords(void** cursor, const void* first, const void* last, void* dest) noexcept;
void* copyExtendedRecords(void** cursor, const void* first, const void* last, void* dest) noexcept;
void* assignShaderParam(void* param, const void* source) noexcept;
void* constructShaderParamRange(void** cursor, const void* first, const void* last, void* dest) noexcept;
void* copyExtendedRecordsBackward(const void* first, const void* last, void* destEnd) noexcept;
void composeShaderTransform(const float* left, const float* right, float* out) noexcept;
void composeShaderTransformInPlace(void* transform, const float* right) noexcept;

// Globals 0x00E1A588/0x00E1A58C (record span), 0x00E1B314 (cached key), 0x00E1B318 (ready flag).
struct ShaderRegistryView {
    const std::uint8_t* begin = nullptr;
    const std::uint8_t* end = nullptr;
    std::uint32_t cacheKey = 0;
    std::uint8_t ready = 0;
    // When non-zero, lookup of 0x00C58C18 returns this instead of 0x42.
    std::uint32_t secondConstantId = 0x42;
    // 0 -> constant query misses; otherwise the query returns this pointer value's stand-in.
    std::uint8_t* constantQueryHit = nullptr;
};
void setShaderRegistryView(const ShaderRegistryView& view) noexcept;
void* resolveRegisteredShader(std::uint32_t* out, const std::uint32_t* key) noexcept;
void bindShaderConstantIds(void* shader) noexcept;

void* replicateShaderParam(void* dest, std::uint32_t count, const void* source) noexcept;
void* moveSubObjectRecordsBackward(const void* first, const void* last, void* destEnd) noexcept;
void* findWideCollisionNode(void* table, const std::uint32_t* key) noexcept;

// 0x00E1B320 ready bit and 0x00E1B31C cached semantic-string pointer.
struct SemanticNameCache {
    std::uint8_t* text = nullptr;
    bool ready = false;
};
void setSemanticNameCache(const SemanticNameCache& cache) noexcept;
std::uint32_t shaderSemanticNames() noexcept;

void refreshShaderParamChain(void* block, const float* matrix) noexcept;
void skinShaderInstances(void* block) noexcept;
bool bindShaderAsset(void* asset, const std::uint32_t* key) noexcept;
std::uint32_t resizeShaderParamVector(void* vec, std::uint32_t count) noexcept;
void* importShaderParamBlock(void* block, const void* descriptor) noexcept;
void* appendSubObjectRecord(void* vec, const void* record) noexcept;

// Console commands. Slot 2 of 0x00C5DA5C / 0x00C5DAAC is the command description.
bool runVisibleObjectsCommand(void* console, void* parsed, std::uint32_t, std::uint32_t) noexcept;
bool parseVisibleObjectsCommand(void* console, void* parsed, void* tokens) noexcept;

// Engine services replaced on both sides of the oracle (allocator / bin lookup).
struct RenderBinServices {
    std::uint8_t* (*service)() = nullptr;
    std::uint8_t* (*allocate)(void* service, std::uint32_t a, std::uint32_t b, std::uint32_t c) = nullptr;
    void (*reserve)(void* vec, std::uint32_t, std::uint32_t) = nullptr;
    void (*submit)(void* console, std::uint32_t, void* text, std::uint32_t, std::uint32_t) = nullptr;
    bool (*matchBin)(void* console, void* name, const char* literal) = nullptr;
    void (*select)(void* obj, std::uint32_t, std::uint32_t) = nullptr;
};
void setRenderBinServices(const RenderBinServices& services) noexcept;
bool runRenderBinCommand(void* console, void* parsed, void* args, void* nameSlot) noexcept;
bool parseRenderBinCommand(void* console, void* parsed, void* tokens) noexcept;

}  // namespace ra3::render
