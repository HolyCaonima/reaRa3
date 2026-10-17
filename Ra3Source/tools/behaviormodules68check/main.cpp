#include "ra3/gamelogic/BehaviorModules68.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

float bitsToFloat(std::uint32_t bits) {
    union {
        std::uint32_t u;
        float f;
    } v{bits};
    return v.f;
}

std::uint32_t floatToBits(float f) {
    union {
        float f;
        std::uint32_t u;
    } v{f};
    return v.u;
}

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

std::int32_t parseI32(const std::string& token) {
    return static_cast<std::int32_t>(std::strtol(token.c_str(), nullptr, 0));
}

std::string hex32(std::uint32_t val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << val;
    return oss.str();
}

}  // namespace

int main() {
    using namespace ra3::gamelogic;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) {
            continue;
        }

        if (cmd == "quit") {
            break;
        }
        if (cmd == "find_module_by_key") {
            std::string tk, tn;
            iss >> tk >> tn;
            const std::uint32_t targetKey = parseU32(tk);
            const std::size_t count = static_cast<std::size_t>(parseU32(tn));
            std::vector<std::uint32_t> addrs(count);
            std::vector<std::uint32_t> keys(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string ma, mk;
                iss >> ma >> mk;
                addrs[i] = parseU32(ma);
                keys[i] = parseU32(mk);
            }
            const std::uint32_t res = findBehaviorModuleByKey(
                addrs.data(), keys.data(), count, targetKey);
            std::cout << "res=" << hex32(res) << "\n";
        } else if (cmd == "aim_weapon_target") {
            std::string sx, sy, sz, om, ix, iy, iz;
            iss >> sx >> sy >> sz >> om >> ix >> iy >> iz;
            Coord3D68 stored{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            std::int32_t mode = parseI32(om);
            const Coord3D68 input{bitsToFloat(parseU32(ix)), bitsToFloat(parseU32(iy)), bitsToFloat(parseU32(iz))};
            updateAimWeaponTargetPosition(stored, mode, input);
            std::cout << "x=" << hex32(floatToBits(stored.x))
                      << " y=" << hex32(floatToBits(stored.y))
                      << " z=" << hex32(floatToBits(stored.z))
                      << " mode=" << mode << "\n";
        } else if (cmd == "aim_weapon_remap") {
            std::string sm, isMod;
            iss >> sm >> isMod;
            std::uint32_t outPair[2]{0u, 0u};
            if (parseI32(isMod) != 0) {
                getAimWeaponModuleSlotMask(outPair, parseU32(sm));
            } else {
                remapAimWeaponSlotMask(outPair, parseU32(sm));
            }
            std::cout << "p0=" << hex32(outPair[0]) << " p1=" << hex32(outPair[1]) << "\n";
        } else if (cmd == "cast_subobj_10") {
            std::string sub;
            iss >> sub;
            std::cout << "res=" << hex32(castBehaviorModuleToSubobjectPlus10(parseU32(sub))) << "\n";
        } else if (cmd == "cast_subobj_1c") {
            std::string sub;
            iss >> sub;
            std::cout << "res=" << hex32(castBehaviorModuleToSubobjectPlus1C(parseU32(sub))) << "\n";
        } else if (cmd == "static_get_from_obj") {
            std::string which, g, ck, rk, tn;
            iss >> which >> g >> ck >> rk >> tn;
            const StaticModuleKeyState state{parseU32(g), parseU32(ck)};
            const std::uint32_t resolverKey = parseU32(rk);
            const std::size_t count = static_cast<std::size_t>(parseU32(tn));
            std::vector<std::uint32_t> addrs(count);
            std::vector<std::uint32_t> keys(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string ma, mk;
                iss >> ma >> mk;
                addrs[i] = parseU32(ma);
                keys[i] = parseU32(mk);
            }
            StaticModuleFinderResult res{};
            const int w = parseI32(which);
            if (w == 0) {
                res = getAssistedPlacementBehaviorFromObject(state, resolverKey, addrs.data(), keys.data(), count);
            } else if (w == 1) {
                res = getAssistedPlacementInfoBehaviorFromObject(state, resolverKey, addrs.data(), keys.data(), count);
            } else if (w == 2) {
                res = getBuildingBehaviorFromObject(state, resolverKey, addrs.data(), keys.data(), count);
            } else if (w == 3) {
                res = getFXListBehaviorFromObject(state, resolverKey, addrs.data(), keys.data(), count);
            } else {
                res = getStructurePlacementBehaviorFromObject(state, resolverKey, addrs.data(), keys.data(), count);
            }
            std::cout << "mod=" << hex32(res.moduleAddress)
                      << " guard=" << hex32(res.guard)
                      << " key=" << hex32(res.cachedKey)
                      << " called=" << (res.resolverCalled ? 1 : 0) << "\n";
        } else if (cmd == "assisted_place_init") {
            std::uint32_t b9[9]{}, b15[15]{}, b7[7]{};
            for (int i = 0; i < 9; ++i) { std::string s; iss >> s; b9[i] = parseU32(s); }
            for (int i = 0; i < 15; ++i) { std::string s; iss >> s; b15[i] = parseU32(s); }
            for (int i = 0; i < 7; ++i) { std::string s; iss >> s; b7[i] = parseU32(s); }
            AssistedPlacementEntry68 entry{};
            initAssistedPlacementEntry(entry, b9, b15, b7);
            std::cout << "vt=" << hex32(entry.vtable)
                      << " f04=" << hex32(entry.field04)
                      << " b9_0=" << hex32(entry.block9[0])
                      << " b9_8=" << hex32(entry.block9[8])
                      << " b15_0=" << hex32(entry.block15[0])
                      << " b15_14=" << hex32(entry.block15[14])
                      << " b7_0=" << hex32(entry.block7[0])
                      << " b7_6=" << hex32(entry.block7[6])
                      << " flg=" << static_cast<unsigned>(entry.hasBlock9)
                      << ":" << static_cast<unsigned>(entry.hasBlock15)
                      << ":" << static_cast<unsigned>(entry.hasBlock7) << "\n";
        } else if (cmd == "auto_heal_damage") {
            std::string sub, own, upg, rad, dly, dt, shf, hgl, cf, sc, fps;
            iss >> sub >> own >> upg >> rad >> dly >> dt >> shf >> hgl >> cf >> sc >> fps;
            const WakeScheduleCall res = onAutoHealDamage(
                parseU32(sub), parseU32(own), parseI32(upg) != 0,
                parseU32(rad), bitsToFloat(parseU32(dly)), parseU32(dt),
                parseU32(shf), parseI32(hgl) != 0, parseU32(cf),
                bitsToFloat(parseU32(sc)), bitsToFloat(parseU32(fps)));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " obj=" << hex32(res.objectAddress)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame) << "\n";
        } else if (cmd == "auto_heal_reset_fields") {
            std::uint32_t f[9]{};
            for (int i = 0; i < 9; ++i) { f[i] = 0xDEADBEEFu; }
            resetAutoHealStateFields(f);
            std::cout << "f0=" << hex32(f[0]) << " f6=" << hex32(f[6])
                      << " f7=" << hex32(f[7]) << " f8=" << hex32(f[8]) << "\n";
        } else if (cmd == "auto_heal_flag_1000") {
            std::string flg, en;
            iss >> flg >> en;
            std::uint32_t f = parseU32(flg);
            setAutoHealFlag1000(f, parseI32(en) != 0);
            std::cout << "f40=" << hex32(f) << "\n";
        } else if (cmd == "auto_heal_enable_sched") {
            std::string sub, own, cf, en, f18;
            iss >> sub >> own >> cf >> en >> f18;
            std::uint32_t val18 = parseU32(f18);
            const WakeScheduleCall res = setAutoHealEnabledAndSchedule(
                parseU32(sub), parseU32(own), parseU32(cf), parseI32(en) != 0, val18);
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " obj=" << hex32(res.objectAddress)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame)
                      << " f18=" << hex32(val18) << "\n";
        } else if (cmd == "bezier_advance") {
            std::string cnt, idx, f7c, fc8, fcc, step;
            iss >> cnt >> idx >> f7c >> fc8 >> fcc >> step;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<Coord3D68> wps(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string sx, sy, sz;
                iss >> sx >> sy >> sz;
                wps[i] = Coord3D68{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            }
            float c8 = bitsToFloat(parseU32(fc8));
            float cc = bitsToFloat(parseU32(fcc));
            Coord3D68 outPos{};
            const std::uint32_t ret = advanceBezierProjectileAlongPath(
                wps.data(), count, parseU32(idx), bitsToFloat(parseU32(f7c)),
                c8, cc, bitsToFloat(parseU32(step)), outPos);
            std::cout << "ret=" << ret
                      << " c8=" << hex32(floatToBits(c8))
                      << " cc=" << hex32(floatToBits(cc))
                      << " x=" << hex32(floatToBits(outPos.x))
                      << " y=" << hex32(floatToBits(outPos.y))
                      << " z=" << hex32(floatToBits(outPos.z)) << "\n";
        } else if (cmd == "bezier_seg_dist") {
            std::string cnt, idx, flg, thr, acc;
            iss >> cnt >> idx >> flg >> thr >> acc;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<Coord3D68> wps(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string sx, sy, sz;
                iss >> sx >> sy >> sz;
                wps[i] = Coord3D68{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            }
            float c0 = bitsToFloat(parseU32(acc));
            const bool ok = updateBezierProjectileSegmentDistance(
                wps.data(), count, parseI32(idx), parseU32(flg), bitsToFloat(parseU32(thr)), c0);
            std::cout << "ok=" << (ok ? 1 : 0)
                      << " c0=" << hex32(floatToBits(c0)) << "\n";
        } else if (cmd == "bezier_prev_curr") {
            std::string cnt, idx;
            iss >> cnt >> idx;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<Coord3D68> wps(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string sx, sy, sz;
                iss >> sx >> sy >> sz;
                wps[i] = Coord3D68{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            }
            Coord3D68 prev{bitsToFloat(0x11111111u), bitsToFloat(0x22222222u), bitsToFloat(0x33333333u)};
            Coord3D68 curr{bitsToFloat(0x44444444u), bitsToFloat(0x55555555u), bitsToFloat(0x66666666u)};
            getBezierProjectilePreviousAndCurrentWaypoints(wps.data(), count, parseI32(idx), prev, curr);
            std::cout << "px=" << hex32(floatToBits(prev.x))
                      << " py=" << hex32(floatToBits(prev.y))
                      << " pz=" << hex32(floatToBits(prev.z))
                      << " cx=" << hex32(floatToBits(curr.x))
                      << " cy=" << hex32(floatToBits(curr.y))
                      << " cz=" << hex32(floatToBits(curr.z)) << "\n";
        } else if (cmd == "bridge_has_null") {
            std::string cnt;
            iss >> cnt;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> slots(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                slots[i] = parseU32(s);
            }
            std::cout << "res=" << (hasNullBridgeTowerSlot(slots.data(), count) ? 1 : 0) << "\n";
        } else if (cmd == "bridge_get_slot") {
            std::string cnt, idx;
            iss >> cnt >> idx;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> slots(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                slots[i] = parseU32(s);
            }
            std::cout << "res=" << hex32(getBridgeTowerSlotByIndex(slots.data(), count, parseU32(idx))) << "\n";
        } else if (cmd == "bridge_sec_pos") {
            std::string f10, f18, hasF20, f20, c34, c38, tx, ty, tz, ox, oy, oz, md;
            iss >> f10 >> f18 >> hasF20 >> f20 >> c34 >> c38 >> tx >> ty >> tz >> ox >> oy >> oz >> md;
            const float optVal = bitsToFloat(parseU32(f20));
            const float* optPtr = (parseI32(hasF20) != 0) ? &optVal : nullptr;
            const Coord3D68 tpos{bitsToFloat(parseU32(tx)), bitsToFloat(parseU32(ty)), bitsToFloat(parseU32(tz))};
            const Coord3D68 opos{bitsToFloat(parseU32(ox)), bitsToFloat(parseU32(oy)), bitsToFloat(parseU32(oz))};
            Coord3D68 out{};
            computeBridgeSectionPosition(
                bitsToFloat(parseU32(f10)), bitsToFloat(parseU32(f18)), optPtr,
                parseI32(c34), parseI32(c38), tpos, opos, parseU32(md), out);
            std::cout << "x=" << hex32(floatToBits(out.x))
                      << " y=" << hex32(floatToBits(out.y))
                      << " z=" << hex32(floatToBits(out.z)) << "\n";
        } else if (cmd == "bridge_span_counts") {
            std::string f10, f18, alt1c, hasF20, f20, tx, ty, ox, oy;
            iss >> f10 >> f18 >> alt1c >> hasF20 >> f20 >> tx >> ty >> ox >> oy;
            const float optVal = bitsToFloat(parseU32(f20));
            const float* optPtr = (parseI32(hasF20) != 0) ? &optVal : nullptr;
            std::uint32_t c34 = 0u, c38 = 0u;
            computeBridgeSpanSegmentCounts(
                bitsToFloat(parseU32(f10)), bitsToFloat(parseU32(f18)), parseU32(alt1c), optPtr,
                bitsToFloat(parseU32(tx)), bitsToFloat(parseU32(ty)),
                bitsToFloat(parseU32(ox)), bitsToFloat(parseU32(oy)),
                c34, c38);
            std::cout << "c34=" << c34 << " c38=" << c38 << "\n";
        } else if (cmd == "building_alert") {
            std::string mod, own, amm, lfps, cf, ns, cd28, f2d, s2e;
            iss >> mod >> own >> amm >> lfps >> cf >> ns >> cd28 >> f2d >> s2e;
            std::uint32_t c28 = parseU32(cd28);
            std::uint8_t fl2d = static_cast<std::uint8_t>(parseU32(f2d));
            std::uint8_t st2e = static_cast<std::uint8_t>(parseU32(s2e));
            const WakeScheduleCall res = setBuildingBehaviorAlertState(
                parseU32(mod), parseU32(own), parseU32(amm), parseU32(lfps),
                parseU32(cf), static_cast<std::uint8_t>(parseU32(ns)),
                c28, fl2d, st2e);
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " wake=" << hex32(res.wakeFrame)
                      << " c28=" << hex32(c28)
                      << " f2d=" << static_cast<unsigned>(fl2d)
                      << " s2e=" << static_cast<unsigned>(st2e) << "\n";
        } else if (cmd == "building_reset_bone") {
            std::string hs, ro, bi, mbc;
            iss >> hs >> ro >> bi >> mbc;
            const BoneDamageResetCall res = resetBuildingDestructionBoneDamage(
                parseI32(hs) != 0, parseU32(ro), parseI32(bi), parseI32(mbc));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " mod=" << hex32(res.modelAddress)
                      << " bi=" << res.boneIndex
                      << " val=" << hex32(res.valueBits) << "\n";
        } else if (cmd == "building_closest_bone") {
            std::string ng, qx, qy, qz, og, ob;
            iss >> ng >> qx >> qy >> qz >> og >> ob;
            const std::uint32_t numGroups = parseU32(ng);
            std::vector<std::uint32_t> counts(numGroups);
            std::vector<std::vector<BuildingBoneCandidate68>> storage(numGroups);
            std::vector<const BuildingBoneCandidate68*> ptrs(numGroups);
            for (std::uint32_t g = 0; g < numGroups; ++g) {
                std::string sc;
                iss >> sc;
                counts[g] = parseU32(sc);
                storage[g].resize(counts[g]);
                for (std::uint32_t b = 0; b < counts[g]; ++b) {
                    std::string m04, px, py, pz;
                    iss >> m04 >> px >> py >> pz;
                    storage[g][b].activeMetric04 = bitsToFloat(parseU32(m04));
                    storage[g][b].position0C = Coord3D68{
                        bitsToFloat(parseU32(px)), bitsToFloat(parseU32(py)), bitsToFloat(parseU32(pz))};
                }
                ptrs[g] = storage[g].data();
            }
            const Coord3D68 qp{bitsToFloat(parseU32(qx)), bitsToFloat(parseU32(qy)), bitsToFloat(parseU32(qz))};
            std::uint32_t outG = parseU32(og);
            std::uint32_t outB = parseU32(ob);
            findClosestBuildingDestructionBone(ptrs.data(), counts.data(), numGroups, qp, outG, outB);
            std::cout << "g=" << hex32(outG) << " b=" << hex32(outB) << "\n";
        } else if (cmd == "energy_disabling_notify") {
            std::string own, pl, em, oid;
            iss >> own >> pl >> em >> oid;
            const EnergyDisablingNotifyCall res = notifyOwnerPlayerEnergyDisabling(
                parseU32(own), parseU32(pl), parseU32(em), parseU32(oid));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " em=" << hex32(res.energyManagerAddress)
                      << " oid=" << hex32(res.ownerObjectId) << "\n";
        } else if (cmd == "fire_weapon_copy") {
            FireWeaponDamagedState68 src{}, dst{};
            std::uint32_t* sWords = reinterpret_cast<std::uint32_t*>(&src);
            for (int i = 0; i < 11; ++i) {
                std::string s;
                iss >> s;
                sWords[i] = parseU32(s);
            }
            copyFireWeaponWhenDamagedState(dst, src);
            const std::uint32_t* dWords = reinterpret_cast<const std::uint32_t*>(&dst);
            std::cout << "w0=" << hex32(dWords[0])
                      << " w4=" << hex32(dWords[4])
                      << " w10=" << hex32(dWords[10]) << "\n";
        } else if (cmd == "fx_find_trigger") {
            std::string base, cnt, tk;
            iss >> base >> cnt >> tk;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> keys(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                keys[i] = parseU32(s);
            }
            const std::uint32_t res = findFXListBehaviorEntryByTrigger(
                parseU32(base), keys.data(), count, parseU32(tk));
            std::cout << "res=" << hex32(res) << "\n";
        } else if (cmd == "gate_can_pass") {
            std::string gs, ho, tf, ogs;
            iss >> gs >> ho >> tf >> ogs;
            const bool res = canObjectPassThroughGate(
                parseU32(gs), parseI32(ho) != 0, parseU32(tf), parseU32(ogs));
            std::cout << "res=" << (res ? 1 : 0) << "\n";
        } else if (cmd == "gate_stop_audio") {
            std::string ah;
            iss >> ah;
            const GateAudioStopResult res = stopGateAudioAndMarkClosed(parseU32(ah));
            std::cout << "ah=" << hex32(res.audioHandlePassed)
                      << " f4c=" << static_cast<unsigned>(res.closedFlag4C) << "\n";
        } else if (cmd == "sweeping_laser_pos") {
            std::string ef, cf, df, rev, sx, sy, sz, ex, ey, ez;
            iss >> ef >> cf >> df >> rev >> sx >> sy >> sz >> ex >> ey >> ez;
            const Coord3D68 sp{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            const Coord3D68 ep{bitsToFloat(parseU32(ex)), bitsToFloat(parseU32(ey)), bitsToFloat(parseU32(ez))};
            Coord3D68 out{};
            computeSweepingLaserPosition(
                parseU32(ef), parseU32(cf), parseU32(df), parseI32(rev) != 0, sp, ep, out);
            std::cout << "x=" << hex32(floatToBits(out.x))
                      << " y=" << hex32(floatToBits(out.y))
                      << " z=" << hex32(floatToBits(out.z)) << "\n";
        } else if (cmd == "lua_sum_entries") {
            std::string cnt;
            iss >> cnt;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> vals(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                vals[i] = parseU32(s);
            }
            std::cout << "res=" << hex32(sumLUAEventListEntries(vals.data(), count)) << "\n";
        } else if (cmd == "parking_has_reserved") {
            std::string gi, oid, cnt;
            iss >> gi >> oid >> cnt;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> ids(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                ids[i] = parseU32(s);
            }
            const bool res = hasParkingPlaceReservedSpace(
                parseI32(gi) != 0, parseU32(oid), ids.data(), count);
            std::cout << "res=" << (res ? 1 : 0) << "\n";
        } else if (cmd == "parking_can_reserve") {
            std::string os, tmpl, pl, vr;
            iss >> os >> tmpl >> pl >> vr;
            const ParkingCanReserveResult res = canParkingPlaceReserveSpaceForObject(
                parseU32(os), parseU32(tmpl), parseU32(pl), parseI32(vr) != 0);
            std::cout << "ok=" << (res.allowed ? 1 : 0)
                      << " vc=" << (res.virtualCalled ? 1 : 0)
                      << " tmpl=" << hex32(res.templatePassed)
                      << " pl=" << hex32(res.playerPassed) << "\n";
        } else if (cmd == "parking_on_exit") {
            std::string oid, cnt, dec;
            iss >> oid >> cnt >> dec;
            const ParkingExitResult res = onParkingPlaceObjectExit(
                parseU32(oid), parseU32(cnt), parseU32(dec));
            std::cout << "oid=" << hex32(res.releasedObjectId)
                      << " cnt=" << hex32(res.updatedPlayerCounter1314) << "\n";
        } else if (cmd == "parking_release_door") {
            std::string cnt, ed;
            iss >> cnt >> ed;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<ParkingSpaceSlot68> spaces(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string d, o, r;
                iss >> d >> o >> r;
                spaces[i] = ParkingSpaceSlot68{parseI32(d), parseU32(o), static_cast<std::uint8_t>(parseU32(r))};
            }
            releaseParkingPlaceDoorForExit(spaces.data(), count, parseI32(ed));
            std::cout << "cnt=" << count;
            for (std::size_t i = 0; i < count; ++i) {
                std::cout << " s" << i << "=" << hex32(spaces[i].objectInSpace34)
                          << ":" << static_cast<unsigned>(spaces[i].reservedForExit38);
            }
            std::cout << "\n";
        } else if (cmd == "parking_match_filter") {
            std::string fa, ca, pa, rv;
            iss >> fa >> ca >> pa >> rv;
            const ObjectFilterCallResult res = matchesParkingPlaceObjectFilter(
                parseU32(fa), parseU32(ca), parseU32(pa), parseU32(rv));
            std::cout << "res=" << hex32(res.result)
                      << " fa=" << hex32(res.filterAddress)
                      << " ca=" << hex32(res.objectAddress)
                      << " pa=" << hex32(res.playerAddress) << "\n";
        } else if (cmd == "passive_area_val") {
            std::string v2c;
            iss >> v2c;
            std::uint32_t out = 0u;
            const std::uint32_t eax = getPassiveAreaEffectValue(parseU32(v2c), out);
            std::cout << "eax=" << hex32(eax) << " out=" << hex32(out) << "\n";
        } else if (cmd == "ppm_owner") {
            std::string own;
            iss >> own;
            std::cout << "res=" << hex32(getPlayerPowerManagerOwnerObject(parseU32(own))) << "\n";
        } else if (cmd == "ppm_is_for_obj") {
            std::string own, cand;
            iss >> own >> cand;
            std::cout << "res=" << (isPlayerPowerManagerForObject(parseU32(own), parseU32(cand)) ? 1 : 0) << "\n";
        } else if (cmd == "ppm_entry_count") {
            std::string b, e;
            iss >> b >> e;
            std::cout << "res=" << getPlayerPowerManagerEntryCount(parseU32(b), parseU32(e)) << "\n";
        } else if (cmd == "ppm_entry_by_idx") {
            std::string cnt, idx;
            iss >> cnt >> idx;
            const std::int32_t count = parseI32(cnt);
            std::vector<std::uint32_t> entries(count > 0 ? static_cast<std::size_t>(count) : 0u);
            for (std::int32_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                entries[static_cast<std::size_t>(i)] = parseU32(s);
            }
            std::cout << "res=" << hex32(getPlayerPowerManagerEntryByIndex(entries.data(), count, parseI32(idx))) << "\n";
        } else if (cmd == "ppm_find_active") {
            std::string cnt, md, hop, initOut;
            iss >> cnt >> md >> hop >> initOut;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<PowerManagerEntryProbe68> entries(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string ea, mv, f0, f1;
                iss >> ea >> mv >> f0 >> f1;
                entries[i] = PowerManagerEntryProbe68{
                    parseU32(ea), bitsToFloat(parseU32(mv)), parseI32(f0) != 0, parseI32(f1) != 0};
            }
            std::uint32_t outAddr = parseU32(initOut);
            const bool found = findActivePlayerPowerManagerEntry(
                entries.data(), count, parseU32(md), parseI32(hop) != 0, outAddr);
            std::cout << "found=" << (found ? 1 : 0) << " out=" << hex32(outAddr) << "\n";
        } else if (cmd == "poisoned_on_healing") {
            std::string sub, own, hd, tb, cf;
            iss >> sub >> own >> hd >> tb >> cf;
            const PoisonedHealingResult res = onPoisonedBehaviorHealing(
                parseU32(sub), parseU32(own), parseI32(hd) != 0,
                static_cast<std::uint8_t>(parseU32(tb)), parseU32(cf));
            std::cout << "f04=" << hex32(res.poisonDamageFrame04)
                      << " f08=" << hex32(res.poisonStopFrame08)
                      << " f0c=" << hex32(res.poisonDamageAmountBits0C)
                      << " tb=" << static_cast<unsigned>(res.drawableTintByte14C)
                      << " wake=" << hex32(res.wake.wakeFrame) << "\n";
        } else if (cmd == "radar_enable") {
            std::string vc, pl, oid, f04;
            iss >> vc >> pl >> oid >> f04;
            const RadarEnableResult res = enableObjectRadarOnPlayer(
                parseI32(vc) != 0, parseU32(pl), parseU32(oid), static_cast<std::uint8_t>(parseU32(f04)));
            std::cout << "add=" << (res.playerRadarAdded ? 1 : 0)
                      << " pl=" << hex32(res.playerAddress)
                      << " oid=" << hex32(res.objectIdPassed)
                      << " f04=" << static_cast<unsigned>(res.flag04) << "\n";
        } else if (cmd == "radar_disable") {
            std::string pl, oid;
            iss >> pl >> oid;
            const RadarDisableResult res = disableObjectRadarOnPlayer(parseU32(pl), parseU32(oid));
            std::cout << "rem=" << (res.playerRadarRemoved ? 1 : 0)
                      << " pl=" << hex32(res.playerAddress)
                      << " oid=" << hex32(res.objectIdPassed) << "\n";
        } else if (cmd == "restrict_sp_pos") {
            std::string ho, ox, oy, oz;
            iss >> ho >> ox >> oy >> oz;
            const Coord3D68 opos{bitsToFloat(parseU32(ox)), bitsToFloat(parseU32(oy)), bitsToFloat(parseU32(oz))};
            Coord3D68 out{};
            getRestrictSpecialPowerPosition(parseI32(ho) != 0, opos, out);
            std::cout << "x=" << hex32(floatToBits(out.x))
                      << " y=" << hex32(floatToBits(out.y))
                      << " z=" << hex32(floatToBits(out.z)) << "\n";
        } else if (cmd == "restrict_sp_sum") {
            std::string cnt, lim;
            iss >> cnt >> lim;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<float> weights(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                weights[i] = bitsToFloat(parseU32(s));
            }
            const float sum = sumRestrictSpecialPowerWeights(weights.data(), count, parseI32(lim));
            std::cout << "sum=" << hex32(floatToBits(sum)) << "\n";
        } else if (cmd == "sec_listener_find") {
            std::string base, cnt, isHas;
            iss >> base >> cnt >> isHas;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint8_t> matches(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                matches[i] = static_cast<std::uint8_t>(parseU32(s));
            }
            const std::uint32_t res = (parseI32(isHas) != 0)
                ? hasSecondaryObjectListenerEntry(parseU32(base), matches.data(), count)
                : findSecondaryObjectListenerEntry(parseU32(base), matches.data(), count);
            std::cout << "res=" << hex32(res) << "\n";
        } else if (cmd == "share_exp_weight") {
            std::string r08, f0c, sx, sy, sz, tx, ty, tz;
            iss >> r08 >> f0c >> sx >> sy >> sz >> tx >> ty >> tz;
            const Coord3D68 sp{bitsToFloat(parseU32(sx)), bitsToFloat(parseU32(sy)), bitsToFloat(parseU32(sz))};
            const Coord3D68 tp{bitsToFloat(parseU32(tx)), bitsToFloat(parseU32(ty)), bitsToFloat(parseU32(tz))};
            const float w = calculateShareExperienceWeight(
                bitsToFloat(parseU32(r08)), bitsToFloat(parseU32(f0c)), sp, tp);
            std::cout << "w=" << hex32(floatToBits(w)) << "\n";
        } else if (cmd == "slow_death_prob") {
            std::string mh, bp, bo, dd, dc;
            iss >> mh >> bp >> bo >> dd >> dc;
            const std::int32_t res = getSlowDeathProbabilityModifier(
                bitsToFloat(parseU32(mh)), parseI32(bp), bitsToFloat(parseU32(bo)),
                bitsToFloat(parseU32(dd)), bitsToFloat(parseU32(dc)));
            std::cout << "res=" << res << "\n";
        } else if (cmd == "spawn_sched_upgrade") {
            std::string sub, own, f59, f90, dly, hgl, cf, sc, fps;
            iss >> sub >> own >> f59 >> f90 >> dly >> hgl >> cf >> sc >> fps;
            const WakeScheduleCall res = scheduleSpawnBehaviorOnUpgrade(
                parseU32(sub), parseU32(own), static_cast<std::uint8_t>(parseU32(f59)),
                parseU32(f90), bitsToFloat(parseU32(dly)), parseI32(hgl) != 0,
                parseU32(cf), bitsToFloat(parseU32(sc)), bitsToFloat(parseU32(fps)));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame) << "\n";
        } else if (cmd == "spawn_sched_replace") {
            std::string sub, own, f59, dly, hgl, cf, sc, fps;
            iss >> sub >> own >> f59 >> dly >> hgl >> cf >> sc >> fps;
            const WakeScheduleCall res = scheduleSpawnBehaviorReplacement(
                parseU32(sub), parseU32(own), static_cast<std::uint8_t>(parseU32(f59)),
                bitsToFloat(parseU32(dly)), parseI32(hgl) != 0,
                parseU32(cf), bitsToFloat(parseU32(sc)), bitsToFloat(parseU32(fps)));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame) << "\n";
        } else if (cmd == "spawn_should_try") {
            std::string a68, os4c, st80, dm, nc;
            iss >> a68 >> os4c >> st80 >> dm >> nc;
            const SpawnTryResult res = shouldSpawnBehaviorTryToSpawn(
                static_cast<std::uint8_t>(parseU32(a68)),
                static_cast<std::uint8_t>(parseU32(os4c)),
                parseU32(st80), parseU32(dm),
                static_cast<std::uint8_t>(parseU32(nc)));
            std::cout << "try=" << (res.shouldSpawn ? 1 : 0)
                      << " stop=" << (res.stopSpawningCalled ? 1 : 0) << "\n";
        } else if (cmd == "stances_can_add") {
            std::string b14, e18, m08;
            iss >> b14 >> e18 >> m08;
            std::cout << "res=" << hex32(canStancesBehaviorAddEntry(parseU32(b14), parseU32(e18), parseU32(m08))) << "\n";
        } else if (cmd == "stances_get_or_pop") {
            std::string cnt, pop, initOut;
            iss >> cnt >> pop >> initOut;
            std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> entries(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                entries[i] = parseU32(s);
            }
            std::uint32_t outVal = parseU32(initOut);
            getOrPopLastStanceEntry(entries.data(), count, parseI32(pop) != 0, outVal);
            std::cout << "cnt=" << count << " out=" << hex32(outVal) << "\n";
        } else if (cmd == "stances_select_usage") {
            std::string cnt, maxU;
            iss >> cnt >> maxU;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<StanceUsageEntry68> entries(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string v, u;
                iss >> v >> u;
                entries[i] = StanceUsageEntry68{parseU32(v), static_cast<std::uint8_t>(parseU32(u))};
            }
            const std::uint32_t res = selectNextStanceEntryByUsage(entries.data(), count, parseI32(maxU));
            std::cout << "res=" << hex32(res);
            for (std::size_t i = 0; i < count; ++i) {
                std::cout << " u" << i << "=" << static_cast<unsigned>(entries[i].useCount04);
            }
            std::cout << "\n";
        } else if (cmd == "find_by_template_id") {
            std::string cnt, tid;
            iss >> cnt >> tid;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<BehaviorTemplateCandidate68> cands(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string sa, ti;
                iss >> sa >> ti;
                cands[i] = BehaviorTemplateCandidate68{parseU32(sa), parseU32(ti)};
            }
            const std::uint32_t res = findObjectBehaviorByTemplateId(cands.data(), count, parseU32(tid));
            std::cout << "res=" << hex32(res) << "\n";
        } else if (cmd == "stream_deactivate") {
            std::string sub, own, cf, f40;
            iss >> sub >> own >> cf >> f40;
            std::uint8_t flag = static_cast<std::uint8_t>(parseU32(f40));
            const WakeScheduleCall res = deactivateStreamStateAndSleep(
                parseU32(sub), parseU32(own), parseU32(cf), flag);
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame)
                      << " f40=" << static_cast<unsigned>(flag) << "\n";
        } else if (cmd == "supply_cripple_dmg") {
            std::string sub, own, fbef, faft, h04;
            iss >> sub >> own >> fbef >> faft >> h04;
            const WakeScheduleCall res = onSupplyWarehouseCripplingDamage(
                parseU32(sub), parseU32(own), parseU32(fbef), parseU32(faft), parseU32(h04));
            std::cout << "called=" << (res.called ? 1 : 0)
                      << " upd=" << hex32(res.updateSubobjectAddress)
                      << " wake=" << hex32(res.wakeFrame) << "\n";
        } else if (cmd == "supply_cripple_effects") {
            std::string cnt, isStart;
            iss >> cnt >> isStart;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> docks(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                docks[i] = parseU32(s);
            }
            const DockCrippledCallResult res = (parseI32(isStart) != 0)
                ? startSupplyWarehouseCrippledEffects(docks.data(), count)
                : stopSupplyWarehouseCrippledEffects(docks.data(), count);
            std::cout << "dock=" << hex32(res.dockInterfaceAddress)
                      << " arg=" << res.crippledArg << "\n";
        } else if (cmd == "tiberium_timers") {
            std::string cf, sc, lfps, bs70, i80, i84, r8c, t94, t98, t9c;
            iss >> cf >> sc >> lfps >> bs70 >> i80 >> i84 >> r8c >> t94 >> t98 >> t9c;
            std::uint32_t v94 = parseU32(t94), v98 = parseU32(t98), v9c = parseU32(t9c);
            const std::uint32_t sleep = updateTiberiumFieldTimersAndGetSleep(
                parseU32(cf), bitsToFloat(parseU32(sc)), parseU32(lfps),
                parseU32(bs70), parseU32(i80), parseU32(i84), bitsToFloat(parseU32(r8c)),
                v94, v98, v9c);
            std::cout << "sleep=" << hex32(sleep)
                      << " t94=" << hex32(v94)
                      << " t98=" << hex32(v98)
                      << " t9c=" << hex32(v9c) << "\n";
        } else if (cmd == "tiberium_set_active") {
            std::string fa0, na, cf, cnt;
            iss >> fa0 >> na >> cf >> cnt;
            std::uint8_t curA0 = static_cast<std::uint8_t>(parseU32(fa0));
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<TiberiumCrystalSubmodule68> subs(count);
            std::vector<TiberiumCrystalSlot68> slots(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s0, hc, hs, a28, f2c, s40;
                iss >> s0 >> hc >> hs >> a28 >> f2c >> s40;
                subs[i] = TiberiumCrystalSubmodule68{
                    static_cast<std::uint8_t>(parseU32(a28)), parseU32(f2c), parseU32(s40)};
                slots[i] = TiberiumCrystalSlot68{
                    parseU32(s0), parseU32(hc), (parseI32(hs) != 0) ? &subs[i] : nullptr};
            }
            setTiberiumFieldActiveState(
                curA0, static_cast<std::uint8_t>(parseU32(na)), parseU32(cf), slots.data(), count);
            std::cout << "fa0=" << static_cast<unsigned>(curA0);
            for (std::size_t i = 0; i < count; ++i) {
                std::cout << " s" << i << "=" << static_cast<unsigned>(subs[i].active28)
                          << ":" << hex32(subs[i].activatedFrame2C);
            }
            std::cout << "\n";
        } else if (cmd == "tiberium_bind_slot") {
            std::string fa0, ss0, g, ck, rk, cnt, a28, c40;
            iss >> fa0 >> ss0 >> g >> ck >> rk >> cnt >> a28 >> c40;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> addrs(count);
            std::vector<std::uint32_t> keys(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string ma, mk;
                iss >> ma >> mk;
                addrs[i] = parseU32(ma);
                keys[i] = parseU32(mk);
            }
            const BindCrystalSlotResult res = bindTiberiumFieldCrystalSlot(
                static_cast<std::uint8_t>(parseU32(fa0)), parseU32(ss0),
                StaticModuleKeyState{parseU32(g), parseU32(ck)}, parseU32(rk),
                addrs.data(), keys.data(), count,
                static_cast<std::uint8_t>(parseU32(a28)), parseU32(c40));
            std::cout << "mod=" << hex32(res.boundModuleAddress)
                      << " guard=" << hex32(res.finder.guard)
                      << " key=" << hex32(res.finder.cachedKey)
                      << " a28=" << static_cast<unsigned>(res.submoduleActive28)
                      << " c40=" << hex32(res.submoduleCrystalState40) << "\n";
        } else if (cmd == "tiberium_no_state_one") {
            std::string c38, r3c;
            iss >> c38 >> r3c;
            const std::int32_t cols = parseI32(c38);
            const std::int32_t rows = parseI32(r3c);
            const std::size_t total = (cols > 0 && rows > 0) ? static_cast<std::size_t>(cols * rows) : 0u;
            std::vector<std::uint32_t> states(total);
            for (std::size_t i = 0; i < total; ++i) {
                std::string s;
                iss >> s;
                states[i] = parseU32(s);
            }
            const bool res = hasNoTiberiumFieldSlotInStateOne(states.data(), cols, rows);
            std::cout << "res=" << (res ? 1 : 0) << "\n";
        } else if (cmd == "tiberium_active_state_eight") {
            std::string c38, r3c;
            iss >> c38 >> r3c;
            const std::int32_t cols = parseI32(c38);
            const std::int32_t rows = parseI32(r3c);
            const std::size_t total = (cols > 0 && rows > 0) ? static_cast<std::size_t>(cols * rows) : 0u;
            std::vector<std::uint32_t> states(total);
            std::vector<std::uint32_t> flags(total);
            for (std::size_t i = 0; i < total; ++i) {
                std::string s, f;
                iss >> s >> f;
                states[i] = parseU32(s);
                flags[i] = parseU32(f);
            }
            const bool res = hasActiveTiberiumFieldSlotInStateEight(
                states.data(), flags.data(), cols, rows);
            std::cout << "res=" << (res ? 1 : 0) << "\n";
        } else if (cmd == "tiberium_pick_random") {
            std::string cnt, si;
            iss >> cnt >> si;
            const std::size_t count = static_cast<std::size_t>(parseU32(cnt));
            std::vector<std::uint32_t> entries(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::string s;
                iss >> s;
                entries[i] = parseU32(s);
            }
            const std::uint32_t res = pickRandomTiberiumFieldEntry(
                entries.data(), count, parseU32(si));
            std::cout << "res=" << hex32(res) << "\n";
        } else {
            std::cerr << "Unknown command: " << cmd << "\n";
            return 1;
        }
    }
    return 0;
}
