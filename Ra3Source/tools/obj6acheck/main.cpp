#include <ra3/gamelogic/ObjectCore6A.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ra3::gamelogic;

std::uint32_t floatToBits(float v) noexcept
{
    std::uint32_t b = 0u;
    std::memcpy(&b, &v, sizeof(b));
    return b;
}

float bitsToFloat(std::uint32_t b) noexcept
{
    float v = 0.0f;
    std::memcpy(&v, &b, sizeof(v));
    return v;
}

bool selfTest() noexcept
{
    std::uint32_t words[15]{};
    words[1] = 1u << 5u;
    if (!objectTestStatusBit(words, 37u) || objectTestStatusBit(words, 36u)) {
        return false;
    }
    const Vec3f6A a{1.0f, 2.0f, 3.0f};
    const Vec3f6A b{4.0f, 6.0f, 3.0f};
    if (floatToBits(objectGetDistanceSquaredToObject(a, b)) != floatToBits(25.0f)) {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--self-test") {
            return selfTest() ? 0 : 1;
        }
    }
    if (!selfTest()) {
        std::cerr << "obj6acheck selfTest failed\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "quit") {
            break;
        }

        if (cmd == "f_6a15c0") {
            std::size_t n = 0;
            std::int32_t ktag = 0;
            std::uint32_t kx = 0, ky = 0, kz = 0;
            ss >> n >> ktag >> kx >> ky >> kz;
            std::vector<WeaponSetEntryKey> entries(n);
            for (std::size_t i = 0; i < n; ++i) {
                std::uint32_t ex = 0, ey = 0, ez = 0;
                ss >> entries[i].kindOrTag >> ex >> ey >> ez;
                entries[i].x = bitsToFloat(ex);
                entries[i].y = bitsToFloat(ey);
                entries[i].z = bitsToFloat(ez);
            }
            const WeaponSetEntryKey key{ktag, bitsToFloat(kx), bitsToFloat(ky), bitsToFloat(kz)};
            const WeaponSetEntryKey* begin = entries.empty() ? nullptr : entries.data();
            const WeaponSetEntryKey* end = entries.empty() ? nullptr : entries.data() + entries.size();
            const bool res = weaponSetHasEntryKey(begin, end, key);
            std::cout << (res ? 1 : 0) << "\n";
        } else if (cmd == "f_6a1640") {
            std::size_t n = 0;
            int hasSub64 = 0;
            ss >> n >> hasSub64;
            std::vector<std::uint32_t> flags(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> flags[i];
            }
            std::uint32_t mask34 = 0xDEADBEEFu;
            int elemCalls = 0, sub64Calls = 0;
            const int ret = weaponSetRebuildMaskAndNotify(
                mask34, flags.data(), n, hasSub64 != 0, elemCalls, sub64Calls);
            std::cout << ret << " " << mask34 << " " << elemCalls << " " << sub64Calls << "\n";
        } else if (cmd == "f_6a16a0") {
            std::size_t n = 0;
            ss >> n;
            std::cout << weaponSetNotifyAllSlots1C(n) << "\n";
        } else if (cmd == "f_6a16d0") {
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> n >> tag;
            std::vector<std::int32_t> tags(n);
            std::vector<std::uint8_t> res30(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned r = 0;
                ss >> tags[i] >> r;
                res30[i] = static_cast<std::uint8_t>(r);
            }
            int idx = -1;
            const bool r = weaponSetInvokeSlot30ByTag(tags.data(), res30.data(), n, tag, idx);
            std::cout << (r ? 1 : 0) << " " << idx << "\n";
        } else if (cmd == "f_6a1720") {
            std::size_t n = 0;
            std::int32_t arg0 = 0, tag = 0;
            ss >> n >> arg0 >> tag;
            std::vector<std::int32_t> tags(n);
            std::vector<std::uint32_t> w20(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> tags[i] >> w20[i];
            }
            int calls = 0;
            const std::uint32_t w = weaponSetFindWeapon20ByTag(tags.data(), w20.data(), n, arg0, tag, calls);
            std::cout << w << " " << calls << "\n";
        } else if (cmd == "f_6a1770") {
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> n >> tag;
            std::vector<std::int32_t> tags(n);
            std::vector<std::uint32_t> w24(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> tags[i] >> w24[i];
            }
            int calls = 0;
            const std::uint32_t w = weaponSetFindWeapon24ByTag(tags.data(), w24.data(), n, tag, calls);
            std::cout << w << " " << calls << "\n";
        } else if (cmd == "f_6a17c0") {
            std::size_t n = 0;
            ss >> n;
            std::vector<std::uint32_t> w24(n);
            std::vector<std::int32_t> f5c(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> w24[i] >> f5c[i];
            }
            std::cout << weaponSetFindActiveWeapon24(w24.data(), f5c.data(), n) << "\n";
        } else if (cmd == "f_6a17f0") {
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> n >> tag;
            std::vector<std::uint32_t> addrs(n);
            std::vector<std::int32_t> tags(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> addrs[i] >> tags[i];
            }
            std::cout << weaponSetFindSlotByPositiveTag(addrs.data(), tags.data(), n, tag) << "\n";
        } else if (cmd == "f_6a1820") {
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> n >> tag;
            std::vector<std::uint32_t> addrs(n);
            std::vector<std::int32_t> tags(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> addrs[i] >> tags[i];
            }
            std::cout << weaponSetFindSlotByTag(addrs.data(), tags.data(), n, tag) << "\n";
        } else if (cmd == "f_6a18a0") {
            std::size_t n = 0;
            ss >> n;
            std::vector<std::uint32_t> w10(n), sub90(n);
            std::vector<std::int32_t> res(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> w10[i] >> sub90[i] >> res[i];
            }
            std::cout << weaponSetQueryFirstReloadState(w10.data(), sub90.data(), res.data(), n) << "\n";
        } else if (cmd == "f_6a18e0") {
            std::size_t n = 0;
            std::uint32_t targetId = 0;
            ss >> n >> targetId;
            std::vector<std::uint32_t> w10(n), sub90(n), f24(n), f28(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> w10[i] >> sub90[i] >> f24[i] >> f28[i];
            }
            const bool r = weaponSetHasTargetObjectId(
                w10.data(), sub90.data(), f24.data(), f28.data(), n, targetId);
            std::cout << (r ? 1 : 0) << "\n";
        } else if (cmd == "f_6a1930") {
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> n >> tag;
            std::vector<std::int32_t> tags(n);
            std::vector<std::uint8_t> res34(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned r = 0;
                ss >> tags[i] >> r;
                res34[i] = static_cast<std::uint8_t>(r);
            }
            int idx = -1;
            const bool r = weaponSetDispatchSlot34ByTag(tags.data(), res34.data(), n, tag, idx);
            std::cout << (r ? 1 : 0) << " " << idx << "\n";
        } else if (cmd == "f_6a19e0") {
            std::size_t n = 0;
            ss >> n;
            std::vector<std::uint8_t> r2c(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned r = 0;
                ss >> r;
                r2c[i] = static_cast<std::uint8_t>(r);
            }
            std::cout << (weaponSetHasAnySlotReady2C(r2c.data(), n) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a1a10") {
            std::size_t n = 0;
            std::uint32_t argAddr = 0, argObjId = 0;
            ss >> n >> argAddr >> argObjId;
            std::vector<std::uint32_t> sub38(n), f24(n), f28(n), f2c(n), f30(n), f38(n);
            std::vector<std::int32_t> f70(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> sub38[i] >> f24[i] >> f28[i] >> f2c[i] >> f30[i] >> f38[i] >> f70[i];
            }
            weaponSetUpdateAllSlotSub38Targets(
                sub38.data(), n, argAddr, argObjId,
                f24.data(), f28.data(), f2c.data(), f30.data(), f38.data(), f70.data());
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0) std::cout << " ";
                std::cout << f24[i] << " " << f28[i] << " " << f2c[i] << " "
                          << f30[i] << " " << f38[i] << " " << f70[i];
            }
            std::cout << "\n";
        } else if (cmd == "f_6a1a50") {
            std::size_t n = 0;
            ss >> n;
            std::vector<std::uint32_t> sub38(n), f30(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> sub38[i] >> f30[i];
            }
            weaponSetClearAllSlotSub38Field30(sub38.data(), n, f30.data());
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0) std::cout << " ";
                std::cout << f30[i];
            }
            std::cout << "\n";
        } else if (cmd == "f_6a3b80") {
            int hasVec = 0;
            std::size_t n = 0;
            std::uint32_t objId = 0, initIdx = 0;
            ss >> hasVec >> n >> objId >> initIdx;
            std::vector<std::uint32_t> ids(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> ids[i];
            }
            std::uint32_t outIdx = initIdx;
            const bool r = experienceVectorFindObjectId(hasVec != 0, ids.data(), n, objId, outIdx);
            std::cout << (r ? 1 : 0) << " " << outIdx << "\n";
        } else if (cmd == "f_6a3bd0") {
            int hasVec = 0;
            std::size_t n = 0;
            ss >> hasVec >> n;
            std::vector<std::uint32_t> ids(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> ids[i];
            }
            std::cout << experienceVectorFirstId(hasVec != 0, ids.data(), n) << "\n";
        } else if (cmd == "f_6a3bf0") {
            std::uint32_t defBits = 0;
            std::size_t n = 0;
            std::int32_t key = 0;
            ss >> defBits >> n >> key;
            std::vector<ExperienceScaleEntry> entries(n);
            for (std::size_t i = 0; i < n; ++i) {
                std::uint32_t sb = 0;
                ss >> entries[i].key >> sb;
                entries[i].scale = bitsToFloat(sb);
            }
            const float res = experienceTemplateLookupScale(bitsToFloat(defBits), entries.data(), n, key);
            std::cout << floatToBits(res) << "\n";
        } else if (cmd == "f_6a3c30") {
            int hasTracker = 0;
            std::uint32_t defBits = 0;
            std::size_t n = 0;
            std::int32_t key = 0;
            ss >> hasTracker >> defBits >> n >> key;
            std::vector<ExperienceScaleEntry> entries(n);
            for (std::size_t i = 0; i < n; ++i) {
                std::uint32_t sb = 0;
                ss >> entries[i].key >> sb;
                entries[i].scale = bitsToFloat(sb);
            }
            const float res = experienceTrackerLookupScaleOrOne(
                hasTracker != 0, bitsToFloat(defBits), entries.data(), n, key);
            std::cout << floatToBits(res) << "\n";
        } else if (cmd == "f_6a4130") {
            std::uint32_t flags14 = 0, contA = 0, contB = 0;
            int hasA = 0, hasB = 0;
            ss >> flags14 >> hasA >> hasB >> contA >> contB;
            std::uint32_t src244 = 0, dstCont = 0;
            const bool r = experienceShouldSyncContainedStateBit10(
                flags14, hasA != 0, hasB != 0, contA, contB, src244, dstCont);
            std::cout << (r ? 1 : 0) << " " << src244 << " " << dstCont << "\n";
        } else if (cmd == "f_6a4180") {
            std::uint32_t flags14 = 0, visABits = 0, visBBits = 0;
            int hasA = 0, hasB = 0, hasContA = 0, hasContB = 0;
            ss >> flags14 >> hasA >> hasB >> hasContA >> hasContB >> visABits >> visBBits;
            float visA = bitsToFloat(visABits);
            experienceSyncContainedVisionRangeBit11(
                flags14, hasA != 0, hasB != 0, hasContA != 0, hasContB != 0,
                visA, bitsToFloat(visBBits));
            std::cout << floatToBits(visA) << "\n";
        } else if (cmd == "f_6a0bf0") {
            unsigned flag45 = 0;
            std::uint32_t start40 = 0, curFrame = 0, schedFrame = 0;
            ss >> flag45 >> start40 >> curFrame;
            std::uint8_t f45 = static_cast<std::uint8_t>(flag45);
            structureUnpackStart(f45, start40, schedFrame, curFrame);
            std::cout << static_cast<unsigned>(f45) << " " << start40 << " " << schedFrame << "\n";
        } else if (cmd == "f_6a0c80") {
            unsigned flag45 = 0;
            std::uint32_t start40 = 0, acc3C = 0, dur38 = 0, curFrame = 0, schedFrame = 0;
            ss >> flag45 >> start40 >> acc3C >> dur38 >> curFrame;
            std::uint8_t f45 = static_cast<std::uint8_t>(flag45);
            structureUnpackPauseAndAccumulate(f45, start40, acc3C, dur38, curFrame, schedFrame);
            std::cout << static_cast<unsigned>(f45) << " " << start40 << " " << acc3C << " " << schedFrame << "\n";
        } else if (cmd == "f_6a4cf0") {
            std::size_t n = 0;
            std::uint32_t mask = 0;
            ss >> n >> mask;
            std::vector<std::uint32_t> addrs(n), masks(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> addrs[i] >> masks[i];
            }
            std::cout << objectFindSubObjectByMask(addrs.data(), masks.data(), n, mask) << "\n";
        } else if (cmd == "f_6a4e70") {
            std::uint32_t s84 = 0;
            std::int32_t ai190 = 0;
            ss >> s84 >> ai190;
            std::cout << (objectIsMobileOrHasWaypoints(s84, ai190) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a4ed0") {
            unsigned crusher = 0;
            std::uint32_t d4 = 0, dc = 0, ai208 = 0;
            int hasAi = 0;
            ss >> crusher >> d4 >> dc >> hasAi >> ai208;
            std::cout << (objectCanCrushOrSquishTarget(
                static_cast<std::uint8_t>(crusher), d4, dc, hasAi != 0, ai208) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a4f20") {
            std::uint32_t s80 = 0, s98 = 0, wf1c = 0;
            ss >> s80 >> s98 >> wf1c;
            std::cout << (objectIsIgnoredByWeaponOrKind(s80, s98, wf1c) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a4f60") {
            int hasWs = 0;
            std::size_t n = 0;
            ss >> hasWs >> n;
            std::vector<std::uint8_t> r2c(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned r = 0;
                ss >> r;
                r2c[i] = static_cast<std::uint8_t>(r);
            }
            std::cout << (objectHasAnyWeaponReady(hasWs != 0, r2c.data(), n) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a4fb0") {
            std::uint32_t selfAddr = 0, headAddr = 0, prev4EC = 0, next4F0 = 0, oldHeadPrev = 0;
            int hasAi = 0;
            std::uint32_t aiWake14 = 0, curFrame = 0;
            unsigned ai43B = 0;
            ss >> selfAddr >> headAddr >> prev4EC >> next4F0 >> oldHeadPrev
               >> hasAi >> aiWake14 >> ai43B >> curFrame;
            bool sched = false;
            std::uint32_t wakeFrame = 0;
            objectMarkStatusDirtyAndWakeAI(
                selfAddr, headAddr, prev4EC, next4F0, oldHeadPrev,
                hasAi != 0, aiWake14, static_cast<std::uint8_t>(ai43B),
                curFrame, sched, wakeFrame);
            std::cout << headAddr << " " << prev4EC << " " << next4F0 << " "
                      << oldHeadPrev << " " << (sched ? 1 : 0) << " " << wakeFrame << "\n";
        } else if (cmd == "f_6a5030") {
            std::uint32_t words[15]{};
            for (int i = 0; i < 15; ++i) ss >> words[i];
            std::uint32_t bit = 0;
            ss >> bit;
            std::cout << (objectTestStatusBit(words, bit) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5060") {
            std::uint32_t words[15]{}, newWords[15]{};
            for (int i = 0; i < 15; ++i) ss >> words[i];
            for (int i = 0; i < 15; ++i) ss >> newWords[i];
            int hasTeam = 0;
            std::int32_t teamCnt = 0;
            ss >> hasTeam >> teamCnt;
            bool dirty = false;
            objectUpdateStatusBits5060(words, newWords, hasTeam != 0, teamCnt, dirty);
            std::cout << (dirty ? 1 : 0) << " " << teamCnt << " " << words[11] << "\n";
        } else if (cmd == "f_6a50f0") {
            std::uint32_t words[15]{}, srcWords[15]{};
            for (int i = 0; i < 15; ++i) ss >> words[i];
            for (int i = 0; i < 15; ++i) ss >> srcWords[i];
            unsigned forceNotify = 0;
            int hasTeam = 0, hasCont = 0;
            std::int32_t teamCnt = 0;
            ss >> forceNotify >> hasTeam >> teamCnt >> hasCont;
            bool notified = false;
            objectAssignStatusMask50F0(
                words, srcWords, static_cast<std::uint8_t>(forceNotify),
                hasTeam != 0, teamCnt, hasCont != 0, notified);
            std::cout << (notified ? 1 : 0) << " " << teamCnt << " " << words[11] << "\n";
        } else if (cmd == "f_6a51c0") {
            int hasWs = 0;
            std::size_t n = 0;
            std::int32_t tag = 0;
            ss >> hasWs >> n >> tag;
            std::vector<std::int32_t> tags(n);
            std::vector<std::uint32_t> w24(n), w20(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> tags[i] >> w24[i] >> w20[i];
            }
            std::cout << objectFindWeaponBySlotOrTag(
                hasWs != 0, tags.data(), w24.data(), w20.data(), n, tag) << "\n";
        } else if (cmd == "f_6a51f0") {
            int hasWs = 0;
            std::size_t n = 0;
            ss >> hasWs >> n;
            std::vector<std::uint32_t> w24(n);
            std::vector<std::int32_t> f5c(n);
            for (std::size_t i = 0; i < n; ++i) {
                ss >> w24[i] >> f5c[i];
            }
            std::cout << objectGetActiveWeapon(hasWs != 0, w24.data(), f5c.data(), n) << "\n";
        } else if (cmd == "f_6a5240") {
            std::uint32_t s88 = 0;
            int hasT = 0, v4 = 0;
            ss >> s88 >> hasT >> v4;
            std::cout << (objectCanInteractWithTarget(s88, hasT != 0, v4 != 0) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5270") {
            std::uint32_t s90 = 0;
            int hasAi = 0, hasLoco = 0, v38 = 0;
            ss >> s90 >> hasAi >> hasLoco >> v38;
            std::cout << objectIsAirborneOrLocomotorActive(s90, hasAi != 0, hasLoco != 0, v38 != 0) << "\n";
        } else if (cmd == "f_6a52a0") {
            int hasPhys = 0;
            std::int32_t st38 = 0;
            std::uint32_t s98 = 0;
            ss >> hasPhys >> st38 >> s98;
            std::cout << (objectIsPhysicsFallingWithKindBit(hasPhys != 0, st38, s98) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a52c0") {
            std::uint32_t s84 = 0, s80 = 0, s90 = 0, sb0 = 0, tdc = 0;
            int hasPhys = 0;
            std::int32_t st38 = 0;
            ss >> s84 >> s80 >> s90 >> sb0 >> tdc >> hasPhys >> st38;
            std::cout << (objectIsCrushableOrPhysicsActive(
                s84, s80, s90, sb0, tdc, hasPhys != 0, st38) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5320") {
            std::uint32_t te0 = 0, idDC = 0;
            std::size_t n = 0;
            ss >> te0 >> idDC >> n;
            std::vector<std::uint32_t> list(n);
            for (std::size_t i = 0; i < n; ++i) ss >> list[i];
            const std::uint32_t* b = list.empty() ? nullptr : list.data();
            const std::uint32_t* e = list.empty() ? nullptr : list.data() + list.size();
            std::cout << (objectHasObjectInList4B8(te0, idDC, b, e) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5450") {
            std::uint32_t tc8 = 0, prodId = 0, fE4 = 0;
            int hasTeam = 0, hasProd = 0;
            unsigned f4d7 = 0;
            ss >> tc8 >> hasTeam >> f4d7 >> hasProd >> prodId >> fE4;
            bool unreg = false, reg = false;
            objectSetProducerId(
                tc8, hasTeam != 0, static_cast<std::uint8_t>(f4d7),
                hasProd != 0, prodId, fE4, unreg, reg);
            std::cout << fE4 << " " << (unreg ? 1 : 0) << " " << (reg ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5530") {
            int hasDraw = 0, pFlag = 0;
            std::uint32_t tc4 = 0, pIdx = 0;
            ss >> hasDraw >> pFlag >> tc4 >> pIdx;
            std::uint32_t tbl[20]{};
            for (int i = 0; i < 20; ++i) ss >> tbl[i];
            std::cout << objectGetDrawableVisibility5530(hasDraw != 0, pFlag != 0, tc4, pIdx, tbl) << "\n";
        } else if (cmd == "f_6a5590") {
            int hasDraw = 0, pFlag = 0;
            std::uint32_t tc4 = 0, pIdx = 0;
            ss >> hasDraw >> pFlag >> tc4 >> pIdx;
            std::uint32_t tbl[20]{};
            for (int i = 0; i < 20; ++i) ss >> tbl[i];
            std::cout << objectGetDrawableVisibility5590(hasDraw != 0, pFlag != 0, tc4, pIdx, tbl) << "\n";
        } else if (cmd == "f_6a56f0") {
            int hasCont = 0;
            std::uint32_t mask26c = 0;
            std::int32_t dtype = 0;
            unsigned b14c = 0;
            ss >> hasCont >> mask26c >> dtype >> b14c;
            std::uint8_t byte14C = static_cast<std::uint8_t>(b14c);
            objectPropagateDisabledTypeToContainer(hasCont != 0, mask26c, dtype, byte14C);
            std::cout << static_cast<unsigned>(byte14C) << "\n";
        } else if (cmd == "f_6a5750") {
            std::uint32_t dtype = 0, mask26c = 0, curFrame = 0;
            ss >> dtype >> mask26c >> curFrame;
            std::uint32_t frames[12]{};
            for (int i = 0; i < 12; ++i) ss >> frames[i];
            std::cout << objectGetDisabledUntilFrame(dtype, mask26c, frames, curFrame) << "\n";
        } else if (cmd == "f_6a5810" || cmd == "f_6a5890") {
            int hasTbl = 0, cnt = 0;
            std::uint32_t remId = 0;
            ss >> hasTbl >> cnt;
            if (cmd == "f_6a5890") ss >> remId;
            std::vector<ObjectSlotTableEntry450> entries(cnt > 0 ? cnt : 1);
            for (int i = 0; i < cnt; ++i) {
                unsigned f4 = 0, f5 = 0, f6 = 0;
                ss >> entries[i].objectId >> f4 >> f5 >> f6;
                entries[i].flag4 = static_cast<std::uint8_t>(f4);
                entries[i].recentFlag5 = static_cast<std::uint8_t>(f5);
                entries[i].activeFlag6 = static_cast<std::uint8_t>(f6);
                entries[i].pad7 = 0;
            }
            ObjectSlotTable450 tbl{};
            tbl.entries = entries.data();
            tbl.count = static_cast<std::int8_t>(cnt);
            if (cmd == "f_6a5810") {
                objectCompactSlotTable450(hasTbl ? &tbl : nullptr);
            } else {
                objectRemoveFromSlotTable450(hasTbl ? &tbl : nullptr, remId);
            }
            std::cout << static_cast<int>(tbl.count);
            for (int i = 0; i < cnt; ++i) {
                std::cout << " " << entries[i].objectId << " "
                          << static_cast<unsigned>(entries[i].flag4) << " "
                          << static_cast<unsigned>(entries[i].recentFlag5) << " "
                          << static_cast<unsigned>(entries[i].activeFlag6);
            }
            std::cout << "\n";
        } else if (cmd == "f_6a5a60") {
            std::uint32_t s80 = 0, s88 = 0;
            int hasAi = 0, aiRes = 0;
            unsigned cmd7c = 0;
            ss >> s80 >> s88 >> hasAi >> aiRes >> cmd7c;
            std::cout << static_cast<unsigned>(objectCanExecuteCommand(
                s80, s88, hasAi != 0, aiRes != 0, static_cast<std::uint8_t>(cmd7c))) << "\n";
        } else if (cmd == "f_6a5b10") {
            int hasTbl = 0, cnt = 0;
            std::uint32_t lastFrame = 0, curFrame = 0, targetId = 0;
            ss >> hasTbl >> lastFrame >> curFrame >> targetId >> cnt;
            std::vector<ObjectSlotTableEntry450> entries(cnt > 0 ? cnt : 1);
            for (int i = 0; i < cnt; ++i) {
                unsigned f5 = 0;
                ss >> entries[i].objectId >> f5;
                entries[i].recentFlag5 = static_cast<std::uint8_t>(f5);
            }
            ObjectSlotTable450 tbl{};
            tbl.entries = entries.data();
            tbl.lastUpdateFrame = lastFrame;
            tbl.count = static_cast<std::int8_t>(cnt);
            std::cout << (objectHasRecentFlag5InSlotTable450(
                hasTbl ? &tbl : nullptr, curFrame, targetId) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5b70") {
            int hasTbl = 0, cnt = 0;
            std::uint32_t targetId = 0;
            ss >> hasTbl >> targetId >> cnt;
            std::vector<ObjectSlotTableEntry450> entries(cnt > 0 ? cnt : 1);
            for (int i = 0; i < cnt; ++i) {
                unsigned f6 = 0;
                ss >> entries[i].objectId >> f6;
                entries[i].activeFlag6 = static_cast<std::uint8_t>(f6);
            }
            ObjectSlotTable450 tbl{};
            tbl.entries = entries.data();
            tbl.count = static_cast<std::int8_t>(cnt);
            std::cout << (objectHasActiveEntryInSlotTable450(
                hasTbl ? &tbl : nullptr, targetId) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a5bc0") {
            std::uint32_t px = 0, py = 0, pz = 0, th = 0;
            ss >> px >> py >> pz >> th;
            const float res = objectGetHeightAboveTerrain(
                bitsToFloat(px), bitsToFloat(py), bitsToFloat(pz), bitsToFloat(th));
            std::cout << floatToBits(res) << "\n";
        } else if (cmd == "f_6a5f50") {
            std::uint32_t tc4 = 0, tc8 = 0, s80 = 0;
            unsigned b24b = 0, f471 = 0, s4d2 = 0;
            ss >> tc4 >> tc8 >> b24b >> s80 >> f471 >> s4d2;
            std::cout << (objectIsSelectable(
                tc4, tc8, static_cast<std::uint8_t>(b24b), s80,
                static_cast<std::uint8_t>(f471), static_cast<std::uint8_t>(s4d2)) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a60a0") {
            std::uint32_t selfAddr = 0, otherAddr = 0, self328 = 0, selfSlot32C = 0, other328 = 0, otherSlot32C = 0;
            std::size_t nSelf = 0, nOther = 0;
            ss >> selfAddr >> otherAddr >> self328 >> selfSlot32C >> other328 >> otherSlot32C >> nSelf >> nOther;
            std::vector<std::uint32_t> sMods(nSelf + 1, 0u), sTags(nSelf, 0u);
            std::vector<std::uint32_t> oMods(nOther + 1, 0u), oTags(nOther, 0u);
            for (std::size_t i = 0; i < nSelf; ++i) ss >> sMods[i] >> sTags[i];
            for (std::size_t i = 0; i < nOther; ++i) ss >> oMods[i] >> oTags[i];
            std::uint32_t newSelfOwner = 0, newOtherOwner = 0;
            objectSwapPrimaryBodyModules(
                selfAddr, otherAddr, sMods.data(), sTags.data(), self328, selfSlot32C,
                oMods.data(), oTags.data(), other328, otherSlot32C, newSelfOwner, newOtherOwner);
            std::cout << self328 << " " << selfSlot32C << " " << other328 << " " << otherSlot32C
                      << " " << newSelfOwner << " " << newOtherOwner;
            for (std::size_t i = 0; i < nSelf; ++i) std::cout << " " << sMods[i];
            for (std::size_t i = 0; i < nOther; ++i) std::cout << " " << oMods[i];
            std::cout << "\n";
        } else if (cmd == "f_6a6210") {
            unsigned f471 = 0;
            int hasAi = 0, hasSub47C = 0, hasWs = 0;
            std::int32_t aiRes = 0;
            std::size_t n = 0;
            ss >> f471 >> hasAi >> hasSub47C >> aiRes >> hasWs >> n;
            std::vector<std::uint32_t> w10(n), sub90(n);
            std::vector<std::int32_t> res(n);
            for (std::size_t i = 0; i < n; ++i) ss >> w10[i] >> sub90[i] >> res[i];
            std::cout << objectGetReloadTimeOrWeaponState(
                static_cast<std::uint8_t>(f471), hasAi != 0, hasSub47C != 0,
                aiRes, hasWs != 0, w10.data(), sub90.data(), res.data(), n) << "\n";
        } else if (cmd == "f_6a6250") {
            int hasAi = 0, hasSub47C = 0, hasWs = 0;
            std::uint32_t aiF30 = 0;
            std::size_t n = 0;
            ss >> hasAi >> hasSub47C >> aiF30 >> hasWs >> n;
            std::vector<std::uint32_t> sub38(n), f30(n);
            for (std::size_t i = 0; i < n; ++i) ss >> sub38[i] >> f30[i];
            objectClearWeaponTargetField30(
                hasAi != 0, hasSub47C != 0, aiF30, hasWs != 0, sub38.data(), n, f30.data());
            std::cout << aiF30;
            for (std::size_t i = 0; i < n; ++i) std::cout << " " << f30[i];
            std::cout << "\n";
        } else if (cmd == "f_6a6280") {
            std::uint32_t td4 = 0, sb0 = 0;
            ss >> td4 >> sb0;
            std::cout << objectIsUnderConstructionOrUpgrading(td4, sb0) << "\n";
        } else if (cmd == "f_6a6510") {
            std::uint32_t v25c = 0, v258 = 0, modScale = 0, pz = 0, gz = 0, hf = 0, maxB = 0, minB = 0;
            int hasMod = 0;
            ss >> v25c >> v258 >> hasMod >> modScale >> pz >> gz >> hf >> maxB >> minB;
            const float r = objectComputeVisionRange(
                bitsToFloat(v25c), bitsToFloat(v258), hasMod != 0, bitsToFloat(modScale),
                bitsToFloat(pz), bitsToFloat(gz), bitsToFloat(hf), bitsToFloat(maxB), bitsToFloat(minB));
            std::cout << floatToBits(r) << "\n";
        } else if (cmd == "f_6a65e0") {
            std::uint32_t v260 = 0, modScale = 0, pz = 0, gz = 0, hf = 0, maxB = 0, minB = 0, tdc = 0, s94 = 0;
            int hasMod = 0;
            ss >> v260 >> hasMod >> modScale >> pz >> gz >> hf >> maxB >> minB >> tdc >> s94;
            const float r = objectComputeShroudClearingRange(
                bitsToFloat(v260), hasMod != 0, bitsToFloat(modScale),
                bitsToFloat(pz), bitsToFloat(gz), bitsToFloat(hf), bitsToFloat(maxB), bitsToFloat(minB),
                tdc, s94);
            std::cout << floatToBits(r) << "\n";
        } else if (cmd == "f_6a6770") {
            std::uint32_t f464 = 0, fb94 = 0, outV = 0;
            ss >> f464 >> fb94;
            objectGetFormationOrTemplateValue464(f464, fb94, outV);
            std::cout << outV << "\n";
        } else if (cmd == "f_6a68e0") {
            int has3ac = 0, hasP = 0, p1f8 = 0, p1fc = 0;
            std::int32_t f20 = 0;
            std::uint32_t td4 = 0;
            unsigned f471 = 0, tc7 = 0;
            ss >> has3ac >> f20 >> td4 >> f471 >> tc7 >> hasP >> p1f8 >> p1fc;
            std::cout << objectGetRadarOrVeterancyPriority(
                has3ac != 0, f20, td4, static_cast<std::uint8_t>(f471),
                static_cast<std::uint8_t>(tc7), hasP != 0, p1f8 != 0, p1fc != 0) << "\n";
        } else if (cmd == "f_6a6b10") {
            std::uint32_t c0 = 0, c4 = 0, d0 = 0, d4 = 0, d8 = 0;
            ss >> c0 >> c4 >> d0 >> d4 >> d8;
            std::cout << objectComputePackedStateFlags6B10(c0, c4, d0, d4, d8) << "\n";
        } else if (cmd == "f_6a6c10") {
            std::uint32_t ts80 = 0;
            std::size_t n = 0;
            ss >> ts80 >> n;
            std::vector<std::uint32_t> f0c(n);
            for (std::size_t i = 0; i < n; ++i) ss >> f0c[i];
            std::cout << objectDispatchCollisionModules2A0(ts80, f0c.data(), n) << "\n";
        } else if (cmd == "f_6a6c80") {
            unsigned alt = 0;
            ss >> alt;
            float m1[12]{}, m2[12]{};
            for (int i = 0; i < 12; ++i) { std::uint32_t b = 0; ss >> b; m1[i] = bitsToFloat(b); }
            for (int i = 0; i < 12; ++i) { std::uint32_t b = 0; ss >> b; m2[i] = bitsToFloat(b); }
            Vec3f6A out{};
            objectGetTransformTranslation(static_cast<std::uint8_t>(alt), m1, m2, out);
            std::cout << floatToBits(out.x) << " " << floatToBits(out.y) << " " << floatToBits(out.z) << "\n";
        } else if (cmd == "f_6a71a0") {
            unsigned cb = 0;
            std::uint32_t c8 = 0;
            ss >> cb >> c8;
            std::cout << (objectHasTemplateBitCBAndStatusC8Bit9(static_cast<std::uint8_t>(cb), c8) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a72a0") {
            unsigned raw = 0;
            std::uint32_t rx = 0, ry = 0, rz = 0, ox = 0, oy = 0, oz = 0;
            ss >> raw >> rx >> ry >> rz >> ox >> oy >> oz;
            float mat[12]{};
            for (int i = 0; i < 12; ++i) { std::uint32_t b = 0; ss >> b; mat[i] = bitsToFloat(b); }
            Vec3f6A out{};
            objectComputeTransformedOffsetPosition(
                static_cast<std::uint8_t>(raw),
                Vec3f6A{bitsToFloat(rx), bitsToFloat(ry), bitsToFloat(rz)},
                mat,
                Vec3f6A{bitsToFloat(ox), bitsToFloat(oy), bitsToFloat(oz)},
                out);
            std::cout << floatToBits(out.x) << " " << floatToBits(out.y) << " " << floatToBits(out.z) << "\n";
        } else if (cmd == "f_6a73a0") {
            std::size_t n = 0;
            std::uint32_t tid = 0;
            ss >> n >> tid;
            std::vector<std::uint32_t> addrs(n + 1, 0u), ids(n, 0u);
            for (std::size_t i = 0; i < n; ++i) ss >> addrs[i] >> ids[i];
            std::cout << objectFindModuleById310(addrs.data(), ids.data(), n + 1, tid) << "\n";
        } else if (cmd == "f_6a7510") {
            std::uint32_t c8 = 0;
            ss >> c8;
            std::cout << objectGetStatusTriState7510(c8) << "\n";
        } else if (cmd == "f_6a7540") {
            std::uint32_t c8c = 0;
            ss >> c8c;
            std::cout << (objectHasStatus8CBit3Or4(c8c) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a7560") {
            std::uint32_t c8 = 0;
            ss >> c8;
            std::cout << (objectHasValidStatusTriState7560(c8) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a75a0") {
            std::uint32_t te0 = 0, tdc = 0, sc0 = 0, s90 = 0, s80 = 0;
            ss >> te0 >> tdc >> sc0 >> s90 >> s80;
            std::cout << objectIsCrushedOrImmobilized75A0(te0, tdc, sc0, s90, s80) << "\n";
        } else if (cmd == "f_6a7620") {
            std::size_t n = 0;
            std::uint32_t p328 = 0;
            ss >> n >> p328;
            std::vector<std::uint32_t> mods(n);
            std::vector<std::uint8_t> m94(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned m = 0;
                ss >> mods[i] >> m;
                m94[i] = static_cast<std::uint8_t>(m);
            }
            std::cout << objectFindBodyOrWeaponModule7620(mods.data(), m94.data(), n, p328) << "\n";
        } else if (cmd == "f_6a7780") {
            std::uint32_t head = 0;
            std::size_t n = 0;
            ss >> head >> n;
            std::vector<std::uint32_t> addrs(n), prev4ec(n), next4f0(n);
            std::vector<std::uint8_t> hasCont(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned hc = 0;
                ss >> addrs[i] >> prev4ec[i] >> next4f0[i] >> hc;
                hasCont[i] = static_cast<std::uint8_t>(hc);
            }
            int notifs = 0;
            const int proc = objectFlushDirtyStatusList7780(
                head, addrs.data(), prev4ec.data(), next4f0.data(), hasCont.data(), n, notifs);
            std::cout << head << " " << proc << " " << notifs;
            for (std::size_t i = 0; i < n; ++i) {
                std::cout << " " << prev4ec[i] << " " << next4f0[i];
            }
            std::cout << "\n";
        } else if (cmd == "f_6a78d0") {
            std::uint32_t f54 = 0, f3a0 = 0, p2c = 0;
            ss >> f54 >> f3a0 >> p2c;
            std::cout << (objectIsOwnedByLocalPlayerShifted78(f54, f3a0, p2c) ? 1 : 0) << "\n";
        } else if (cmd == "f_6a7900") {
            std::size_t n = 0;
            std::uint32_t p328 = 0;
            ss >> n >> p328;
            std::vector<std::uint32_t> mods(n);
            std::vector<std::uint8_t> m94(n);
            std::vector<std::int32_t> v158(n);
            for (std::size_t i = 0; i < n; ++i) {
                unsigned m = 0;
                ss >> mods[i] >> m >> v158[i];
                m94[i] = static_cast<std::uint8_t>(m);
            }
            std::cout << objectQueryModuleValue158Shifted78(
                mods.data(), m94.data(), v158.data(), n, p328) << "\n";
        } else if (cmd == "f_6a7930") {
            std::uint32_t s[7]{}, gm[7]{}, sb4 = 0;
            for (int i = 0; i < 7; ++i) ss >> s[i];
            for (int i = 0; i < 7; ++i) ss >> gm[i];
            ss >> sb4;
            std::cout << objectMatchesGlobalStatusMaskOrBitB4(s, gm, sb4) << "\n";
        } else if (cmd == "f_6a7970") {
            std::uint32_t sx = 0, sy = 0, m08 = 0, m18 = 0, m28 = 0, tx = 0, ty = 0;
            ss >> sx >> sy >> m08 >> m18 >> m28 >> tx >> ty;
            std::cout << objectIsTargetBehind2D(
                bitsToFloat(sx), bitsToFloat(sy), bitsToFloat(m08),
                bitsToFloat(m18), bitsToFloat(m28), bitsToFloat(tx), bitsToFloat(ty)) << "\n";
        } else if (cmd == "f_6abc00") {
            std::uint32_t words[15]{};
            for (int i = 0; i < 15; ++i) ss >> words[i];
            std::uint32_t cbit = 0, sbit = 0;
            int hasTeam = 0;
            std::int32_t teamCnt = 0;
            ss >> cbit >> sbit >> hasTeam >> teamCnt;
            bool dirty = false;
            objectTransitionStatusBitBC00(words, cbit, sbit, hasTeam != 0, teamCnt, dirty);
            std::cout << (dirty ? 1 : 0) << " " << teamCnt;
            for (int i = 0; i < 15; ++i) std::cout << " " << words[i];
            std::cout << "\n";
        } else if (cmd == "f_6abce0") {
            std::uint32_t ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
            ss >> ax >> ay >> az >> bx >> by >> bz;
            const float r = objectGetDistanceSquaredToObject(
                Vec3f6A{bitsToFloat(ax), bitsToFloat(ay), bitsToFloat(az)},
                Vec3f6A{bitsToFloat(bx), bitsToFloat(by), bitsToFloat(bz)});
            std::cout << floatToBits(r) << "\n";
        } else {
            std::cout << "error\n";
        }
    }
    return 0;
}
