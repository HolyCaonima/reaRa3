#pragma once

#include <cstdint>

namespace ra3::render {

// ============================================================================
// 1. W3DTerrainVisual (primary vtable 0x00be38e8) & ray/water helpers
// ============================================================================

// 0x00561d70 — 0x00be38e8 slot 9
// Calls this->vtable[10](this, x, y); if non-null, returns result->vtable[6]
// via 0x00917a50 (mov eax, [ecx]; mov eax, [eax+0x18]; ret).
std::uint32_t queryTerrainVisualSubobjectSlot6(void* terrainVisual, float x, float y);

// 0x00561db0 — 0x00be38e8 slot 20
// Reads heightmap sample at (x + border, y + border) from m_worldHeightMap (+0x30)
// and converts via int((float)sample * 0.0390625f + 0.5f).
std::int32_t getRawMapHeight(const void* terrainVisual, std::int32_t x, std::int32_t y);

// 0x00561e10 — 0x00be38e8 slot 21
// Expands non-null IRegion2D (lo.x-1, lo.y-1, hi.x+1, hi.y+1) or passes nullptr
// to m_terrainRenderObject (+0x28) vtable slot 10 (+0x28).
void partialMapUpdate(void* terrainVisual, const void* region2D);

// 0x00561e70 — 0x00be38e8 slot 23
// Returns m_worldHeightMap->m_borderSize (+0x2c) if m_worldHeightMap (+0x30) != nullptr, else 0.
std::int32_t getTerrainVisualBorderSize(const void* terrainVisual);

// 0x00561e80 — 0x00be38e8 slot 18
// If m_terrainRenderObject (+0x28) != nullptr, tail-calls its vtable slot 19 (+0x4c).
void removeAllBibs(void* terrainVisual);

// 0x0056b290 — 0x00be38e8 slot 8
// If m_worldHeightMap (+0x30) != nullptr, forwards (x, y, outColor) to WorldHeightMap::getTerrainColorAt (0x00562df0).
void getTerrainVisualColorAt(const void* terrainVisual, float x, float y, void* outColor);

// 0x0056b2f0 — 0x00be38e8 slot 19
// Updates height sample (+0x44) and cell byte flag (+0x48) at (x + border, y + border) in m_worldHeightMap (+0x30).
// Preserves upper 24 bits of EAX from internal index computation, sets AL = 1 if updated else 0.
std::uint32_t setRawMapHeightAndFlags(
    void* terrainVisual,
    std::int32_t x,
    std::int32_t y,
    std::int32_t heightValue,
    std::uint32_t flagWord);

// 0x0056b380 — 0x00be38e8 slot 22
// Reads byte at m_worldHeightMap->m_cellFlags[idx] (+0x48), preserving upper 24 bits of EAX from idx.
std::uint32_t getCellFlags(const void* terrainVisual, std::int32_t x, std::int32_t y);

// 0x00570d50 — direct callee of 0x005882c0 at 0x00588364
// LineSegClass::recalculate(): computes DP = P1 - P0, Dir = DP, Length = sqrt(DP.DP), and normalizes Dir if Length > 1e-4f.
void recalculateLineSeg(void* lineSeg);

// 0x00572070 — direct callee of 0x005882c0 at 0x0058837d
// RayCollisionTestClass::RayCollisionTestClass(ray, result, collisionType, flag3c, flag3d)
void* initRayCollisionTest(
    void* rayCollisionTest,
    const void* lineSeg,
    void* castResult,
    std::uint32_t collisionType,
    std::uint8_t flag3c,
    std::uint8_t flag3d);

// 0x00580690 — direct callee of 0x005861e0 at 0x00586653
// LineSegClass::set(p0, p1): copies p0 and p1 via x87 m32 and calls recalculateLineSeg (0x00570d50).
void* setLineSegPoints(void* lineSeg, const void* p0, const void* p1);

// 0x005882c0 — 0x00be38e8 slot 11
// W3DTerrainVisual::intersectTerrain(rayStart, rayEnd, outHitPoint)
std::uint32_t intersectTerrain(
    void* terrainVisual,
    const void* rayStart,
    const void* rayEnd,
    void* outHitPoint);

// 0x0057a140 — direct callee of 0x00587ff0 at 0x00588039
// Initializes W3D water/wave grid subobject (size 0x1320, 100 entries of 0x30 bytes at +0x34..+0x12f3).
void initWaterWaveGridState(void* waterState);

// ============================================================================
// 2. W3DTerrainLogic (primary vtable 0x00be39f8) & direct helpers
// ============================================================================

// 0x0056ec10 — 0x00be39f8 slot 10
// W3DTerrainLogic::getExtent(Region3D* extent)
void getTerrainLogicExtent(const void* terrainLogic, void* extent);

// 0x0056ecc0 — 0x00be39f8 slot 11
// W3DTerrainLogic::getMaximumPathfindExtent(Region3D* extent)
void getMaximumPathfindExtent(const void* terrainLogic, void* extent);

// 0x0056edb0 — 0x00be39f8 slot 14
// W3DTerrainLogic::getSmallestExtent(Region3D* extent)
void getSmallestTerrainExtent(const void* terrainLogic, void* extent);

// 0x00563980 — direct callee of 0x0056ef10 at 0x0056ef57 (0x00be39f8 slot 18)
// Lexicographically orders two 3D points (by x, then y) into (*outFirst, *outSecond).
void orderCoord3DPair(
    float ax, float ay, float az,
    float bx, float by, float bz,
    void* outFirst,
    void* outSecond);

// ============================================================================
// 3. WorldHeightMap (vtable 0x00be4e2c & WorldHeightMap.cpp)
// ============================================================================

// 0x00562bb0 — direct callee of 0x00580a90 at 0x00580c72
// WorldHeightMap::setFlipState(xIndex, yIndex, value) on bit array at +0x4c (size at +0x50, stride at +0x54).
void setFlipState(void* heightMap, std::int32_t xIndex, std::int32_t yIndex, std::uint8_t value);

// 0x00562d30 — direct callee of 0x0058a460 at 0x0058a613
// WorldHeightMap::countTiles(InputStream* pStrm, Bool* halfTile)
std::int32_t countTilesFromTgaHeader(void* inputStream, std::uint8_t* halfTileOut);

// 0x00562df0 — direct callee of 0x0056b290 at 0x0056b2ae
// WorldHeightMap::getTerrainColorAt(x, y, outRGBColor)
void getTerrainColorAt(const void* heightMap, float x, float y, void* outColor);

// 0x00562f10 — direct callee of 0x0056e0a0 at 0x0056e1ea
// WorldHeightMap::evaluateBlendTileCoeffs(blendInfo, outFloats4)
void evaluateBlendTileCoeffs(const void* heightMap, const void* blendInfo, float* outCoeffs4);

// 0x005642b0 — direct callee of 0x00580a90 at 0x00580c9b
// WorldHeightMap::getCellFlagBit78(xIndex, yIndex)
std::uint32_t getCellFlagBit78(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x00564300 — direct callee of 0x0056f180 at 0x0056f209
// WorldHeightMap::getCellFlagBit88(xIndex, yIndex)
std::uint32_t getCellFlagBit88(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056d970 — direct callee of 0x008e4c30 at 0x008e4c9f
// WorldHeightMap::getCliffState(xIndex, yIndex) (bit array at +0x58..+0x5c)
std::uint32_t getCliffState(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056d9d0 — direct callee of 0x008e4cb0 at 0x008e4d1f
// WorldHeightMap::getCellFlagBit98(xIndex, yIndex) (bit array at +0x98..+0x9c)
std::uint32_t getCellFlagBit98(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056da40 — direct callee of 0x008e4d30 at 0x008e4d9f
// WorldHeightMap::getCellFlagBitB8(xIndex, yIndex) (bit array at +0xb8..+0xbc)
std::uint32_t getCellFlagBitB8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056dab0 — direct callee of 0x008e4db0 at 0x008e4e1f
// WorldHeightMap::getCellFlagBitA8(xIndex, yIndex) (bit array at +0xa8..+0xac)
std::uint32_t getCellFlagBitA8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056db20 — direct callee of 0x008e4e30 at 0x008e4e98
// WorldHeightMap::getCellFlagBitD8(xIndex, yIndex) (bit array at +0xd8..+0xdc, returns unnormalized mask in EAX or AL=0)
std::uint32_t getCellFlagBitD8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056db80 — direct callee of 0x008e4eb0 at 0x008e4f18
// WorldHeightMap::getCellFlagBitC8(xIndex, yIndex) (bit array at +0xc8..+0xcc, returns unnormalized mask in EAX or AL=0)
std::uint32_t getCellFlagBitC8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056dc30 — direct callee of 0x0056dfb0 at 0x0056e086
// WorldHeightMap::setCliffState(xIndex, yIndex, state) (bit array at +0x58..+0x5c)
void setCliffState(void* heightMap, std::int32_t xIndex, std::int32_t yIndex, std::uint8_t state);

// 0x0056dfb0 — direct callee of 0x0057e4e0 at 0x0057e504
// WorldHeightMap::setCellCliffFlagFromHeights(xIndex, yIndex)
void setCellCliffFlagFromHeights(void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0057e4e0 — direct callee of 0x005a1700 at 0x005a1c15
// WorldHeightMap::initCliffFlagsFromHeights()
void initCliffFlagsFromHeights(void* heightMap);

// 0x0056e0a0 — direct callee of 0x00580a90 at 0x00580c55
// WorldHeightMap::getCellBlendClassesAndCoeffs(xIndex, yIndex, outTexClasses3, outBlendCoeffs8)
void getCellBlendClassesAndCoeffs(
    const void* heightMap,
    std::int32_t xIndex,
    std::int32_t yIndex,
    std::int32_t* outTexClasses3,
    float* outBlendCoeffs8);

// 0x0056dd70 — direct callee of 0x0057db70 at 0x0057dbb1
// WorldHeightMap::isCliffMappedTextureCheck(xOffset, yOffset)
std::uint32_t isCliffMappedTextureCheck(const void* heightMap, std::int32_t xOffset, std::int32_t yOffset);

// 0x0057db70 — direct callee of 0x00580a90 at 0x00580c5f
// WorldHeightMap::shouldFlipCellQuad(xIndex, yIndex)
std::uint32_t shouldFlipCellQuad(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex);

// 0x0056de70 — direct callee of 0x00580fb0 at 0x00581286
// WorldHeightMap::getBlendVertexAlphaMask(xOffset, yOffset, outAlphaBytes4, unusedArg)
void getBlendVertexAlphaMask(
    const void* heightMap,
    std::int32_t xOffset,
    std::int32_t yOffset,
    std::uint8_t* outAlphaBytes4,
    std::uint32_t unusedArg);

// ============================================================================
// 4. BaseHeightMapRenderObjClass (0x00be4290), Scorch, Road & Prop Buffers
// ============================================================================

// 0x00565250 — 0x00be4290 slot 14
// BaseHeightMapRenderObjClass::clearPatchVisibilityAndBounds()
void clearPatchVisibilityAndBounds(void* heightMapRenderObj);

// 0x00563520 — direct callee of 0x00580a90 at 0x00580bbd
// Initializes a 2-dword stream lock helper: this[0] = streamObj, this[1] = streamObj->vtable[3](streamObj, lockArg).
void* initStreamLockHelper(void* helperThis, void* streamObj, std::uint32_t lockArg);

// 0x005635f0 — direct callee of 0x00564f80 at 0x00564ff6
// Component-wise Vector3 min using SSE sign-bit mask: (a - b) < 0 ? a : b.
void* vec3SignMaskMin(void* outVec3, const void* aVec3, const void* bVec3);

// 0x005636e0 — direct callee of 0x00564f80 at 0x00565048
// Component-wise Vector3 max using SSE sign-bit mask: (a - b) < 0 ? b : a.
void* vec3SignMaskMax(void* outVec3, const void* aVec3, const void* bVec3);

// 0x00564e70 — direct callee of 0x00580a90 at 0x00580f12
// Converts MinMaxAABox (min at +0, max at +0xc) to CenterExtentAABox (center=(max+min)*0.5f, extent=(max-min)*0.5f).
void aaboxFromMinMax(void* outCenterExtentBox, const void* inMinMaxBox);

// 0x00564f20 — direct callee of 0x00580a90 at 0x00580d23
// Merges a 3D point into a MinMaxAABox using vec3SignMaskMin and vec3SignMaskMax.
void minMaxAABoxMergePoint(void* minMaxBox, const void* point3D);

// 0x00564f80 — direct callee of 0x00570e60 at 0x00570f20
// Merges a CenterExtentAABox into a MinMaxAABox if any extent component != 0.0f.
void minMaxAABoxMergeCenterExtentBox(void* minMaxBox, const void* centerExtentBox);

// 0x00565130 — direct callee of 0x00580a90 at 0x00580d12
// Packs terrain vertex position, normal, 6 float UV parameters, and 2 blend class indices into a 24-byte compressed vertex.
void packCompressedTerrainVertex(
    const void* renderObj,
    void* outVertex24,
    const void* posVec3,
    const void* normalVec3,
    const void* texParams6,
    const void* blendParams2);

// 0x00565700 — direct callee of 0x008f17e0 at 0x008f18f3
// Copies 13 floats (52 bytes) via x87 fld/fstp sequence.
void copyTerrainRenderState52(void* dst52, const void* src52);

// 0x005615a0 — direct callee of 0x008fd1e0 at 0x008fd206
// W3DScorchBuffer::clearAllScorches(): zeros +0x4660, +0x4664, +0x4668.
void clearAllScorches(void* scorchBuffer);

// 0x0056ab00 — direct callee of 0x008e51a0 at 0x008e51b4
// W3DScorchBuffer::invalidateScorchesInRadius(centerPos, radius)
void invalidateScorchesInRadius(void* scorchBuffer, const void* centerPos, float radius);

// 0x005612f0 — direct callee of 0x00569570 at 0x00569571
// W3DRoadBuffer::clearRoadTypeLinks(): clears m_roads[i]+0x44 and m_roadTypes[j]+0x04.
void clearRoadTypeLinks(void* roadBuffer);

// 0x00561340 — direct callee of 0x00585900 at 0x00585e6b
// W3DRoadBuffer::adjustStacking(topUniqueID, bottomUniqueID)
void adjustRoadStacking(void* roadBuffer, std::int32_t topUniqueID, std::int32_t bottomUniqueID);

// 0x00569570 — direct callee of 0x005a7a60 at 0x005a7aa1
// W3DRoadBuffer::linkRoadsToRoadTypes()
void linkRoadsToRoadTypes(void* roadBuffer);

// 0x005695e0 — direct callee of 0x00577cc0 at 0x00577e6d
// W3DRoadBuffer::updateCounts(RoadSegment* pRoad)
void updateRoadSegmentCounts(void* roadBuffer, void* roadSegment);

// 0x00569750 — direct callee of 0x005a7a60 at 0x005a7a7e
// W3DRoadBuffer::updateCountsAndFlags()
void updateAllRoadCountsAndFlags(void* roadBuffer);

// 0x0056a8a0 — direct callee of 0x005861e0 at 0x00586a19
// W3DRoadBuffer::rotateAbout(Vector2* ptP, Vector2 center, float angle)
void rotateRoadPointAbout(void* roadBuffer, void* ptVec2, float centerX, float centerY, float angle);

// 0x00568ea0 — direct callee of 0x00578150 at 0x00578219
// RoadSegment::RoadSegment() default constructor (0x7c bytes, leaving +0x44 uninitialized per binary).
void* initRoadSegment(void* roadSegment);

// 0x00569480 — direct callee of 0x00578150 at 0x0057834c
// RoadSegment::operator=(const RoadSegment& src) (0x7c bytes with x87 float copies).
void* copyRoadSegment(void* dstSegment, const void* srcSegment);

// 0x0058d1b0 — direct callee of 0x008ec630 at 0x008ec795
// W3DPropBuffer::W3DPropBuffer() default constructor (sets vtable 0x00be4d60 and zeros fields).
void* initPropBuffer(void* propBuffer);

} // namespace ra3::render
