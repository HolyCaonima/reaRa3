#pragma once

#include <cstddef>
#include <cstdint>
#include <ra3/render/W3DDrawable.h>

namespace ra3::render {

// 0x008E52A0 - W3DDynamicsDraw_pri[59]
void setDynamicsDrawWord2FC(void* draw, std::uint32_t value) noexcept;

// 0x008E5690 - W3DScriptedModelDraw_pri[17]
void releaseScriptedModelTerrainDecal(void* draw) noexcept;

// 0x008E57E0 - W3DScriptedModelDraw animation trigger frame crossing predicate
bool animTriggerCrossedFrame(const void* animState, std::int32_t triggerFrame) noexcept;

// 0x008E5A60 - W3DLaserDraw[3] / W3DPropDraw[3] / W3DStreamDraw[3] / W3DTracerDraw[3]
std::uint8_t notifySimpleDrawBoundToObject(void* draw, void* drawable) noexcept;

// 0x008E5E80 - ParticleSystem refcounted pointer slot assignment (+0x24 refCount, +0x20 vtable)
void* assignParticleSysRef(void* dstSlot, const void* srcSlot) noexcept;

// 0x008E6060 - 8-byte ParticleAttachRecord assignment
void* assignParticleAttachRecord8(void* dstRecord, const void* srcRecord) noexcept;

// 0x008E6180 - Forward range assignment of 8-byte ParticleAttachRecord
void* copyParticleAttachRecords8Forward(const void* first, const void* last, void* dst) noexcept;

// 0x008E6260 - Backward range assignment of 8-byte ParticleAttachRecord
void* copyParticleAttachRecords8Backward(const void* first, const void* last, void* dstLast) noexcept;

// 0x008E8550 - W3DTruckDraw extra rotation reset and notification
void resetTruckExtraRotationIfNonZero(void* truckDraw) noexcept;

// 0x008E8900 - Query first animation name from current ModelConditionState
void* getFirstAnimationName(const void* drawStateSlot, void* outAsciiString) noexcept;

// 0x008E8960 - Query modulo-indexed animation name from current ModelConditionState
void* getIndexedAnimationName(
    const void* drawStateSlot, void* outAsciiString, std::uint32_t index) noexcept;

// 0x008E8D50 - Query whether a W3DScriptedModelDraw animation layer has completed
bool isAnimationLayerComplete(
    const void* drawStateSlot, const void* animLayer, std::uint8_t checkCompleteFlag) noexcept;

// 0x008E8DC0 - W3DScriptedModelDraw_pri[5]
void* getScriptedModelModuleKeyOrOverride(const void* draw, std::uint32_t* outKey) noexcept;

// 0x008E8E90 - W3DScriptedModelDraw_pri[23]
void updatePrimaryShadowTransform(
    void* draw, std::uint32_t firstArg, std::uint32_t secondArg) noexcept;

// 0x008E8EC0 - W3DScriptedModelDraw_pri[24]
void updatePrimaryShadowVisibility(void* draw, std::uint32_t visibleArg) noexcept;

// 0x008E8FD0 - W3DScriptedModelDraw_pri[26]
void releasePrimaryShadow(void* draw) noexcept;

// 0x008E9000 - W3DScriptedModelDraw_pri[29]
void updateAuxShadowModule200(void* draw) noexcept;

// 0x008E9020 - W3DScriptedModelDraw_pri[30]
void updateSecondaryShadowTransform(
    void* draw, std::uint32_t firstArg, std::uint32_t secondArg) noexcept;

// 0x008E9050 - W3DScriptedModelDraw_pri[31]
void updateSecondaryShadowVisibility(void* draw, std::uint32_t visibleArg) noexcept;

// 0x008E9240 - W3DScriptedModelDraw_pri[34]
void releaseSecondaryShadow(void* draw) noexcept;

// 0x008E9270 - W3DScriptedModelDraw_pri[35]
void releaseAuxShadowModule200(void* draw) noexcept;

// 0x008E93B0 - W3DScriptedModelDraw_sec[15]
void applyScriptedModelHouseColor(void* drawInterface, const PassMask* mask) noexcept;

// 0x008EB120 - Uninitialized copy of 8-byte ParticleAttachRecord range
void uninitializedCopyParticleAttachRecords8(
    void** outEnd, const void* first, const void* last, void* dst) noexcept;

// 0x008EB360 - Uninitialized copy of 28-byte SubObjectRecord range
void uninitializedCopySubObjectRecords28(
    void** outEnd, const void* first, const void* last, void* dst) noexcept;

// 0x008EB490 - Uninitialized copy of 80-byte HermiteSplineSegment range
void** uninitializedCopySplineRecords80(
    void** outEnd, const void* first, const void* last, void* dst) noexcept;

// 0x008EB4E0 - Copy 56-byte BoneTransformRecord using x87 float transfers
void copyBoneTransformRecord56(void* dstRecord, const void* srcRecord) noexcept;

// 0x008EB540 - Uninitialized copy of 68-byte RenderRecord68 range
void** uninitializedCopyRenderRecords68(
    void** outEnd, const void* first, const void* last, void* dst) noexcept;

// 0x008EB5E0 - Backward copy of 204-byte RenderState204 range
void* copyRenderStates204Backward(const void* first, const void* last, void* dstLast) noexcept;

// 0x008EC560 - Retain refcounted RenderObjClass pointer at +4
void retainSubRenderObjectAt4(const void* holder, void** outRenderObj) noexcept;

// 0x008ED690 - Find animation bone info in ModelConditionState RB-tree (+0x4C)
void* findAnimationBoneInfoByKey(
    void* conditionState, std::int32_t key, std::uint32_t* outCount) noexcept;

// 0x008EDA20 - Propagate sub-object visibility across child bones in compound RenderObj
void propagateSubObjectBoneVisibility(
    std::uint32_t hideFlag, std::int32_t parentBoneIndex, void* compoundRenderObj) noexcept;

// 0x008EDB30 - Apply named sub-object visibility and propagate to child bones
void applyNamedSubObjectVisibility(
    const void* subObjEntry, void* compoundRenderObj, std::uint32_t hideFlag) noexcept;

// 0x008EE860 - Apply clamped opacity to sub-object, mark translucency, and release sub-object
void applySubObjectOpacityAndRelease(
    void* draw, void* subObjectRenderObj, float opacity) noexcept;

// 0x008EE8E0 - Apply non-fading sub-object hide state, propagate to children, and release
void applySubObjectHideState(
    void* draw, const void* hideEntry, void* subObjectRenderObj, std::int32_t subObjIndex) noexcept;

// 0x008EE990 - Update fading sub-object opacity/delay, propagate to children, and release
void updateSubObjectFadeAndVisibility(
    void* draw, void* fadeEntry, void* subObjectRenderObj, std::int32_t subObjIndex) noexcept;

// 0x008EEED0 - Reset W3DScriptedModelDraw animation layer slot
void resetAnimationLayerSlot(void* draw, std::int32_t layerIndex) noexcept;

// 0x008EF1A0 - Initialize 80-byte HermiteSplineSegment between two 3D points
void* initializeHermiteSplineSegment80(
    void* segment, const float* startPoint, const float* endPoint,
    float durationDivisor, float midpointZScale) noexcept;

// 0x008F0A90 - Destroy range of 8-byte ParticleAttachRecord elements
void destroyParticleAttachRecords8(void* first, void* last) noexcept;

// 0x008F0B30 - Construct 56-byte BoneTransformRecord from key and 48-byte matrix+flags
void* constructBoneTransformRecord56(
    void* dstRecord, const std::uint32_t* keyPtr, const void* matrixAndFlags) noexcept;

// 0x008F0B90 - Equal-range query on integer-keyed RB-tree
void** rbTreeEqualRangeIntKey(void* tree, void** outPair, const std::int32_t* keyPtr) noexcept;

// 0x008F0DF0 - Uninitialized copy of 12-byte AnimHandleRecord range
void uninitializedCopyAnimHandleRecords12(
    void** outEnd, const void* first, const void* last, void* dst) noexcept;

// 0x008F0F30 - Uninitialized fill of 68-byte RenderRecord68 elements
void uninitializedFillRenderRecords68(
    void* dst, std::uint32_t count, const void* value) noexcept;

// 0x008F1010 - Uninitialized fill of 8-byte RefCountPairRecord elements
void uninitializedFillRefCountPairRecords8(
    void* dst, std::uint32_t count, const void* value) noexcept;

// 0x008F1120 - Fill range of 68-byte RenderRecord68 elements
void fillRenderRecords68Range(void* first, void* last, const void* value) noexcept;

// 0x008F2700 - Stop all attached particle systems in pair vector
void stopAttachedParticleSystemsInVector(const void* pairVector) noexcept;

// 0x008F2C10 - Update start/stop state of attached particle systems in linked list
void updateAttachedParticleSystemsVisibility(void* draw) noexcept;

// 0x008F3D60 - W3DScriptedModelDraw_sec[23] updateSubObjects
void updateScriptedModelSubObjects(void* drawInterface) noexcept;

// 0x008F3EA0 - Assign W3DScriptedModelDraw animation layer slot
void assignAnimationLayerSlot(
    void* draw, std::int32_t layerIndex, void* animObj, std::uint8_t loopFlag,
    std::uint32_t animMode, float frameStep, float currentFrame, float blendFactor,
    std::uint8_t activeFlag, std::uint32_t direction) noexcept;

// 0x008F3FA0 - Move W3DScriptedModelDraw animation layer slot to another index
void moveAnimationLayerSlot(
    void* draw, std::int32_t dstLayerIndex, std::int32_t srcLayerIndex) noexcept;

// 0x008F5CB0 - Count matching integer keys in RB-tree
std::uint32_t rbTreeCountIntKey(void* tree, const std::int32_t* keyPtr) noexcept;

// 0x008F6180 - Forward copy of 28-byte SubObjectRecord range
void* copySubObjectRecords28Forward(const void* first, const void* last, void* dst) noexcept;

// 0x008F61B0 - Destroy range of 12-byte AnimHandleRecord elements
void destroyAnimHandleRecords12(void* first, void* last) noexcept;

// 0x008F6690 - Backward copy of 28-byte SubObjectRecord range
void* copySubObjectRecords28Backward(
    const void* first, const void* last, void* dstLast) noexcept;

// 0x008F6900 - Forward copy-assignment of 12-byte AnimHandleRecord range
void* copyAnimHandleRecords12Forward(const void* first, const void* last, void* dst) noexcept;

// 0x008F6C00 - Backward copy-assignment of 12-byte AnimHandleRecord range
void* copyAnimHandleRecords12Backward(
    const void* first, const void* last, void* dstLast) noexcept;

// 0x008F6ED0 - W3DScriptedModelDraw_sec[18] query sub-object bone count for state
std::uint32_t getSubObjectBoneCountForState(
    void* drawInterface, std::int32_t innerKey, std::int32_t outerKey) noexcept;

// 0x008F95D0 - Erase range from 28-byte SubObjectRecord vector
void* eraseSubObjectRecords28Range(
    void* vectorThis, void* eraseFirst, void* eraseLast) noexcept;

// 0x008FA6D0 - Fill range of 8-byte RefCountPairRecord elements
void fillRefCountPairRecords8Range(void* first, void* last, const void* value) noexcept;

// 0x008FA790 - Forward copy-assignment of 8-byte RefCountPairRecord range
void* copyRefCountPairRecords8Forward(const void* first, const void* last, void* dst) noexcept;

// 0x008FAB20 - Initialize 60-byte ModelConditionAnimationEntry
void* initializeModelConditionAnimationEntry(void* entry) noexcept;

// 0x00907AB0 - Initialize 136-byte W3DModelDrawConditionState
void* initializeW3DModelDrawConditionState(void* state) noexcept;

// 0x00910E50 - W3DScriptedModelDraw_pri[12]
void setScriptedModelFlag6D(void* draw, std::uint8_t value) noexcept;

// 0x00910E60 - W3DScriptedModelDraw_pri[57]
std::uint8_t getScriptedModelDataFlagD3(const void* draw) noexcept;

// 0x00910E70 - W3DScriptedModelDraw_pri[22]
bool isScriptedModelActive6DOrDataD4(const void* draw) noexcept;

// 0x00910E90 - W3DScriptedModelDraw_pri[20]
void setScriptedModelFlag6C(void* draw, std::uint8_t value) noexcept;

// 0x00910EB0 - W3DScriptedModelDraw_pri[56]
void setScriptedModelFlag216(void* draw, std::uint8_t value) noexcept;

// 0x00910EC0 - W3DScriptedModelDraw_pri[44..45]
void* getScriptedModelDrawInterface(void* draw) noexcept;

// 0x00910ED0 - W3DScriptedModelDraw_pri[10..11]
void* getScriptedModelSubInterface18(void* draw) noexcept;

// 0x00910EE0 - W3DScriptedModelDraw_pri[55]
void setScriptedModelFlag215(void* draw, std::uint8_t value) noexcept;

// 0x00911250 - W3DDynamicsDraw_pri[48..49]
void* getDynamicsDrawSubInterface290(void* draw) noexcept;

}  // namespace ra3::render
