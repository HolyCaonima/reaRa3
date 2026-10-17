#include "ra3/gamelogic/ProtectedCaveLogic2.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using ra3::gamelogic::CaveState;
using ra3::gamelogic::kCaveImageBase;
using ra3::gamelogic::kCaveImageSize;

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseHex(const std::string& text, std::uint8_t* out, std::size_t bytes) {
    if (text.size() != bytes * 2u) return false;
    for (std::size_t i = 0; i < bytes; ++i) {
        const int hi = hexVal(text[i * 2u]);
        const int lo = hexVal(text[i * 2u + 1u]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::vector<std::uint32_t> parseList(const std::string& text) {
    std::vector<std::uint32_t> out;
    if (text == ".") return out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t comma = text.find(',', pos);
        const std::string part = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        out.push_back(static_cast<std::uint32_t>(std::stoul(part, nullptr, 16)));
        if (comma == std::string::npos) break;
        pos = comma + 1u;
    }
    return out;
}

void parseGlobals(CaveState& st, const std::string& text) {
    st.globalCount = 0;
    if (text == ".") return;
    std::size_t pos = 0;
    while (pos < text.size() && st.globalCount < 12) {
        const std::size_t comma = text.find(',', pos);
        const std::string part = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) return;
        st.globalAddr[st.globalCount] = static_cast<std::uint32_t>(std::stoul(part.substr(0, eq), nullptr, 16));
        st.globalVal[st.globalCount] = static_cast<std::uint32_t>(std::stoul(part.substr(eq + 1u), nullptr, 16));
        ++st.globalCount;
        if (comma == std::string::npos) break;
        pos = comma + 1u;
    }
}

void printHex(const std::uint8_t* bytes, std::size_t n) {
    static const char kHex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < n; ++i) {
        std::cout << kHex[bytes[i] >> 4] << kHex[bytes[i] & 0x0F];
    }
}

void dispatch(const std::string& name, CaveState& st, const std::vector<std::uint32_t>& args) {
    const std::uint32_t a0 = args.size() > 0u ? args[0] : 0u;
    const std::uint32_t a1 = args.size() > 1u ? args[1] : 0u;
    const std::uint32_t a2 = args.size() > 2u ? args[2] : 0u;
    const std::uint32_t a3 = args.size() > 3u ? args[3] : 0u;
    const std::uint32_t a4 = args.size() > 4u ? args[4] : 0u;
    if (name == "spready") ra3::gamelogic::runNotifySpecialPowerReady(st, a0);
    else if (name == "stopstream") ra3::gamelogic::runStopSubChannelStream(st);
    else if (name == "envelope") ra3::gamelogic::runUpdateSubChannelEnvelope(st, a0);
    else if (name == "createref") ra3::gamelogic::runCreateRefCountedStateObject(st, a0);
    else if (name == "xformview") ra3::gamelogic::runApplyTransformAndRefreshView(st, a0, a1, a2, a3, a4);
    else if (name == "teamentry") ra3::gamelogic::runResolveObjectTeamOrPlayerEntry(st);
    else if (name == "flushdirty") ra3::gamelogic::runFlushDirtySubObjectsAndNotify(st, a0);
    else if (name == "terrhelper") ra3::gamelogic::runInitTerrainRenderHelper(st, a0);
    else if (name == "projstr") ra3::gamelogic::runInitLocalizedProjectileString(st, a0);
    else if (name == "destroyarr") ra3::gamelogic::runDestroySubObjectArrayBuffer(st);
    else if (name == "tblentry") ra3::gamelogic::runIsIndexedTableEntryEmpty(st, a0);
    else if (name == "dtorgameinfo") ra3::gamelogic::runDestroyGameInfoBase(st);
    else if (name == "resetgameslot") ra3::gamelogic::runResetGameSlot(st);
    else if (name == "regmap") ra3::gamelogic::runRegisterEntryInTaggedMap(st, a0);
    else if (name == "findmodsub") ra3::gamelogic::runFindModuleMatchingSubObject(st, a0, a1);
    else if (name == "kindpred") ra3::gamelogic::runInitKindOfFilterPredicate(st, a0, a1, a2);
    else if (name == "diffcontain") ra3::gamelogic::runHaveDifferentContainerStatuses(st, a0, a1);
    else if (name == "wpnaudio") ra3::gamelogic::runTriggerWeaponInitAudioEvent(st);
    else if (name == "indcolor") ra3::gamelogic::runResolveObjectIndicatorColor(st);
    else if (name == "powerradar") ra3::gamelogic::runUpdateObjectPowerRadarStatusBit(st, a0);
    else if (name == "vec3cmd") ra3::gamelogic::runDispatchVec3CommandMessage(st, a0, a1);
    else if (name == "tgtcmd3d") ra3::gamelogic::runDispatchTargetCommandMessage3D(st, a0, a1);
    else if (name == "dtorbody") ra3::gamelogic::runDestroyActiveBodyBase(st);
    else if (name == "execsp") ra3::gamelogic::runTryExecuteSpecialPowerOnTarget(st, a0, a1, a2);
    else if (name == "refreshdraw") ra3::gamelogic::runRefreshObjectModulesAndDrawableState(st);
    else if (name == "findsel44") ra3::gamelogic::runFindSelectableObjectAndQueryModule44(st, a0, a1);
    else if (name == "producer") ra3::gamelogic::runResolveObjectByProducerId(st);
    else if (name == "mobile") ra3::gamelogic::runIsObjectMobileAndControllable(st);
    else if (name == "tintenv") ra3::gamelogic::runApplyDrawableTintEnvelope(st, a0);
    else if (name == "wpnaim") ra3::gamelogic::runCopyWeaponAimTransformAndUpdate(st, a0, a1, a2);
    else throw std::runtime_error("unknown op: " + name);
}

}  // namespace

int main() {
#ifdef _WIN32
    void* mem = VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kCaveImageBase)),
                             kCaveImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mem != reinterpret_cast<void*>(static_cast<std::uintptr_t>(kCaveImageBase))) {
        std::cerr << "image alloc failed\n";
        return 2;
    }
    auto* image = static_cast<std::uint8_t*>(mem);
#else
    static std::uint8_t image[kCaveImageSize];
#endif
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line == "quit") return 0;
        std::istringstream iss(line);
        std::string name;
        std::string obj;
        std::string argsText;
        std::string retsText;
        std::string globText;
        iss >> name >> obj >> argsText >> retsText >> globText;
        if (!parseHex(obj, image, kCaveImageSize)) {
            std::cerr << "bad image\n";
            return 3;
        }
        const std::vector<std::uint32_t> args = parseList(argsText);
        const std::vector<std::uint32_t> rets = parseList(retsText);
        CaveState st{};
        st.image = image;
        st.retCount = static_cast<int>(rets.size());
        for (int i = 0; i < st.retCount && i < 16; ++i) {
            st.rets[i] = rets[static_cast<std::size_t>(i)];
        }
        parseGlobals(st, globText);
        try {
            dispatch(name, st, args);
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return 4;
        }
        std::cout << std::hex;
        std::cout << "eax=" << st.eax << " edx=" << st.edx << " clean=" << std::dec << st.cleanup << " cbs=";
        if (st.callCount == 0) {
            std::cout << ".";
        } else {
            for (int i = 0; i < st.callCount; ++i) {
                if (i != 0) std::cout << ";";
                const auto& c = st.calls[i];
                std::cout << std::hex << c.target << ":" << c.ecx << ":" << c.a0 << ":" << c.a1
                          << ":" << c.a2 << ":" << std::dec << c.nstack << ":" << std::hex << c.result;
            }
        }
        std::cout << " glob=";
        if (st.globalCount == 0) {
            std::cout << ".";
        } else {
            for (int i = 0; i < st.globalCount; ++i) {
                if (i != 0) std::cout << ",";
                std::cout << std::hex << st.globalAddr[i] << "=" << st.globalVal[i];
            }
        }
        std::cout << " obj=";
        printHex(image, kCaveImageSize);
        std::cout << "\n" << std::dec;
        std::cout.flush();
    }
    return 0;
}
