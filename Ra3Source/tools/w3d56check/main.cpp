#include <ra3/render/W3DLayer56.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::unique_ptr<std::vector<std::uint8_t>>> g_slots;

std::string hexBytes(const std::uint8_t* data, std::size_t size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::nouppercase;
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<int>(data[i]);
    }
    return out.str().empty() ? "." : out.str();
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << value;
    return out.str();
}

std::uint32_t parseU32(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

std::vector<std::uint8_t> parseHex(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (text == ".") {
        return out;
    }
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::uint8_t* slotData(std::size_t index) {
    return g_slots.at(index)->data();
}

std::uint32_t argValue(const std::string& text) {
    if (!text.empty() && text[0] == '@') {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(slotData(std::stoul(text.substr(1)))));
    }
    return parseU32(text);
}

void printState(std::uint32_t eax, std::uint32_t st, const std::vector<std::uint8_t>& mem,
                const std::vector<int>& watch) {
    std::cout << "eax=" << hex32(eax)
              << " al=" << hex32(eax & 0xFF)
              << " st=" << hex32(st)
              << " log=" << ra3::render::w3d56TakeLog()
              << " mem=" << hexBytes(mem.data(), mem.size())
              << " pause=" << hex32(ra3::render::w3d56PauseByte())
              << " latch=" << hex32(ra3::render::w3d56Latch())
              << " eflag=" << hex32(ra3::render::w3d56ListenerFlag())
              << " ear0=" << hex32(ra3::render::w3d56ListenerBits(0))
              << " ear1=" << hex32(ra3::render::w3d56ListenerBits(1))
              << " ear2=" << hex32(ra3::render::w3d56ListenerBits(2));
    for (std::size_t i = 0; i < watch.size(); ++i) {
        const auto& slot = *g_slots.at(static_cast<std::size_t>(watch[i]));
        std::cout << " a" << i << "=" << hexBytes(slot.data(), slot.size());
    }
    std::cout << "\n";
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << std::unitbuf;
    ra3::render::w3d56SetControlWord();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            break;
        }
        if (command == "reset") {
            g_slots.clear();
            ra3::render::w3d56ResetProbe();
            ra3::render::w3d56SetControlWord();
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "frame") {
            int value = 0;
            input >> value;
            ra3::render::w3d56SetFrameMillis(value);
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "halfpi") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetHalfPiBits(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "pause") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetPauseByte(static_cast<std::uint8_t>(parseU32(bits)));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "latch") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetLatch(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "eflag") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetListenerFlag(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "queryret") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetQueryResult(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "finret") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetFinishResult(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "htail") {
            std::string bits;
            input >> bits;
            ra3::render::w3d56SetHeightBits(parseU32(bits));
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "slot") {
            std::string hex;
            input >> hex;
            auto buffer = std::make_unique<std::vector<std::uint8_t>>(parseHex(hex));
            if (buffer->empty()) {
                buffer->push_back(0);
            }
            g_slots.push_back(std::move(buffer));
            std::cout << "ptr=" << hex32(static_cast<std::uint32_t>(
                              reinterpret_cast<std::uintptr_t>(g_slots.back()->data())))
                      << " n=" << (g_slots.size() - 1) << "\n";
            continue;
        }
        if (command == "xfer") {
            std::string version;
            input >> version;
            auto buffer = std::make_unique<std::vector<std::uint8_t>>(16);
            g_slots.push_back(std::move(buffer));
            ra3::render::w3d56PrepareXfer(g_slots.back()->data(), parseU32(version));
            std::cout << "ptr=" << hex32(static_cast<std::uint32_t>(
                              reinterpret_cast<std::uintptr_t>(g_slots.back()->data())))
                      << " n=" << (g_slots.size() - 1) << "\n";
            continue;
        }
        if (command == "bind") {
            std::string kind;
            std::string index;
            input >> kind >> index;
            void* pointer = slotData(std::stoul(index));
            if (kind == "follow") {
                ra3::render::w3d56SetFollow(pointer);
            } else if (kind == "terrain") {
                ra3::render::w3d56SetTerrain(pointer);
            } else if (kind == "owner") {
                ra3::render::w3d56SetSampleOwner(pointer);
            }
            std::cout << "ok=1\n";
            continue;
        }
        if (command == "ease") {
            std::string easeHex;
            std::string pos;
            input >> easeHex >> pos;
            auto ease = parseHex(easeHex);
            const std::uint32_t bits = ra3::render::w3d56EvaluateEaseBits(ease.data(),
                                                                          [&] {
                                                                              std::uint32_t raw = parseU32(pos);
                                                                              float value = 0;
                                                                              std::memcpy(&value, &raw, 4);
                                                                              return value;
                                                                          }());
            std::cout << "st=" << hex32(bits) << "\n";
            continue;
        }
        if (command != "call") {
            std::cout << "err=cmd\n";
            continue;
        }
        std::string name;
        std::string thisHex;
        input >> name >> thisHex;
        std::vector<std::string> args;
        std::vector<int> watch;
        std::string token;
        bool watching = false;
        while (input >> token) {
            if (token == "watch") {
                watching = true;
                continue;
            }
            if (watching) {
                watch.push_back(std::stoi(token));
            } else {
                args.push_back(token);
            }
        }
        auto mem = parseHex(thisHex);
        if (mem.empty()) {
            mem.push_back(0);
        }
        void* self = mem.data();
        auto arg = [&](std::size_t index) { return argValue(args.at(index)); };
        auto ptr = [&](std::size_t index) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg(index))); };
        std::uint32_t eax = 0;
        std::uint32_t st = 0;
        ra3::render::w3d56SetControlWord();
        if (name == "track") {
            ra3::render::view56BeginTrackedMove(self, arg(0), arg(1), arg(2), arg(3), arg(4));
        } else if (name == "trackheld") {
            ra3::render::view56BeginTrackedMoveHeld(self, arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        } else if (name == "uzoom") {
            ra3::render::view56UpdateZoom(self);
        } else if (name == "upitch") {
            ra3::render::view56UpdatePitch(self);
        } else if (name == "uheight") {
            ra3::render::view56UpdateHeight(self);
        } else if (name == "uangle") {
            ra3::render::view56UpdateAngle(self);
        } else if (name == "pause") {
            ra3::render::view56SetPauseFlag(self, arg(0));
        } else if (name == "sphere") {
            eax = ra3::render::view56SphereVisible(self, ptr(0), arg(1));
        } else if (name == "listen") {
            eax = ra3::render::view56PublishListener(self);
        } else if (name == "follow") {
            st = ra3::render::view56FollowHeightRatio(self, arg(0));
        } else if (name == "bzoom") {
            ra3::render::view56BeginZoom(self, arg(0), arg(1), arg(2), arg(3));
        } else if (name == "bheight") {
            ra3::render::view56BeginHeight(self, arg(0), arg(1), arg(2), arg(3));
        } else if (name == "bangle") {
            ra3::render::view56BeginAngle(self, arg(0), arg(1), arg(2), arg(3));
        } else if (name == "samples") {
            ra3::render::view56TranslateSamples(self, ptr(0));
        } else if (name == "clearmotion") {
            ra3::render::view56ClearCameraMotion(self);
        } else if (name == "efloat") {
            st = ra3::render::view56EmbeddedFlagFloat(self);
        } else if (name == "eset4") {
            ra3::render::view56EmbeddedSet4(self, arg(0));
        } else if (name == "eset8") {
            ra3::render::view56EmbeddedSet8(self, arg(0));
        } else if (name == "eset12") {
            ra3::render::view56EmbeddedSet12(self, arg(0));
        } else if (name == "eset16") {
            ra3::render::view56EmbeddedSet16(self, arg(0));
        } else if (name == "bcopy") {
            eax = ra3::render::bounds56Copy(self, ptr(0));
        } else if (name == "bassign") {
            ra3::render::bounds56Assign(self, arg(0), ptr(1));
        } else if (name == "binter") {
            ra3::render::bounds56Intersect(self, ptr(0));
        } else if (name == "extent") {
            ra3::render::terrain56FillExtent(self, ptr(0));
        } else if (name == "height") {
            ra3::render::terrain56HeightOrZero(self, arg(0), arg(1), &eax, &st);
        } else if (name == "synccell") {
            ra3::render::terrain56SyncCell(self, arg(0));
        } else if (name == "txfer") {
            ra3::render::terrain56XferHeader(self, ptr(0));
        } else if (name == "fwda") {
            ra3::render::terrain56ForwardA(self, arg(0), arg(1), arg(2));
        } else if (name == "fwdb") {
            ra3::render::terrain56ForwardB(self, arg(0), arg(1), arg(2));
        } else if (name == "fwdc") {
            ra3::render::terrain56ForwardScorch(self, arg(0), arg(1));
        } else if (name == "sample") {
            eax = ra3::render::terrain56SampleIndex(self, arg(0), arg(1));
        } else if (name == "latch") {
            eax = ra3::render::client56Latch(self, arg(0), arg(1));
        } else if (name == "setflag") {
            ra3::render::client56SetFlag(self, arg(0));
        } else if (name == "wvec3") {
            ra3::render::record56WriteVec3(self, ptr(0), arg(1));
        } else if (name == "weq") {
            ra3::render::record56WriteFloatUnlessEqual(self, ptr(0), arg(1));
        } else if (name == "wvec4") {
            ra3::render::record56WriteVec4(self, ptr(0), arg(1));
        } else if (name == "wpair") {
            ra3::render::record56WritePair(self, ptr(0), arg(1));
        } else if (name == "transpose") {
            ra3::render::mat56Transpose(ptr(0), ptr(1));
        } else if (name == "pose") {
            ra3::render::pose56CopyTranslation(self, ptr(0));
        } else if (name == "scorch") {
            ra3::render::scorch56Xfer(self, ptr(0));
        } else if (name == "easereset") {
            ra3::render::ease56ResetTrack(self, arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        } else {
            std::cout << "err=name\n";
            continue;
        }
        printState(eax, st, mem, watch);
    }
    return 0;
}
