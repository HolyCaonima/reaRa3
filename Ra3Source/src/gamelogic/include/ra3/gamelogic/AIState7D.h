#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Memory and call stand-ins for the 0x007D0000 AI state / AI player bodies.
// `call` is invoked for every direct or virtual call the original instruction
// stream makes. Stack-temporary pointers passed through it live in `scratch`.
struct AIState7DEnv {
    std::uint32_t gameLogic = 0;    // dword at 0x00DEBA64
    std::uint32_t logicTable = 0;   // dword at 0x00DFC4B4
    std::uint32_t timeScaleBits = 0; // dword at 0x00DEEEE8
    std::uint32_t partition = 0;    // dword at 0x00DFB9D8
    std::uint32_t subsystem = 0;    // dword at 0x00DF03FC
    std::uint32_t registry = 0;     // dword at 0x00DFBD7C
    std::uint8_t scratch[256]{};
    std::uint32_t lastSt0 = 0;
    std::uint32_t (*call)(void* ctx, std::uint32_t target, std::uint32_t ecx,
                          const std::uint32_t* args, int argc) = nullptr;
    void* callCtx = nullptr;
};

// Runs one counted body. Returns false when `va` is not part of this batch.
// `eaxOut` receives the body's EAX. Stack arguments follow the original
// right-to-left pushes; thiscall callees are modelled by `env.call`.
bool runAIState7D(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                  const std::uint32_t* stack, int argc, AIState7DEnv& env,
                  std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
