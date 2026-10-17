#include "ra3/gameclient/GameClient52.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string g_log;

void addLog(const std::string& text)
{
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

std::uint32_t loadU32(const void* address)
{
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value)
{
    std::memcpy(address, &value, 4);
}

void putFn(std::uint8_t* table, int offset, const void* function)
{
    storeU32(table + offset, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function)));
}

float bitsToFloat(std::uint32_t bits)
{
    float value = 0.f;
    std::memcpy(&value, &bits, 4);
    return value;
}

std::uint32_t parseU32(const std::string& token)
{
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 16));
}

std::string hex32(std::uint32_t value)
{
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
}

std::string hexBytes(const std::uint8_t* data, std::size_t size)
{
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        text << std::setw(2) << static_cast<int>(data[i]);
    }
    return text.str();
}

void setControl()
{
    unsigned short control = 0x037F;
    __asm { fldcw control }
}

void captureFloat80(float value, std::uint8_t encoded[10])
{
    setControl();
    __asm {
        lea eax, value
        fld dword ptr [eax]
        mov ecx, encoded
        fstp tbyte ptr [ecx]
    }
}

void __fastcall notify16(void*, void*)
{
    addLog("n16");
}

void __fastcall setField28(void* self, void*, float value)
{
    std::memcpy(static_cast<std::uint8_t*>(self) + 0x28, &value, 4);
}

void __fastcall setField30(void* self, void*, float value)
{
    std::memcpy(static_cast<std::uint8_t*>(self) + 0x30, &value, 4);
}

float __fastcall getField28(void* self, void*)
{
    float value = 0.f;
    std::memcpy(&value, static_cast<std::uint8_t*>(self) + 0x28, 4);
    return value;
}

struct Reply {
    std::uint32_t eax = 0;
    std::string st0 = ".";
    std::string o0 = ".";
    std::string o1 = ".";
    std::string out = ".";
};

std::uint8_t g_obj[0xC0];
std::uint8_t g_vt[0x220];
std::uint8_t g_point[0x20];
std::uint8_t g_state[0x20];
std::uint8_t g_out[32];
std::uint32_t g_origin[2];

void fillObject()
{
    std::memset(g_obj, 0xA5, sizeof g_obj);
    std::memset(g_vt, 0x11, sizeof g_vt);
    putFn(g_vt, 0x40, &notify16);
    putFn(g_vt, 0x48, reinterpret_cast<const void*>(&ra3::gameclient::View_setPositionFromScreen));
    putFn(g_vt, 0xC0, &setField28);
    putFn(g_vt, 0xC4, &getField28);
    putFn(g_vt, 0xC8, reinterpret_cast<const void*>(&ra3::gameclient::View_setAngleClamped));
    putFn(g_vt, 0xCC, reinterpret_cast<const void*>(&ra3::gameclient::View_getAngle));
    putFn(g_vt, 0xD0, &setField30);
    putFn(g_vt, 0xD4, reinterpret_cast<const void*>(&ra3::gameclient::View_getPitch));
    putFn(g_vt, 0xF4, reinterpret_cast<const void*>(&ra3::gameclient::View_getFov));
    putFn(g_vt, 0xF8, reinterpret_cast<const void*>(&ra3::gameclient::View_setFov));
    storeU32(g_obj, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_vt)));
    g_log.clear();
    std::memset(g_out, 0x5A, sizeof g_out);
}

using F1 = void(__fastcall*)(void*, void*, float);
using U1 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
using GetF = float(__fastcall*)(void*, void*);

void callFloatSetter(F1 function, std::uint32_t bits)
{
    function(g_obj, nullptr, bitsToFloat(bits));
}

std::uint32_t callDwordSetter(U1 function, std::uint32_t bits)
{
    return function(g_obj, nullptr, bits);
}

void callFloatGetter(GetF function, Reply& reply)
{
    std::uint8_t encoded[10]{};
    captureFloat80(function(g_obj, nullptr), encoded);
    reply.st0 = hexBytes(encoded, 10);
}

bool samePtr(const void* left, const void* right)
{
    return left == right;
}

void dispatch(const std::string& name, const std::vector<std::uint32_t>& args, Reply& reply)
{
    using namespace ra3::gameclient;
    setControl();
    if (name == "setfov") {
        callFloatSetter(&View_setFov, args.at(0));
    } else if (name == "setheight") {
        callFloatSetter(&View_setHeight, args.at(0));
    } else if (name == "setmax") {
        callFloatSetter(&View_setMaxHeight, args.at(0));
    } else if (name == "setterr") {
        callFloatSetter(&View_setTerrainHeight, args.at(0));
    } else if (name == "setextra") {
        callFloatSetter(&View_setExtraPitch, args.at(0));
    } else if (name == "setnear") {
        callFloatSetter(&View_setNearClip, args.at(0));
    } else if (name == "setfar") {
        callFloatSetter(&View_setFarClip, args.at(0));
    } else if (name == "setshake") {
        View_setShakeRange(g_obj, nullptr, bitsToFloat(args.at(0)), bitsToFloat(args.at(1)));
    } else if (name == "getangle") {
        callFloatGetter(&View_getAngle, reply);
    } else if (name == "getpitch") {
        callFloatGetter(&View_getPitch, reply);
    } else if (name == "getzoom") {
        callFloatGetter(&View_getZoom, reply);
    } else if (name == "getfov") {
        callFloatGetter(&View_getFov, reply);
    } else if (name == "getheight") {
        callFloatGetter(&View_getHeight, reply);
    } else if (name == "getmax") {
        callFloatGetter(&View_getMaxHeight, reply);
    } else if (name == "getterr") {
        callFloatGetter(&View_getTerrainHeight, reply);
    } else if (name == "getzcur") {
        callFloatGetter(&View_getZoomCurrent, reply);
    } else if (name == "getextra") {
        callFloatGetter(&View_getExtraPitch, reply);
    } else if (name == "getshake") {
        callFloatGetter(&View_getShake, reply);
    } else if (name == "getnear") {
        callFloatGetter(&View_getNearClip, reply);
    } else if (name == "getfar") {
        callFloatGetter(&View_getFarClip, reply);
    } else if (name == "setlock") {
        reply.eax = View_setLockByte(g_obj, nullptr, args.at(0));
    } else if (name == "setwide") {
        reply.eax = View_setViewportWidth(g_obj, nullptr, args.at(0));
    } else if (name == "settall") {
        reply.eax = View_setViewportHeight(g_obj, nullptr, args.at(0));
    } else if (name == "setorigin") {
        reply.eax = View_setViewportOrigin(g_obj, nullptr, args.at(0), args.at(1));
    } else if (name == "getorigin") {
        reply.eax = View_getViewportOrigin(g_obj, nullptr, &g_origin[0], &g_origin[1]);
        reply.o0 = hex32(g_origin[0]);
        reply.o1 = hex32(g_origin[1]);
    } else if (name == "screen") {
        storeU32(g_point, args.at(0));
        storeU32(g_point + 4, args.at(1));
        reply.eax = View_setPositionFromScreen(g_obj, nullptr, g_point);
    } else if (name == "addxy") {
        storeU32(g_point, args.at(0));
        storeU32(g_point + 4, args.at(1));
        reply.eax = View_addPositionXY(g_obj, nullptr, g_point);
    } else if (name == "screenif") {
        const void* point = nullptr;
        if (args.size() >= 2) {
            storeU32(g_point, args.at(0));
            storeU32(g_point + 4, args.at(1));
            point = g_point;
        }
        reply.eax = View_setPositionFromScreenIf(g_obj, nullptr, point, 0, 0, 0, 0, 0);
    } else if (name == "clamp") {
        View_setAngleClamped(g_obj, nullptr, bitsToFloat(args.at(0)));
    } else if (name == "push") {
        View_pushZoom(g_obj, nullptr, bitsToFloat(args.at(0)));
    } else if (name == "shake") {
        View_addShakeMagnitude(g_obj, nullptr, bitsToFloat(args.at(0)));
    } else if (name == "copydef") {
        View_copyAnglePitchDefaults(g_obj, nullptr);
    } else if (name == "init") {
        View_resetCameraDefaults(g_obj, nullptr);
    } else if (name == "viewport") {
        View_initDefaultViewport(g_obj, nullptr);
    } else if (name == "scaledown") {
        View_scaleFovDown(g_obj, nullptr);
    } else if (name == "scaleup") {
        View_scaleFovUp(g_obj, nullptr);
    } else if (name == "setb7d") {
        reply.eax = View_setByte7D(g_obj, nullptr, args.at(0));
    } else if (name == "clearf") {
        reply.eax = View_clearFilter(g_obj, nullptr, args.at(0));
    } else if (name == "ready") {
        View_setFilterReady(g_obj, nullptr);
    } else if (name == "setf") {
        reply.eax = View_setFilter(g_obj, nullptr, args.at(0), bitsToFloat(args.at(1)));
    } else if (name == "getmode") {
        reply.eax = View_getZoomMode(g_obj, nullptr);
    } else if (name == "setmode") {
        reply.eax = View_setZoomMode(g_obj, nullptr, args.at(0));
    } else if (name == "clearz") {
        reply.eax = View_clearZoomFilter(g_obj, nullptr);
    } else if (name == "setcam") {
        reply.eax = View_setCameraByte(g_obj, nullptr, args.at(0));
    } else if (name == "getflag") {
        reply.eax = View_getGuardFlag(g_obj, nullptr);
    } else if (name == "getgid") {
        reply.eax = View_getGuardId(g_obj, nullptr);
    } else if (name == "setgid") {
        reply.eax = View_setGuardId(g_obj, nullptr, args.at(0));
    } else if (name == "setg") {
        reply.eax = View_setGuardPosition(g_obj, nullptr, args.at(0), args.at(1));
    } else if (name == "clearg") {
        reply.eax = View_clearGuardPosition(g_obj, nullptr);
    } else if (name == "hasg") {
        reply.eax = View_hasGuardPosition(g_obj, nullptr);
    } else if (name == "copyg") {
        reply.eax = samePtr(View_copyGuardPosition(g_obj, nullptr, g_out), g_out) ? 1u : 0u;
        reply.out = hexBytes(g_out, 32);
    } else if (name == "copypos") {
        reply.eax = samePtr(View_copyPosition(g_obj, nullptr, g_out), g_out) ? 1u : 0u;
        reply.out = hexBytes(g_out, 32);
    } else if (name == "setid") {
        reply.eax = samePtr(View_setViewId(g_obj, nullptr, args.at(0)), g_obj) ? 1u : 0u;
    } else if (name == "capture") {
        reply.eax = samePtr(View_captureCamera(g_obj, nullptr, g_out), g_out) ? 1u : 0u;
        reply.out = hexBytes(g_out, 32);
    } else if (name == "apply") {
        std::memset(g_state, 0, sizeof g_state);
        g_state[0] = static_cast<std::uint8_t>(args.at(0));
        storeU32(g_state + 4, args.at(1));
        storeU32(g_state + 8, args.at(2));
        storeU32(g_state + 0x10, args.at(3));
        storeU32(g_state + 0x14, args.at(4));
        storeU32(g_state + 0x18, args.at(5));
        storeU32(g_state + 0x1C, args.at(6));
        View_applyCamera(g_obj, nullptr, g_state);
    } else if (name == "distfov") {
        View_distanceFromFov(g_obj, nullptr, bitsToFloat(args.at(0)), g_out);
        reply.st0 = hexBytes(g_out, 10);
    } else if (name == "disthalf") {
        View_distanceFromHalfAngle(g_obj, nullptr, bitsToFloat(args.at(0)), g_out);
        reply.st0 = hexBytes(g_out, 10);
    } else {
        throw std::runtime_error("unknown " + name);
    }
}

std::string memList(const std::vector<int>& offsets)
{
    if (offsets.empty()) {
        return ".";
    }
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        if (i != 0) {
            text << ",";
        }
        text << offsets[i] << ":" << std::setw(8) << std::setfill('0') << loadU32(g_obj + offsets[i]);
    }
    return text.str();
}

const std::vector<std::string> kWords{"fill", "u", "call", "mem"};

bool isWord(const std::string& token)
{
    for (const auto& word : kWords) {
        if (token == word) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "quit\r") {
            break;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream stream(line);
        std::vector<std::string> tokens;
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
        try {
            Reply reply;
            std::vector<int> mem;
            std::string name;
            std::vector<std::uint32_t> args;
            bool called = false;
            for (std::size_t i = 0; i < tokens.size();) {
                const std::string& word = tokens[i];
                if (word == "fill") {
                    fillObject();
                    ++i;
                } else if (word == "u") {
                    storeU32(g_obj + static_cast<int>(parseU32(tokens.at(i + 1))), parseU32(tokens.at(i + 2)));
                    i += 3;
                } else if (word == "call") {
                    name = tokens.at(i + 1);
                    i += 2;
                    args.clear();
                    while (i < tokens.size() && !isWord(tokens[i])) {
                        args.push_back(parseU32(tokens[i]));
                        ++i;
                    }
                    dispatch(name, args, reply);
                    called = true;
                } else if (word == "mem") {
                    ++i;
                    while (i < tokens.size() && !isWord(tokens[i])) {
                        mem.push_back(static_cast<int>(parseU32(tokens[i])));
                        ++i;
                    }
                } else {
                    throw std::runtime_error("bad token");
                }
            }
            if (!called) {
                throw std::runtime_error("no call");
            }
            std::cout << "eax=" << hex32(reply.eax)
                      << " log=" << (g_log.empty() ? "." : g_log)
                      << " mem=" << memList(mem)
                      << " st0=" << reply.st0
                      << " o0=" << reply.o0
                      << " o1=" << reply.o1
                      << " out=" << reply.out
                      << "\n";
        } catch (const std::exception& error) {
            std::cout << "err=" << error.what() << "\n";
        }
    }
    return 0;
}
