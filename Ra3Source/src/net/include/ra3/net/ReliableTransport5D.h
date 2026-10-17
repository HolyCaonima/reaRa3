#pragma once

#include <cstdint>

namespace ra3::net {

// Leaves from 0x005D0000-0x005DFFFF. Names follow the class when slot 2 is a
// name getter (GameInfo, SkirmishGameInfo, LANGameInfo). The other tables have
// no such getter; those functions are named from the operation and the comments
// name the vtable plus the constructor that writes it.

constexpr std::uint32_t kEmptyString = 0x00BBED30u;
constexpr std::uint32_t kNatBufferVtable = 0x00BEE370u;
constexpr std::uint32_t kGameInfoVtable = 0x00BE8018u;
constexpr std::uint32_t kFloatMaxBits = 0x7F7FFFFFu;

// 00BF0DDC slots 4, 5, 10, 11, 12, 13. Null child yields 0 or kEmptyString.
std::uint32_t childByte2E(const std::uint8_t* self) noexcept;
std::uint32_t childByte2D(const std::uint8_t* self) noexcept;
std::uint32_t childDword30(const std::uint8_t* self) noexcept;
std::uint32_t childField8C(const std::uint8_t* self) noexcept;
std::uint32_t childDword6C(const std::uint8_t* self) noexcept;
std::uint32_t childDword7C(const std::uint8_t* self) noexcept;

// 00BF0BB4 slots 4 and 5. Same +0xC0 child as the table above.
std::uint32_t childByte29(const std::uint8_t* self) noexcept;
std::uint32_t childByte28(const std::uint8_t* self) noexcept;

// 00BF1198 slot 8. AL is the inequality; the high bits stay the first field.
std::uint32_t cursorFieldsDiffer(const std::uint8_t* self) noexcept;

// 00BF1558 slots 97 and 64.
std::uint32_t storePair124(std::uint8_t* self, std::uint32_t first, std::uint32_t second) noexcept;
std::uint32_t flagOrPointerSet(const std::uint8_t* self) noexcept;

// 00BEE470 slots 3 and 5. Constructor 005E6D20 (GameSPYNATImpl.cpp) writes the table.
std::int32_t natBufferRead(std::uint8_t* self, std::uint8_t* dest, std::int32_t count) noexcept;
std::uint32_t natBufferSeek(std::uint8_t* self, std::uint32_t offset, std::uint32_t origin) noexcept;

// GameInfo / SkirmishGameInfo. Human state is 6. Slots live at this+4 as six pointers.
std::uint32_t clearSlotIpAddresses(std::uint8_t* self) noexcept;
std::uint32_t dismissIndexedSlot(std::uint8_t* self) noexcept;
std::uint32_t findLocalHumanSlot(const std::uint8_t* self) noexcept;
std::uint32_t slotZeroIsLocalHuman(const std::uint8_t* self) noexcept;
std::uint32_t otherSlotIsHuman(const std::uint8_t* self) noexcept;
std::uint32_t countAcceptedSlots(const std::uint8_t* self) noexcept;

// LANGameInfo 00BEFCA0 slot 13. network is the global at 0x00DF6B2C; a non-null
// value is invoked through vtable+0x7C and the return is ignored.
std::uint32_t clearLanHumanReady(std::uint8_t* self, std::uint8_t* network) noexcept;

// 00BEEDB0 slots. Constructor 005DD0A0 writes the table and zeros +0x24/+0x28/+0x2C.
std::uint32_t linkStateIsOne(const std::uint8_t* self) noexcept;
std::uint32_t linkChildField8(const std::uint8_t* self) noexcept;
std::uint32_t clampChannelStamp(std::uint8_t* self, std::uint32_t index, std::uint32_t value) noexcept;
std::uint32_t setChannelBit(std::uint8_t* self, std::uint32_t bit) noexcept;
std::uint32_t channelFrameReached(const std::uint8_t* self, std::uint32_t index,
                                  const std::uint8_t* gameInfo, const std::uint8_t* gameLogic) noexcept;

// 00BEF6E8 slots 48, 61, 52. Constructor 005E71B0 writes the table.
std::uint32_t chainNodeAt(const std::uint8_t* self, std::int32_t index) noexcept;
std::uint32_t findEndpointNode(const std::uint8_t* self, const std::uint8_t* key) noexcept;
std::uint32_t headEndpointMatches(std::uint8_t* self) noexcept;

// Called from 00BEF6E8 methods.
std::uint32_t endpointsEqual(const std::uint8_t* self, const std::uint8_t* other) noexcept;
std::uint32_t endpointsDiffer(const std::uint8_t* self, const std::uint8_t* other) noexcept;
std::uint32_t clampTinyState(std::uint8_t* self, std::uint32_t value) noexcept;
std::uint32_t recordAtFc(const std::uint8_t* self, std::uint32_t index) noexcept;
std::uint32_t occupiedRecordField(const std::uint8_t* self, std::uint32_t index) noexcept;
std::uint32_t setOccupiedRecordField(std::uint8_t* self, std::uint32_t index, std::uint32_t value) noexcept;
std::uint32_t unlinkWideChain(std::uint8_t* self, std::uint8_t* node, std::uint8_t*& gameInfo) noexcept;
std::uint32_t unlinkShortChain(std::uint8_t* self, std::uint8_t* node) noexcept;

// 00C730A4 slot 8 calls this. Writes vtable 00BEE370.
std::uint32_t initTaggedPointer(std::uint8_t* self, std::uint8_t* value) noexcept;

// 00C7D3F8 slot 3 calls this.
std::uint32_t stateIsSixOrSeven(const std::uint8_t* self) noexcept;

// 00BE8538 slot 1 calls this. Retains the header stored at object+0.
std::uint32_t attachAndRetain(std::uint8_t* self, std::uint8_t* object) noexcept;

// 00BDCC74 slot 4 calls this. Fills seven floats from 0x00BE700C (FLT_MAX).
void initFloatExtents(std::uint8_t* self) noexcept;

// GameStart.cpp callers.
std::uint32_t otherSlotSharesId(const std::uint8_t* self, std::uint32_t id, std::uint32_t skip) noexcept;
std::uint32_t slotStateIsAi(const std::uint8_t* self) noexcept;

}  // namespace ra3::net
