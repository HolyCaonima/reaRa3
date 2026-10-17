#pragma once

#include "ra3/render/RenderScript9B.h"

namespace ra3::render {

// Stand-ins shared with the oracle. `manager` is the 009973F0 object (bump
// allocation at +0x20 and scripted callee results). `pool` is the 00997400
// string pool, the same pointer installed in ScriptServices::stringPool.
struct ScriptHost {
    std::uint8_t* manager = nullptr;
    std::uint8_t* pool = nullptr;
    std::uint32_t (*log)(std::uint32_t format, std::uint32_t text) = nullptr;
    std::uint32_t (*reserve)(std::uint8_t* vec, std::uint32_t count, std::uint32_t flag) = nullptr;
    std::uint32_t (*shrink)(std::uint8_t* vec) = nullptr;
};

void setScriptHost(const ScriptHost& host) noexcept;

// Second pass over the script interpreter. Command classes keep the render9b
// record layout. Unported callees are modeled by the same stand-ins on both
// sides of the oracle (allocator bump, technique/name results, draw log).

std::uint32_t parseSetShaderCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetShaderOverrideCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetTextureCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetIntCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetBoolCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetDepthCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseGotoCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseElifCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseBeginEventCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseCallProcCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetFloatCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetToSizeCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseIfNotCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseIfCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseEndIfCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetCameraCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;

std::uint32_t execRenderQuadCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execRenderFullQuadCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetViewportCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetScissorCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetCameraCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetToSizeCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execIfNotCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execIfCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execEndIfCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;

// Vertex-batch flushes, one per D3DPRIMITIVETYPE (DrawPrimitiveUP). Slot 2 is 009B58A0, not a usage string.
std::uint32_t flushLineList(std::uint8_t* self) noexcept;
std::uint32_t flushPointList(std::uint8_t* self) noexcept;
std::uint32_t flushLineStrip(std::uint8_t* self) noexcept;
std::uint32_t flushTriangleList(std::uint8_t* self) noexcept;
std::uint32_t flushTriangleStrip(std::uint8_t* self) noexcept;
std::uint32_t flushTriangleFan(std::uint8_t* self) noexcept;

std::uint32_t dispatchDirtyState(std::uint8_t* self, std::uint32_t maskA, std::uint32_t maskB) noexcept;
std::uint32_t classifyPrimitive(std::uint8_t* self, std::uint8_t* record) noexcept;
std::uint32_t readMatrixOrder(std::uint8_t* self) noexcept;
std::uint32_t selectConstantRoutine(std::uint8_t* self, std::uint32_t kind, std::uint32_t unused, std::uint8_t* info) noexcept;
std::uint32_t bindDynamicStream(std::uint8_t* self, std::uint8_t* source, std::uint8_t* callback) noexcept;

}  // namespace ra3::render
