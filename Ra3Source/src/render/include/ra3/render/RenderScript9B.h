#pragma once

#include <cstdint>

namespace ra3::render {

// Direct callees of the script-command bodies. The oracle supplies the same
// effects on both sides; COM/device calls are not executed against a real device.
struct ScriptServices {
    std::uint8_t* stringPool = nullptr;
    std::uint32_t (*log)(std::uint32_t format, std::uint32_t text) = nullptr;
    std::uint32_t (*clearSurface)(
        std::uint8_t* device, std::uint32_t flags, std::uint32_t color, std::uint32_t depth,
        std::uint32_t stencil) = nullptr;
    std::uint32_t (*setRenderTarget)(std::uint8_t* device, std::uint32_t a, std::uint32_t b) = nullptr;
    std::uint32_t (*bindShader)(std::uint8_t* shader, std::uint32_t arg) = nullptr;
    std::uint32_t (*vectorReserve)(std::uint8_t* vec, std::uint32_t count, std::uint32_t flag) = nullptr;
    std::uint32_t (*vectorShrink)(std::uint8_t* vec) = nullptr;
    std::uint32_t (*lookupFormat)(std::uint8_t* context, std::uint32_t text) = nullptr;
    std::uint32_t (*nameEquals)(std::uint8_t* entry, std::uint32_t left, std::uint32_t right) = nullptr;
    std::uint32_t (*ensurePass)(std::uint8_t* self, std::uint32_t index) = nullptr;
    std::uint32_t (*bindConstants)(std::uint8_t* device, std::uint32_t block) = nullptr;
    std::uint32_t (*refreshConstants)(std::uint8_t* device, std::uint32_t block) = nullptr;
    std::uint32_t (*flushTarget)(std::uint8_t* device, std::uint32_t block) = nullptr;
};

void setScriptServices(const ScriptServices& services) noexcept;

// Token conversion. 0x009B7E50 / 0x009B7EF0.
std::uint32_t parseFloatTokens(
    std::uint8_t* context, std::uint8_t* tokens, std::uint32_t start, std::uint8_t* dest,
    std::uint32_t count) noexcept;
std::uint32_t parseIntTokens(
    std::uint8_t* context, std::uint8_t* tokens, std::uint32_t start, std::uint8_t* dest,
    std::uint32_t count) noexcept;

// Name scans. 0x009B7740 shader list, 0x009B7980 texture list.
std::uint32_t findShaderByName(std::uint8_t* self, const char* name) noexcept;
std::uint32_t findTextureByName(std::uint8_t* self, const char* name) noexcept;

// Command parses (vtable slot 3) and executes (slot 4).
std::uint32_t parseClearCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseClearTargetCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetClearColorCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseSetClearDepthCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseRenderQuadCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseClearShaderOverrideCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseCastTextureCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseEndProcCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseReturnCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;
std::uint32_t parseElseCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept;

std::uint32_t execSetClearColorCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetClearDepthCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execClearCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execClearColorCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execClearZCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execClearTargetCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execGotoCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execElseCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execElifCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetShaderOverrideCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execClearShaderOverrideCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetShaderCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* out, std::uint8_t* ip) noexcept;
std::uint32_t execSetTextureCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execCastTextureCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetIntCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetBoolCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetDepthCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execBeginEventCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execEndEventCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execSetMarkerCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execEndProcCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;
std::uint32_t execCallProcCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t* ip) noexcept;

// Inferred classes: stream binder (00C5EC94, no slot-2 name), pass override
// (00C600F0), surface pair (00C5DB10), and the script machine (00C5DB18,
// slot 2 is 009A8750).
std::uint32_t clearStreamBindings(std::uint8_t* self) noexcept;
std::uint32_t assignStreamSource(std::uint8_t* self, std::uint8_t* source, std::uint8_t* callback) noexcept;
std::uint32_t resetPassOverrides(std::uint8_t* self) noexcept;
std::uint32_t applyNamedState(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept;
std::uint32_t applyNamedSampler(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept;
std::uint32_t applyNamedStage(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept;
std::uint32_t querySurfaceExtent(std::uint8_t* self, std::uint8_t* name, std::uint8_t* out) noexcept;
std::uint32_t flushActiveTarget(std::uint8_t* self) noexcept;
std::uint32_t selectPassConstant(std::uint8_t* self, std::uint32_t index) noexcept;
std::uint32_t releasePrimarySurface(std::uint8_t* self) noexcept;
std::uint32_t releaseSecondarySurface(std::uint8_t* self) noexcept;
std::uint32_t bindTechniquePass(std::uint8_t* self, std::uint32_t pass, std::uint32_t index) noexcept;

}  // namespace ra3::render
