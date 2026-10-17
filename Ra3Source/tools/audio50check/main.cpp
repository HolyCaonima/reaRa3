#include <ra3/audio/SageAudioManager50.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
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

using Bytes = std::vector<std::byte>;

constexpr std::size_t kArenaSize = 0x10000;
std::byte* g_arena = nullptr;
bool g_mutate = false;

std::byte* allocateArena() {
    for (std::uintptr_t addr = 0x30000000u; addr <= 0x50000000u; addr += 0x01000000u) {
        void* mem = VirtualAlloc(reinterpret_cast<void*>(addr), kArenaSize,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (mem != nullptr) {
            return static_cast<std::byte*>(mem);
        }
    }
    throw std::runtime_error("failed to allocate deterministic arena");
}

std::uint32_t parseWord(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

std::uint32_t argument(std::istringstream& input) {
    std::string text;
    input >> text;
    if (text.empty()) {
        throw std::runtime_error("missing argument");
    }
    return parseWord(text);
}

Bytes decode(const std::string& text) {
    if (text == ".") {
        return {};
    }
    if ((text.size() & 1u) != 0) {
        throw std::runtime_error("odd hex input");
    }
    Bytes result(text.size() / 2);
    for (std::size_t index = 0; index != result.size(); ++index) {
        result[index] =
            static_cast<std::byte>(parseWord(text.substr(index * 2, 2)));
    }
    return result;
}

void encode(const void* storage, std::size_t size) {
    if (size == 0) {
        std::cout << ".";
        return;
    }
    const auto* raw = static_cast<const std::byte*>(storage);
    std::cout << std::hex << std::setfill('0');
    for (std::size_t index = 0; index != size; ++index) {
        std::cout << std::setw(2) << std::to_integer<unsigned>(raw[index]);
    }
    std::cout << std::dec;
}

float asFloat(std::uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void captureFloatSt0(float value, std::uint16_t controlWord, Bytes& aux) {
    aux.resize(14);
#if defined(_MSC_VER) && defined(_M_IX86)
    float rounded = 0.0f;
    std::array<std::byte, 10> ext{};
    unsigned short savedCw = 0;
    unsigned short gameCw = controlWord;
    __asm {
        fnstcw savedCw
        fldcw gameCw
        fld dword ptr [value]
        fst dword ptr [rounded]
        fstp tbyte ptr [ext]
        fldcw savedCw
    }
    const std::uint32_t rbits = floatBits(rounded);
    std::memcpy(aux.data(), &rbits, 4);
    std::memcpy(aux.data() + 4, ext.data(), 10);
#else
    (void)controlWord;
    const std::uint32_t rbits = floatBits(value);
    std::memcpy(aux.data(), &rbits, 4);
#endif
}

void captureDoubleSt0(double value, std::uint16_t controlWord, Bytes& aux) {
    aux.resize(14);
#if defined(_MSC_VER) && defined(_M_IX86)
    float rounded = 0.0f;
    std::array<std::byte, 10> ext{};
    unsigned short savedCw = 0;
    unsigned short gameCw = controlWord;
    __asm {
        fnstcw savedCw
        fldcw gameCw
        fld qword ptr [value]
        fst dword ptr [rounded]
        fstp tbyte ptr [ext]
        fldcw savedCw
    }
    const std::uint32_t rbits = floatBits(rounded);
    std::memcpy(aux.data(), &rbits, 4);
    std::memcpy(aux.data() + 4, ext.data(), 10);
#else
    (void)controlWord;
    const std::uint32_t rbits = floatBits(static_cast<float>(value));
    std::memcpy(aux.data(), &rbits, 4);
#endif
}

void* ptrFromAddr(std::uint32_t addr) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr));
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--mutate") {
            g_mutate = true;
        }
    }

    g_arena = allocateArena();
    const auto arenaBase =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_arena));

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string op;
        input >> op;
        if (op == "quit") {
            return 0;
        }
        if (op == "base") {
            std::cout << std::hex << arenaBase << std::dec << "\n";
            std::cout.flush();
            continue;
        }
        if (op == "mutate") {
            g_mutate = argument(input) != 0;
            std::cout << "0 . .\n";
            std::cout.flush();
            continue;
        }

        std::string blobText;
        input >> blobText;
        Bytes slice = decode(blobText);
        if (slice.size() > kArenaSize) {
            std::cout << "error\n";
            std::cout.flush();
            return 1;
        }
        if (!slice.empty()) {
            std::memcpy(g_arena, slice.data(), slice.size());
        }

        std::uint32_t result = 0;
        Bytes aux;

        if (op == "502270") {
            const auto modeOverride = static_cast<std::int32_t>(argument(input));
            const auto sysAddr = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::resolveSpeakerCenterChannelEnabled(
                modeOverride, ptrFromAddr(sysAddr), initialEax);
            if (g_mutate) {
                result ^= 1u;
            }
        } else if (op == "502f00") {
            const auto soundAddr = argument(input);
            const auto durBits = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::pushRenderingSoundSampleDuration(
                ptrFromAddr(soundAddr), asFloat(durBits), initialEax);
        } else if (op == "502fb0") {
            result = ra3::audio::sageAudioManagerEmptyEventAddress();
        } else if (op == "502ff0") {
            const auto mgrAddr = argument(input);
            result = ra3::audio::sageAudioManagerStopAllImmediately(
                ptrFromAddr(mgrAddr));
        } else if (op == "503000") {
            const auto mgrAddr = argument(input);
            const auto listenerCount = argument(input);
            const auto pausedFlag = static_cast<std::uint8_t>(argument(input));
            const auto timerAddr = argument(input);
            const auto stepBits = argument(input);
            const auto initialEax = argument(input);
            const auto cw = static_cast<std::uint16_t>(argument(input));
#if defined(_MSC_VER) && defined(_M_IX86)
            unsigned short savedCw = 0;
            unsigned short gameCw = cw;
            __asm {
                fnstcw savedCw
                fldcw gameCw
            }
#endif
            const float val = ra3::audio::sageAudioManagerUpdateFrameDeltaSeconds(
                ptrFromAddr(mgrAddr), listenerCount, pausedFlag,
                ptrFromAddr(timerAddr), asFloat(stepBits), &result, initialEax);
#if defined(_MSC_VER) && defined(_M_IX86)
            __asm {
                fldcw savedCw
            }
#endif
            captureFloatSt0(val, cw, aux);
        } else if (op == "503290") {
            const auto ptrAddr = argument(input);
            result = ra3::audio::sageAudioManagerListenerOrientationCount(
                 static_cast<const std::uint32_t*>(ptrFromAddr(ptrAddr)));
        } else if (op == "503300") {
            result = ra3::audio::sageAudioManagerSubsystemNameAddress();
            const char* lit = ra3::audio::sageAudioManagerSubsystemNameLiteral();
            const std::size_t len = std::strlen(lit) + 1;
            aux.resize(len);
            std::memcpy(aux.data(), lit, len);
        } else if (op == "503400") {
            result = ra3::audio::sagePathMusicManagerSubsystemNameAddress();
            const char* lit = ra3::audio::sagePathMusicManagerSubsystemNameLiteral();
            const std::size_t len = std::strlen(lit) + 1;
            aux.resize(len);
            std::memcpy(aux.data(), lit, len);
        } else if (op == "5044f0") {
            const auto handleAddr = argument(input);
            const auto eventAddr = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::initAudioEventRtsHandle(ptrFromAddr(handleAddr),
                                                    ptrFromAddr(eventAddr))));
        } else if (op == "504560") {
            const auto handleAddr = argument(input);
            result = ra3::audio::releaseAudioEventRtsHandle(ptrFromAddr(handleAddr));
        } else if (op == "504590") {
            const auto handleAddr = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::releaseAndClearPlayingAudioHandle(
                ptrFromAddr(handleAddr), initialEax);
        } else if (op == "5045c0") {
            const auto handleAddr = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::releasePlayingAudioHandle(ptrFromAddr(handleAddr),
                                                           initialEax);
        } else if (op == "5045e0") {
            const auto nodeAddr = argument(input);
            result = ra3::audio::unlinkPlayingAudioNode(ptrFromAddr(nodeAddr));
        } else if (op == "504ac0") {
            const auto infoAddr = argument(input);
            const auto bitIndex = argument(input);
            result = ra3::audio::testAudioEventCategoryBit(ptrFromAddr(infoAddr),
                                                           bitIndex);
        } else if (op == "504af0") {
            const auto infoAddr = argument(input);
            const auto maskAddr = argument(input);
            result = ra3::audio::intersectsAudioEventCategoryMask(
                ptrFromAddr(infoAddr),
                static_cast<const std::uint32_t*>(ptrFromAddr(maskAddr)));
        } else if (op == "504b20") {
            const auto nodeAddr = argument(input);
            const auto nextAddr = argument(input);
            const auto prevAddr = argument(input);
            const auto payloadAddr = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::initPlayingAudioPoolNode(
                    ptrFromAddr(nodeAddr), ptrFromAddr(nextAddr),
                    ptrFromAddr(prevAddr), ptrFromAddr(payloadAddr))));
        } else if (op == "504c80") {
            const auto playerAddr = argument(input);
            const auto category = argument(input);
            const auto subchannel = argument(input);
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = ra3::audio::recomputePlayerCategoryFinalVolume(
                ptrFromAddr(playerAddr), category, subchannel,
                ptrFromAddr(globalSettingsAddr),
                static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
        } else if (op == "504df0") {
            const auto playerAddr = argument(input);
            const auto category = argument(input);
            const auto lockCount = static_cast<std::int32_t>(argument(input));
            const auto globalSettingsAddr = argument(input);
            result = ra3::audio::recomputePlayerCategoryModifierProduct(
                ptrFromAddr(playerAddr), category, lockCount,
                ptrFromAddr(globalSettingsAddr));
        } else if (op == "504ec0") {
            const auto playerAddr = argument(input);
            result = ra3::audio::decrementPlayerVolumeChangeCounters(
                ptrFromAddr(playerAddr));
        } else if (op == "504f20") {
            const auto playerAddr = argument(input);
            const auto infoPtrAddr = argument(input);
            const auto subchannel = argument(input);
            result = ra3::audio::hasPendingVolumeChangeForEvent(
                ptrFromAddr(playerAddr),
                static_cast<void* const*>(ptrFromAddr(infoPtrAddr)), subchannel);
        } else if (op == "504f80") {
            const auto playerAddr = argument(input);
            const auto duckParamsAddr = argument(input);
            const auto deltaVecAddr = argument(input);
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = ra3::audio::updatePlayerDuckingMultiplierFromDistance(
                ptrFromAddr(playerAddr), ptrFromAddr(duckParamsAddr),
                static_cast<const float*>(ptrFromAddr(deltaVecAddr)),
                ptrFromAddr(globalSettingsAddr),
                static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
        } else if (op == "505150") {
            const auto playerAddr = argument(input);
            const auto valBits = argument(input);
            const auto flags = static_cast<std::uint8_t>(argument(input));
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = ra3::audio::applyPlayerVolumeSettingsByFlags(
                ptrFromAddr(playerAddr), asFloat(valBits), flags,
                ptrFromAddr(globalSettingsAddr),
                static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
        } else if (op == "505340" || op == "506670") {
            const auto guardAddr = argument(input);
            const auto lockCountAddr = argument(input);
            const auto playersAddr = argument(input);
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            auto& lockRef =
                *static_cast<std::int32_t*>(ptrFromAddr(lockCountAddr));
            if (op == "505340") {
                result = ra3::audio::acquireGlobalVolumeModifierLock(
                    ptrFromAddr(guardAddr), lockRef, ptrFromAddr(playersAddr),
                    ptrFromAddr(globalSettingsAddr),
                    static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
            } else {
                result = ra3::audio::sageAudioManagerOnAcquireVolumeModifierLock(
                    ptrFromAddr(guardAddr), lockRef, ptrFromAddr(playersAddr),
                    ptrFromAddr(globalSettingsAddr),
                    static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
            }
        } else if (op == "5053b0" || op == "506680") {
            const auto guardAddr = argument(input);
            const auto lockCountAddr = argument(input);
            const auto playersAddr = argument(input);
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            auto& lockRef =
                *static_cast<std::int32_t*>(ptrFromAddr(lockCountAddr));
            if (op == "5053b0") {
                result = ra3::audio::releaseGlobalVolumeModifierLock(
                    ptrFromAddr(guardAddr), lockRef, ptrFromAddr(playersAddr),
                    ptrFromAddr(globalSettingsAddr),
                    static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
            } else {
                result = ra3::audio::sageAudioManagerOnReleaseVolumeModifierLock(
                    ptrFromAddr(guardAddr), lockRef, ptrFromAddr(playersAddr),
                    ptrFromAddr(globalSettingsAddr),
                    static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
            }
        } else if (op == "506c20") {
            const auto bitsetAddr = argument(input);
            const auto b0 = argument(input);
            const auto b1 = argument(input);
            const auto b2 = argument(input);
            const auto b3 = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::initCategoryBitsetFromFourBits(ptrFromAddr(bitsetAddr),
                                                           b0, b1, b2, b3)));
        } else if (op == "506c90") {
            const auto bitsetAddr = argument(input);
            const auto b0 = argument(input);
            const auto b1 = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::initCategoryBitsetFromTwoBits(ptrFromAddr(bitsetAddr),
                                                          b0, b1)));
        } else if (op == "506cd0") {
            const auto nodeAddr = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::unlinkPlayingAudioNodeIfOwned(
                ptrFromAddr(nodeAddr), initialEax);
        } else if (op == "507720") {
            const auto slotAddr = argument(input);
            result = ra3::audio::hasActiveRenderCursor(ptrFromAddr(slotAddr));
        } else if (op == "508330") {
            const auto soundAddr = argument(input);
            const auto cw = static_cast<std::uint16_t>(argument(input));
            const double val = ra3::audio::sumWindowSamplePlayheadRatios(
                ptrFromAddr(soundAddr), &result);
            captureDoubleSt0(val, cw, aux);
        } else if (op == "5084b0") {
            const auto soundAddr = argument(input);
            const auto cw = static_cast<std::uint16_t>(argument(input));
            const double val = ra3::audio::sumAllChannelSamplePlayheadRatios(
                ptrFromAddr(soundAddr), &result);
            captureDoubleSt0(val, cw, aux);
        } else if (op == "508640") {
            const auto nodeAddr = argument(input);
            const auto ownerAddr = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::appendPlayingAudioNodeToOwnerList(
                    ptrFromAddr(nodeAddr), ptrFromAddr(ownerAddr))));
        } else if (op == "508af0") {
            const auto mgrAddr = argument(input);
            const auto outHandleAddr = argument(input);
            const auto targetIdAddr = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::findPlayingAudioByHandle(
                    ptrFromAddr(mgrAddr),
                    static_cast<void**>(ptrFromAddr(outHandleAddr)),
                    static_cast<const std::uint32_t*>(ptrFromAddr(targetIdAddr)))));
        } else if (op == "508d50") {
            const auto mgrAddr = argument(input);
            const auto objectId = argument(input);
            const auto drawableId = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::hasActiveEventForObjectOrDrawable(
                ptrFromAddr(mgrAddr), objectId, drawableId, initialEax);
        } else if (op == "50a220") {
            const auto playerAddr = argument(input);
            const auto baseAddr = argument(input);
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                ra3::audio::initializePlayerAudioSettingsDefaults(
                    ptrFromAddr(playerAddr), ptrFromAddr(baseAddr),
                    ptrFromAddr(globalSettingsAddr),
                    static_cast<const float*>(ptrFromAddr(scaleTableAddr)))));
        } else if (op == "50ad80") {
            const auto mgrAddr = argument(input);
            const auto drawableId = argument(input);
            const auto lookupAddr = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::isObjectOrDrawablePlayingAudio(
                ptrFromAddr(mgrAddr), drawableId, ptrFromAddr(lookupAddr),
                initialEax);
        } else if (op == "50be70") {
            const auto cacheAddr = argument(input);
            result = ra3::audio::sageAudioCacheTotalEntryCount(
                ptrFromAddr(cacheAddr));
        } else if (op == "50de20") {
            const auto playerAddr = argument(input);
            const auto infoPtrAddr = argument(input);
            const auto srcPlayer = argument(input);
            const auto lockCount = static_cast<std::int32_t>(argument(input));
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = ra3::audio::removePlayerEventVolumeModifiers(
                ptrFromAddr(playerAddr),
                static_cast<void* const*>(ptrFromAddr(infoPtrAddr)), srcPlayer,
                lockCount, ptrFromAddr(globalSettingsAddr),
                static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
        } else if (op == "50df80") {
            const auto cacheAddr = argument(input);
            const auto key = argument(input);
            result = ra3::audio::removeAudioCacheListenerEntry(
                ptrFromAddr(cacheAddr), key);
        } else if (op == "50e040") {
            const auto playingAddr = argument(input);
            const auto playersAddr = argument(input);
            const auto lockCount = static_cast<std::int32_t>(argument(input));
            const auto globalSettingsAddr = argument(input);
            const auto scaleTableAddr = argument(input);
            result = ra3::audio::detachPlayingAudioVolumeModifiers(
                ptrFromAddr(playingAddr), ptrFromAddr(playersAddr), lockCount,
                ptrFromAddr(globalSettingsAddr),
                static_cast<const float*>(ptrFromAddr(scaleTableAddr)));
        } else if (op == "50e510") {
            const auto mgrAddr = argument(input);
            const auto key = argument(input);
            const auto initialEax = argument(input);
            result = ra3::audio::sageAudioManagerRemoveListenerEntry(
                ptrFromAddr(mgrAddr), key, initialEax);
        } else {
            std::cout << "error\n";
            std::cout.flush();
            return 1;
        }

        std::cout << std::hex << result << std::dec << " ";
        encode(g_arena, slice.size());
        std::cout << " ";
        encode(aux.data(), aux.size());
        std::cout << "\n";
        std::cout.flush();
    }
    return 0;
}
