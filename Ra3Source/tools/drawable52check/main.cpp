#include <ra3/gameclient/DrawableAccess.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ra3::gameclient::DrawableServices;
using ra3::gameclient::PositionGlobals;
using ra3::gameclient::TransformGlobals;

struct Named {
    const char* name;
    std::uint32_t address;
};

struct ScriptHost : DrawableServices {
    std::vector<std::uint32_t> results;
    std::size_t cursor = 0;
    std::vector<Named> names;
    std::string trace;
    bool stamp = false;
    int versionByte = -1;
    void* inner = nullptr;
    void* allocation = nullptr;
    bool failAlloc = false;
    int randomIndex = 0;

    std::uint32_t take()
    {
        if (cursor >= results.size()) {
            return 0;
        }
        const std::uint32_t value = results[cursor++];
        if (value == 1 && inner != nullptr) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(inner));
        }
        return value;
    }

    std::string token(std::uint32_t value) const
    {
        for (const Named& named : names) {
            if (named.address == value) {
                return named.name;
            }
        }
        if (value > 0xFFFF && value != 0x00BDFEACu) {
            return "p";
        }
        char text[16];
        std::snprintf(text, sizeof(text), "%x", value);
        return text;
    }

    void note(const char* kind, std::uint32_t id, void* object, const std::uint32_t* args, int count)
    {
        char head[32];
        std::snprintf(head, sizeof(head), "%s%x@%s", kind, id, token(static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(object))).c_str());
        trace += head;
        for (int index = 0; index < count; ++index) {
            trace += ":";
            trace += token(args[index]);
        }
        trace += ";";
    }

    std::uint32_t callSlot(
        void* object, std::uint32_t slot, const std::uint32_t* args, int argCount,
        std::uint32_t) override
    {
        note("s", slot, object, args, argCount);
        if (slot == 0x18 && argCount == 1 && versionByte >= 0) {
            reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args[0]))[1] =
                static_cast<std::uint8_t>(versionByte);
        }
        if (stamp && argCount == 1 &&
            (slot == 0x58 || slot == 0x64 || slot == 0x6C || slot == 0x80 || slot == 0x84)) {
            const std::uint32_t marker = 0xA1000000u | slot;
            std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(args[0])), &marker, 4);
        }
        return take();
    }

    std::uint32_t callDirect(
        std::uint32_t address, void* ecx, const std::uint32_t* args, int argCount,
        std::uint32_t) override
    {
        note("d", address, ecx, args, argCount);
        if (address == 0x0055A0D0 && argCount == 1 && allocation != nullptr) {
            // The looked-up pointer is written by the real callee into its out
            // object.  Tests that need a non-zero local pass it as the first
            // scripted result and the host stores it at ecx, which is &local
            // only when the caller used a named local.  Leave locals alone.
            (void)args;
        }
        return take();
    }

    void* allocate(std::uint32_t bytes) override
    {
        trace += "alloc:" + std::to_string(bytes) + ";";
        if (failAlloc) {
            return nullptr;
        }
        auto* block = new std::uint8_t[bytes];
        std::memset(block, 0xA5, bytes);
        allocation = block;
        return block;
    }

    float randomReal(float, float, std::uint32_t) override
    {
        const float value = randomIndex++ == 0 ? 0.25f : 0.5f;
        return value;
    }
};

struct Mods {
    std::uint32_t body[4]{};
    void* list[5]{};
    int count = 0;
    explicit Mods(int moduleCount)
        : count(moduleCount)
    {
        relink();
    }
    void relink()
    {
        for (int index = 0; index < count; ++index) {
            list[index] = &body[index];
        }
        list[count] = nullptr;
    }
};

std::uint32_t addr(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::string hexBytes(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::string text;
    text.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        char part[3];
        std::snprintf(part, sizeof(part), "%02x", bytes[index]);
        text += part;
    }
    return text;
}

std::string tagOf(
    std::uint32_t eax, const std::vector<Named>& exact, const std::vector<Named>& ahBases,
    std::uint32_t incoming)
{
    for (const Named& named : exact) {
        if (eax == named.address) {
            return named.name;
        }
    }
    if ((eax & 0xFFFFFF00u) == (incoming & 0xFFFFFF00u)) {
        char text[16];
        std::snprintf(text, sizeof(text), "in-al:%02x", eax & 0xFF);
        return text;
    }
    for (const Named& named : ahBases) {
        if (eax != named.address && (eax & 0xFFFF00FFu) == (named.address & 0xFFFF00FFu)) {
            char text[48];
            std::snprintf(text, sizeof(text), "%s-ah:%02x", named.name, (eax >> 8) & 0xFF);
            return text;
        }
    }
    for (const Named& named : ahBases) {
        if (eax != named.address && (eax & 0xFFFFFF00u) == (named.address & 0xFFFFFF00u)) {
            char text[48];
            std::snprintf(text, sizeof(text), "%s-al:%02x", named.name, eax & 0xFF);
            return text;
        }
    }
    char text[16];
    std::snprintf(text, sizeof(text), "imm:%08x", eax);
    return text;
}

void emit(
    std::uint32_t eax, const std::vector<Named>& exact, const std::vector<Named>& ahBases,
    std::uint32_t incoming, const std::string& mem, const std::string& trace)
{
    std::cout << "tag=" << tagOf(eax, exact, ahBases, incoming) << " mem=" << mem
              << " trace=" << (trace.empty() ? "-" : trace) << "\n";
}

void fill(std::uint8_t* object, std::size_t size)
{
    std::memset(object, 0xA5, size);
}

void put32(void* object, std::uint32_t offset, std::uint32_t value)
{
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, 4);
}

void putFloat(void* object, std::uint32_t offset, float value)
{
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, 4);
}

std::uint32_t read32(const void* object, std::uint32_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, 4);
    return value;
}

void run(const std::string& line)
{
    std::istringstream input(line);
    std::string command;
    input >> command;
    constexpr std::uint32_t kIn = 0xA5A5A500u;
    if (command == "tint") {
        emit(ra3::gameclient::tintEnvelopeStatic(), {}, {}, 0, "-", "-");
        return;
    }
    if (command == "copy") {
        int mode = 0;
        input >> mode;
        std::uint32_t source[16]{};
        for (int index = 0; index < 12; ++index) {
            source[index] = 0x01000000u + static_cast<std::uint32_t>(index) * 0x1111u;
        }
        source[3] = 0x7FC00000u;
        std::uint32_t destination[16]{};
        std::memset(destination, 0x5A, sizeof(destination));
        void* dest = destination;
        const void* src = source;
        if (mode == 1) {
            dest = source;
            src = source;
        } else if (mode == 2) {
            dest = source + 1;
            src = source;
        }
        const std::uint32_t eax = ra3::gameclient::copyTwelveDwords(dest, src);
        std::vector<Named> exact{{"dest", addr(dest)}};
        const std::string mem = mode == 0 ? hexBytes(destination, 48) : hexBytes(source, 64);
        emit(eax, exact, {}, 0, mem, "-");
        return;
    }
    if (command == "position") {
        int field = 0;
        int ready = 0;
        input >> field >> ready;
        std::uint8_t object[0x140];
        fill(object, sizeof(object));
        put32(object, 0x130, static_cast<std::uint32_t>(field));
        PositionGlobals globals{};
        globals.ready = static_cast<std::uint32_t>(ready);
        globals.xyz[0] = 4.f;
        globals.xyz[1] = 5.f;
        globals.xyz[2] = 6.f;
        const std::uint32_t eax = ra3::gameclient::positionPointer(object, &globals);
        std::vector<Named> exact{{"self+124", addr(object + 0x124)}, {"pos", addr(&globals)}};
        emit(eax, exact, {}, 0, hexBytes(&globals, sizeof(globals)), "-");
        return;
    }
    if (command == "transform") {
        int field = 0;
        int ready = 0;
        input >> field >> ready;
        std::uint8_t object[0x140];
        fill(object, sizeof(object));
        put32(object, 0x130, static_cast<std::uint32_t>(field));
        TransformGlobals globals{};
        globals.ready = static_cast<std::uint32_t>(ready);
        for (float& cell : globals.cells) {
            cell = 9.f;
        }
        const std::uint32_t eax = ra3::gameclient::transformPointer(object, &globals);
        std::vector<Named> exact{{"self+f4", addr(object + 0xF4)}, {"xfm", addr(&globals)}};
        emit(eax, exact, {}, 0, hexBytes(&globals, sizeof(globals)), "-");
        return;
    }
    if (command == "slot20" || command == "query" || command == "scalar" || command == "condition" ||
        command == "argument" || command == "seta" || command == "setb") {
        int count = 0;
        input >> count;
        int secondCount = 0;
        if (command == "scalar" || command == "argument") {
            input >> secondCount;
        }
        Mods mods(count);
        Mods second(secondCount);
        ScriptHost host;
        int resultCount = 0;
        input >> resultCount;
        for (int index = 0; index < resultCount; ++index) {
            std::uint32_t value = 0;
            input >> std::hex >> value >> std::dec;
            host.results.push_back(value);
        }
        std::uint8_t object[0x400];
        fill(object, sizeof(object));
        put32(object, 0x178, addr(mods.list));
        if (command == "scalar" || command == "argument") {
            put32(object, 0x180, secondCount == 0 && command == "argument" ? 0 : addr(second.list));
        }
        for (int index = 0; index < count; ++index) {
            host.names.push_back({"m0", addr(&mods.body[index])});
            host.names.back().name = index == 0 ? "m0" : index == 1 ? "m1" : index == 2 ? "m2" : "m3";
        }
        std::uint32_t innerBody = 0;
        if (command == "query") {
            host.inner = &innerBody;
            host.names.push_back({"i0", addr(&innerBody)});
        }
        host.names.push_back({"list", addr(mods.list)});
        if (secondCount > 0) {
            host.names.push_back({"n0", addr(&second.body[0])});
            if (secondCount > 1) {
                host.names.push_back({"n1", addr(&second.body[1])});
            }
        }
        std::uint32_t eax = 0;
        std::uint32_t arg0 = 0;
        std::uint32_t arg1 = 0;
        std::uint32_t bits = 0;
        if (command == "slot20") {
            eax = ra3::gameclient::visitModules20(object, host, kIn);
        } else if (command == "query") {
            input >> arg0;
            eax = ra3::gameclient::queryModulesB0(object, arg0, host, kIn);
        } else if (command == "scalar") {
            input >> arg0 >> arg1 >> std::hex >> bits;
            eax = ra3::gameclient::publishScalar(object, arg0, arg1, bits, host, kIn);
        } else if (command == "condition") {
            int b25b = 0;
            int b13d = 0;
            int b13e = 0;
            int b25a = 0;
            int hasObject = 0;
            input >> b25b >> b13d >> b13e >> b25a >> hasObject;
            object[0x25B] = static_cast<std::uint8_t>(b25b);
            object[0x13D] = static_cast<std::uint8_t>(b13d);
            object[0x13E] = static_cast<std::uint8_t>(b13e);
            object[0x25A] = static_cast<std::uint8_t>(b25a);
            std::uint32_t gameObject = 0x11;
            if (hasObject) {
                put32(object, 0x138, addr(&gameObject));
                host.names.push_back({"obj", addr(&gameObject)});
            }
            eax = ra3::gameclient::publishCondition(object, host, kIn);
        } else if (command == "argument") {
            input >> arg0;
            if (secondCount == 0) {
                put32(object, 0x180, 0);
            } else {
                put32(object, 0x180, addr(second.list));
            }
            eax = ra3::gameclient::publishArgument(object, arg0, host, kIn);
        } else if (command == "seta" || command == "setb") {
            input >> arg0;
            int current = 0;
            int b25b = 0;
            input >> current >> b25b;
            object[0x13D] = 0;
            object[0x13E] = 0;
            object[0x25A] = static_cast<std::uint8_t>(current);
            object[0x25B] = static_cast<std::uint8_t>(b25b);
            eax = command == "seta" ? ra3::gameclient::assignByte25A(object, arg0, host, kIn)
                                    : ra3::gameclient::assignByte25B(object, arg0, host, kIn);
        }
        std::vector<Named> exact{{"in", kIn}, {"list", addr(mods.list)}};
        for (const std::uint32_t value : host.results) {
            exact.push_back({"imm", value});
        }
        // Exact names must be unique.  Scripted returns are reported as imm by
        // falling through when they are not equal to a named pointer.
        exact.erase(exact.begin() + 2, exact.end());
        std::string mem = hexBytes(object + 0x13C, 4);
        mem += hexBytes(object + 0x25A, 2);
        emit(eax, exact, {}, kIn, mem, host.trace);
        return;
    }
    if (command == "sway" || command == "angle") {
        int mode = 0;
        input >> mode;
        std::uint8_t self[0x220];
        std::uint8_t sway[0x70];
        std::uint8_t record[8];
        std::uint8_t middle[8];
        std::uint8_t inner[0x110];
        float point[2] = {3.f, 4.f};
        fill(self, sizeof(self));
        fill(sway, sizeof(sway));
        fill(inner, sizeof(inner));
        put32(self, 0x208, addr(sway));
        put32(record, 4, addr(middle));
        put32(middle, 4, addr(inner));
        putFloat(sway, 0x18, 0.25f);
        putFloat(sway, 0x34, 0.5f);
        putFloat(sway, 0x38, 0.75f);
        const float sample = mode == 3 ? 3.f : 0.5f;
        if (command == "sway") {
            putFloat(inner, 0x104, 0.f);
            putFloat(inner, 0x108, 0.f);
            putFloat(inner, 0xFC, 0.f);
            putFloat(inner, 0x100, 0.f);
            if (mode == 1 || mode == 3) {
                putFloat(inner, 0x104, 0.5f);
                putFloat(inner, 0x108, 0.25f);
            }
            if (mode == 2 || mode == 3) {
                putFloat(inner, 0xFC, 0.5f);
                putFloat(inner, 0x100, 0.25f);
            }
            const std::uint32_t eax = ra3::gameclient::addSwayOffset(self, record, point, kIn);
            std::vector<Named> exact{{"sway", addr(sway)}, {"rec", addr(record)}};
            std::vector<Named> ah{{"sway", addr(sway)}, {"inner", addr(inner)}, {"rec", addr(record)}};
            std::string mem = hexBytes(sway + 0x34, 8) + hexBytes(point, 8);
            emit(eax, exact, ah, kIn, mem, "-");
            return;
        }
        if (mode == 1) {
            putFloat(inner, 0xA4, 0.f);
            putFloat(inner, 0xA8, 0.5f);
        } else if (mode == 2) {
            putFloat(inner, 0xA4, 0.5f);
            putFloat(inner, 0xA8, 0.f);
        } else {
            putFloat(inner, 0xA4, 0.5f);
            putFloat(inner, 0xA8, 0.25f);
            if (mode == 4) {
                putFloat(sway, 0x18, 4.f);
            } else if (mode == 5) {
                putFloat(sway, 0x18, -4.f);
            } else if (mode == 6) {
                putFloat(sway, 0x18, 1.5707963705062866f);
            }
        }
        const std::uint32_t eax =
            ra3::gameclient::integrateSwayAngle(self, record, sample, kIn);
        std::vector<Named> exact{{"sway", addr(sway)}, {"mid", addr(middle)}};
        std::vector<Named> ah{{"mid", addr(middle)}};
        emit(eax, exact, ah, kIn, hexBytes(sway + 0x18, 4), "-");
        return;
    }
    if (command == "aux") {
        int mode = 0;
        input >> mode;
        std::uint8_t object[0x340];
        fill(object, sizeof(object));
        put32(object, 0x328, 0);
        ScriptHost host;
        std::uint8_t existing[0x24];
        fill(existing, sizeof(existing));
        if (mode == 0) {
            put32(object, 0x328, addr(existing));
        } else if (mode == 2) {
            host.failAlloc = true;
        }
        const std::uint32_t eax = ra3::gameclient::ensureAuxRecord(object, host);
        void* aux = mode == 0 ? existing : host.allocation;
        std::vector<Named> exact;
        if (aux != nullptr) {
            exact.push_back({"aux", addr(aux)});
        }
        const std::string mem = aux == nullptr ? std::string("-") : hexBytes(aux, 0x24);
        emit(eax, exact, {}, kIn, mem, host.trace);
        return;
    }
    if (command == "release" || command == "clear" || command == "visible" || command == "flagd" ||
        command == "attach" || command == "alternate" || command == "supplied" || command == "install") {
        int mode = 0;
        input >> mode;
        ScriptHost host;
        int resultCount = 0;
        input >> resultCount;
        for (int index = 0; index < resultCount; ++index) {
            std::uint32_t value = 0;
            input >> std::hex >> value >> std::dec;
            host.results.push_back(value);
        }
        std::uint8_t object[0x400];
        std::uint32_t registry = 0;
        std::uint32_t tracked[4] = {0, 2, 0, 0};
        std::uint32_t child[2] = {0, 0};
        std::uint32_t gameObject[0xE0]{};
        fill(object, sizeof(object));
        put32(object, 0x138, 0);
        put32(object, 0x178, 0);
        put32(object, 0x17C, 0);
        put32(object, 0x180, 0);
        put32(object, 0x1F4, 0);
        std::uint32_t innerBody = 0;
        host.inner = &innerBody;
        host.names.push_back({"i0", addr(&innerBody)});
        host.names.push_back({"reg", addr(&registry)});
        host.names.push_back({"track", addr(tracked)});
        host.names.push_back({"child", addr(child)});
        host.names.push_back({"obj", addr(gameObject)});
        Mods mods(0);
        Mods clients(0);
        int modCount = 0;
        int clientCount = 0;
        input >> modCount >> clientCount;
        mods = Mods(modCount);
        clients = Mods(clientCount);
        mods.relink();
        clients.relink();
        if (modCount > 0) {
            host.names.push_back({"m0", addr(&mods.body[0])});
            put32(object, 0x180, addr(mods.list));
        }
        if (clientCount > 0) {
            host.names.push_back({"c0", addr(&clients.body[0])});
            put32(object, 0x17C, addr(clients.list));
        }
        object[0x13D] = 0;
        put32(object, 0x20C, 0x22);
        put32(object, 0x210, 0x33);
        if (mode == 1) {
            put32(object, 0x1F4, addr(tracked));
            tracked[1] = 2;
        } else if (mode == 2) {
            put32(object, 0x1F4, addr(tracked));
            tracked[1] = 1;
        }
        std::uint32_t eax = kIn;
        if (command == "release") {
            eax = ra3::gameclient::releaseTracked(object, &registry, host, kIn);
        } else if (command == "clear") {
            int flags = 0;
            input >> flags;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            eax = ra3::gameclient::clearAttachedBit(object, &registry, host, kIn);
        } else if (command == "visible") {
            int flags = 0;
            int argument = 0;
            int withObject = 0;
            input >> flags >> argument >> withObject;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            if (withObject) {
                put32(object, 0x138, addr(gameObject));
                put32(gameObject, 0x33C, addr(child));
            }
            eax = ra3::gameclient::refreshIfVisible(object, static_cast<std::uint32_t>(argument), &registry, host, kIn);
        } else if (command == "flagd") {
            int flags = 0;
            int bit = 0;
            input >> flags >> bit;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            eax = ra3::gameclient::assignFlag13DBit1(object, static_cast<std::uint32_t>(bit), &registry, host, kIn);
        } else if (command == "attach") {
            int flags = 0;
            input >> flags;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            eax = ra3::gameclient::setAttachedAndRefresh(object, &registry, host, kIn);
        } else if (command == "alternate") {
            int flags = 0;
            int field = 0;
            input >> flags >> field;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            put32(object, 0x26C, static_cast<std::uint32_t>(field));
            put32(object, 0x210, 0x44);
            eax = ra3::gameclient::applyAlternate(object, &registry, host, kIn);
        } else if (command == "supplied") {
            int flags = 0;
            int identifier = 0;
            input >> flags >> identifier;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            put32(object, 0x1F4, static_cast<std::uint32_t>(identifier));
            eax = ra3::gameclient::refreshSupplied(
                object, static_cast<std::uint32_t>(identifier), &registry, host, kIn);
        } else if (command == "install") {
            int flags = 0;
            int modeValue = 0;
            int flag = 0;
            input >> flags >> modeValue >> flag;
            object[0x13D] = static_cast<std::uint8_t>(flags);
            eax = ra3::gameclient::installResolved(
                object, static_cast<std::uint32_t>(modeValue), static_cast<std::uint32_t>(flag),
                &registry, host, kIn);
        }
        std::vector<Named> exact{{"in", kIn}};
        std::string mem = hexBytes(object + 0x13D, 1) + hexBytes(object + 0x1F4, 4) +
                          hexBytes(object + 0x20C, 8) + hexBytes(tracked + 1, 4);
        emit(eax, exact, {}, kIn, mem, host.trace);
        return;
    }
    if (command == "tintread" || command == "block" || command == "resolve" || command == "select" ||
        command == "flagc" || command == "gate" || command == "secondary" || command == "updatesway") {
        ScriptHost host;
        int resultCount = 0;
        if (command != "updatesway") {
            input >> resultCount;
        }
        for (int index = 0; index < resultCount; ++index) {
            std::uint32_t value = 0;
            input >> std::hex >> value >> std::dec;
            host.results.push_back(value);
        }
        std::uint8_t object[0x400];
        fill(object, sizeof(object));
        std::uint32_t eax = kIn;
        std::string mem;
        std::vector<Named> exact{{"in", kIn}};
        std::vector<Named> ah;
        if (command == "tintread") {
            int version = 0;
            int shift = 0;
            input >> version >> shift;
            host.stamp = true;
            host.versionByte = version;
            std::uint32_t xfer[4] = {0, static_cast<std::uint32_t>(shift), 0, 0};
            std::uint32_t vtable[40]{};
            xfer[0] = addr(vtable);
            host.names.push_back({"xfer", addr(xfer)});
            eax = ra3::gameclient::readTintEnvelope(object, xfer, host, kIn);
            exact.push_back({"xfer", addr(xfer)});
            mem = hexBytes(object + 4, 0x44);
        } else if (command == "block") {
            int count = 0;
            int byte = 0;
            int rewrite = -1;
            input >> count >> byte >> rewrite;
            std::uint32_t xfer[8]{};
            std::uint32_t counter[2] = {0x1000, static_cast<std::uint32_t>(count)};
            xfer[2] = static_cast<std::uint32_t>(byte);
            std::uint32_t vtable[40]{};
            xfer[0] = addr(vtable);
            host.names.push_back({"xfer", addr(xfer)});
            host.names.push_back({"vt", addr(vtable)});
            if (rewrite >= 0) {
                host.stamp = true;
            }
            eax = ra3::gameclient::readCountedBlock(object, xfer, counter, host);
            exact = {{"xfer", addr(xfer)}};
            mem = hexBytes(counter, 8);
        } else if (command == "resolve" || command == "select") {
            int key = 0;
            int mods = 0;
            int owner = 0;
            input >> key >> mods >> owner;
            put32(object, 4, 0);
            put32(object, 0x180, 0);
            Mods list(mods);
            std::uint32_t innerBody = 0;
            host.inner = &innerBody;
            host.names.push_back({"i0", addr(&innerBody)});
            if (mods > 0) {
                host.names.push_back({"m0", addr(&list.body[0])});
                put32(object, 0x180, addr(list.list));
            }
            std::uint32_t ownerObject[0x80]{};
            if (owner) {
                put32(object, 4, addr(ownerObject));
                host.names.push_back({"owner", addr(ownerObject + 0x57)});
            }
            if (command == "resolve") {
                eax = ra3::gameclient::resolveRecord(object, static_cast<std::uint32_t>(key), host);
            } else {
                eax = ra3::gameclient::selectRecordKey(object, static_cast<std::uint32_t>(key), host);
            }
            exact.push_back({"miss", ra3::gameclient::kMissingRecord});
            mem = "-";
        } else             if (command == "flagc" || command == "gate") {
            int count = 0;
            input >> count;
            put32(object, 0x1C8, 0);
            put32(object, 0x138, 0);
            Mods list(count);
            put32(object, 0x178, addr(list.list));
            if (count > 0) {
                host.names.push_back({"m0", addr(&list.body[0])});
                if (count > 1) {
                    host.names.push_back({"m1", addr(&list.body[1])});
                }
            }
            if (command == "flagc") {
                int flags = 0;
                int bit = 0;
                input >> flags >> bit;
                object[0x13C] = static_cast<std::uint8_t>(flags);
                eax = ra3::gameclient::assignFlag13CBit1(object, static_cast<std::uint32_t>(bit), host, kIn);
            } else {
                int flags = 0;
                int parked = 0;
                int linked = 0;
                int kind = 0;
                input >> flags >> parked >> linked >> kind;
                object[0x13C] = static_cast<std::uint8_t>(flags);
                object[0x13D] = static_cast<std::uint8_t>(linked);
                object[0x13E] = static_cast<std::uint8_t>(parked);
                object[0x258] = static_cast<std::uint8_t>(kind);
                eax = ra3::gameclient::refreshModuleGate(object, nullptr, nullptr, host, kIn);
            }
            mem = hexBytes(object + 0x13C, 4);
        } else if (command == "secondary") {
            int hasObject = 0;
            int bit = 0;
            int diverge = 0;
            input >> hasObject >> bit >> diverge;
            std::uint8_t* secondary = object + 0x60;
            std::uint32_t gameObject[4] = {1, 2, 3, 4};
            Mods list(0);
            put32(object, 0x178, addr(list.list));
            host.names.push_back({"self", addr(object)});
            host.names.push_back({"list", addr(list.list)});
            put32(secondary, 0xD8, 0);
            if (hasObject) {
                put32(secondary, 0xD8, addr(gameObject));
                host.names.push_back({"obj", addr(gameObject)});
            }
            object[0x13D] = 0;
            secondary[0xDE] = static_cast<std::uint8_t>(bit);
            put32(object, 0x38, 0x111);
            put32(object, 0x3C, 0x222);
            put32(object, 0x40, 0x333);
            for (int index = 0; index < 12; ++index) {
                put32(object, 8 + static_cast<std::uint32_t>(index) * 4, 0x40 + static_cast<std::uint32_t>(index));
            }
            put32(secondary, 0x1E4, diverge ? 1u : 0u);
            put32(secondary, 0x1E8, 0);
            object[0x25B] = 0;
            eax = ra3::gameclient::syncSecondary(secondary, host, kIn);
            exact.push_back({"self", addr(object)});
            exact.push_back({"body", addr(object + 0xF4)});
            ah.push_back({"body", addr(object + 0xF4)});
            mem = hexBytes(object + 0xF4, 0x30) + hexBytes(object + 0x124, 12);
        } else if (command == "updatesway") {
            int mode = 0;
            input >> mode;
            std::uint8_t sway[0x70];
            std::uint8_t record[8];
            std::uint8_t middle[8];
            std::uint8_t inner[0xD0];
            std::uint8_t out[0x10];
            std::uint32_t gameObject[0xE0]{};
            fill(sway, sizeof(sway));
            fill(inner, sizeof(inner));
            fill(out, sizeof(out));
            put32(record, 4, addr(middle));
            put32(middle, 4, addr(inner));
            putFloat(inner, 0x88, 1.f);
            putFloat(inner, 0x94, 0.5f);
            putFloat(inner, 0x9C, 0.25f);
            putFloat(inner, 0xC0, 0.5f);
            putFloat(inner, 0xB4, 2.f);
            putFloat(sway, 0xC, 0.5f);
            putFloat(sway, 0x10, 0.25f);
            putFloat(sway, 0x14, 0.125f);
            putFloat(sway, 0x1C, 2.f);
            putFloat(sway, 0x24, -2.f);
            putFloat(object, 0xC4, 0.f);
            putFloat(object, 0xC8, 0.f);
            putFloat(object, 0xD0, 1.f);
            putFloat(object, 0xD4, 0.f);
            putFloat(object, 0xDC, 0.f);
            putFloat(object, 0xE0, 1.f);
            put32(object, 0x390, 10);
            put32(object, 0x138, 0);
            host.names.push_back({"inner", addr(inner)});
            if (mode == 3 || mode == 4) {
                put32(object, 0x208, 0);
            } else {
                put32(object, 0x208, addr(sway));
            }
            if (mode == 1 || mode == 2) {
                put32(object, 0x138, addr(gameObject));
                put32(gameObject, 0x374, 1);
            }
            const std::uint32_t frame = mode == 2 ? 11u : 10u;
            if (mode == 4) {
                host.failAlloc = true;
            }
            eax = ra3::gameclient::updateSwayState(object, record, out, frame, host);
            void* state = mode == 3 || mode == 4 ? host.allocation : sway;
            if (state != nullptr) {
                exact.push_back({"sway", addr(state)});
            }
            exact.push_back({"rec", addr(record)});
            exact.push_back({"inner", addr(inner)});
            mem = hexBytes(out, 0x10);
            if (state != nullptr) {
                mem += hexBytes(state, 0x70);
            }
            mem += hexBytes(object + 0x390, 4);
        }
        emit(eax, exact, ah, kIn, mem, host.trace);
        return;
    }
    std::cout << "error\n";
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }
        if (!line.empty()) {
            run(line);
        }
    }
    return 0;
}
