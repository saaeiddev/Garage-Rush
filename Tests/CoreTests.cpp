#include "Core/GarageCore.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>

using namespace gr;
namespace {
int passed = 0, failed = 0;
void require(bool condition, const std::string& reason) { if (!condition) throw std::runtime_error(reason); }
void ok(Result r) { require(r.ok, r.message); }
void rejects(State& s, const std::function<Result()>& action) {
    const auto before = serialize(s); const auto r = action();
    require(!r.ok, "Invalid operation succeeded.");
    require(serialize(s) == before, "Failed operation changed session state: " + r.message);
}
void test(const std::string& name, const std::function<void()>& body) {
    try { body(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << '\n'; }
}
void prepare(State& s, const std::string& id) { ok(setEngine(s, id, false)); ok(setHood(s, id, true)); ok(setLift(s, id, true)); }
void detach(State& s, const std::string& v, const std::string& part, std::vector<std::pair<std::string, std::uint64_t>>& removed) {
    if (!s.vehicles.at(v).parts.at(part).installed) return;
    const auto* p = findPart(v, part);
    for (const auto& prereq : p->removeFirst) detach(s, v, prereq, removed);
    while (s.vehicles.at(v).parts.at(part).secured) ok(fastener(s, v, part, p->tool, false));
    const auto r = removePart(s, v, part, p->tool); ok(r); removed.emplace_back(part, r.item);
}
void secure(State& s, const std::string& v, const std::string& part) {
    const auto* p = findPart(v, part);
    while (s.vehicles.at(v).parts.at(part).secured < p->fasteners) ok(fastener(s, v, part, Tool::TorqueWrench, true));
}
void replaceOne(State& s, const std::string& v, const std::string& part, const std::string& sku = {}) {
    std::vector<std::pair<std::string, std::uint64_t>> removed;
    detach(s, v, part, removed);
    const auto purchaseResult = purchase(s, sku.empty() ? standardSku(v, findPart(v, part)->kind) : sku); ok(purchaseResult);
    for (auto it = removed.rbegin(); it != removed.rend(); ++it) {
        ok(installPart(s, v, it->first, it->first == part ? purchaseResult.item : it->second)); secure(s, v, it->first);
    }
}
void repairOrder(State& s) {
    const auto* j = findOrder(s.job->order);
    prepare(s, j->vehicle);
    for (const auto& fault : j->faults) {
        const auto kind = findPart(j->vehicle, fault.part)->kind;
        ok(inspect(s, j->vehicle, fault.part, kind == Kind::Battery || kind == Kind::Alternator || kind == Kind::Fuse ? Tool::Multimeter : Tool::Scanner));
        replaceOne(s, j->vehicle, fault.part);
        ok(validate(s));
    }
    ok(setLift(s, j->vehicle, false)); ok(setHood(s, j->vehicle, false));
}
void verifyAndReturn(State& s) {
    const auto* j = findOrder(s.job->order);
    const bool track = j->test == Test::Braking || j->test == Test::Grip || j->test == Test::Handling;
    if (track) ok(moveVehicle(s, j->vehicle, Location::Track));
    ok(runVerification(s));
    if (track) ok(moveVehicle(s, j->vehicle, Location::Bay));
}
std::uint32_t testCrc(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 16; i + 4 < bytes.size(); ++i) {
        crc ^= bytes[i]; for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return ~crc;
}
void refreshCrc(std::vector<std::uint8_t>& bytes) {
    const auto crc = testCrc(bytes); for (int b = 0; b < 4; ++b) bytes[bytes.size() - 4 + b] = static_cast<std::uint8_t>(crc >> (b * 8));
}
void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc); f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); require(f.good(), "Test fixture write failed.");
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    const auto root = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path() / "Garage Rush Core QA";
    std::filesystem::create_directories(root);
    test("Catalogs: three fictional vehicles, service parts, unique IDs and valid dependency graph", [] {
        require(vehicleCatalog().size() == 3 && orderCatalog().size() == 8, "Incorrect content count.");
        std::set<std::string> skus;
        for (const auto& s : shopCatalog()) require(skus.insert(s.id).second && findVehicle(s.vehicle), "Invalid shop catalog.");
        for (const auto& v : vehicleCatalog()) {
            require(v.parts.size() >= 20, "Insufficient serviceable parts.");
            std::set<std::string> ids;
            for (const auto& p : v.parts) {
                require(ids.insert(p.id).second, "Duplicate component ID.");
                require(findSku(standardSku(v.id, p.kind)) != nullptr, "Part has no compatible SKU.");
                std::set<std::string> visiting, visited;
                std::function<void(const PartDef&)> walk = [&](const PartDef& node) {
                    require(visiting.insert(node.id).second, "Circular removal dependency.");
                    for (const auto& d : node.removeFirst) { const auto* dep = findPart(v.id, d); require(dep != nullptr, "Unknown dependency."); if (!visited.count(d)) walk(*dep); }
                    visiting.erase(node.id); visited.insert(node.id);
                }; walk(p);
                for (const auto& d : p.installFirst) require(findPart(v.id, d) != nullptr, "Unknown installation prerequisite.");
            }
        }
    });
    test("Fresh career has valid, distinct serials and assembled cars", [] { const auto s = newGame(); ok(validate(s)); for (const auto& v : vehicleCatalog()) require(driveReady(s, v.id), "Fresh car is incomplete."); });
    test("Unknown and overlapping orders are atomic failures", [] { auto s = newGame(); rejects(s, [&] { return acceptOrder(s, "missing"); }); ok(acceptOrder(s, "01-battery")); rejects(s, [&] { return acceptOrder(s, "02-charge"); }); });
    test("Battery fault changes actual state-derived start diagnostics", [] { auto s = newGame(); auto healthy = diagnose(s, "hatch"); ok(acceptOrder(s, "01-battery")); auto faulty = diagnose(s, "hatch"); require(healthy.starts && !faulty.starts && faulty.crankSeconds > healthy.crankSeconds, "Battery has no diagnostic effect."); rejects(s, [&] { return setEngine(s, "hatch", true); }); repairOrder(s); require(diagnose(s, "hatch").starts, "Replacement did not restore starting."); });
    test("Access, tools and fasteners reject invalid removals", [] { auto s = newGame(); rejects(s, [&] { return removePart(s, "hatch", "battery", Tool::Socket); }); prepare(s, "hatch"); rejects(s, [&] { return removePart(s, "hatch", "battery", Tool::Hand); }); rejects(s, [&] { return removePart(s, "hatch", "battery", Tool::Socket); }); rejects(s, [&] { return fastener(s, "hatch", "battery", Tool::Hand, false); }); });
    test("Fastener operations are bounded and parts cannot duplicate", [] { auto s = newGame(); prepare(s, "hatch"); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "hatch", "battery", removed); const auto count = s.inventory.size(); rejects(s, [&] { return removePart(s, "hatch", "battery", Tool::Socket); }); rejects(s, [&] { return fastener(s, "hatch", "battery", Tool::Socket, false); }); require(s.inventory.size() == count, "Repeated removal duplicated a part."); ok(installPart(s, "hatch", "battery", removed[0].second)); secure(s, "hatch", "battery"); rejects(s, [&] { return fastener(s, "hatch", "battery", Tool::TorqueWrench, true); }); });
    test("Brake access requires wheel and pad removal before disc", [] { auto s = newGame(); prepare(s, "coupe"); rejects(s, [&] { return fastener(s, "coupe", "pads_fl", Tool::Socket, false); }); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "coupe", "wheel_fl", removed); rejects(s, [&] { return removePart(s, "coupe", "disc_fl", Tool::Socket); }); detach(s, "coupe", "disc_fl", removed); require(removed.size() == 3, "Brake removal sequence was incomplete."); ok(validate(s)); });
    test("Missing suspension/wheels block lowering and track movement", [] { auto s = newGame(); prepare(s, "suv"); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "suv", "shock_fr", removed); rejects(s, [&] { return setLift(s, "suv", false); }); rejects(s, [&] { return moveVehicle(s, "suv", Location::Track); }); rejects(s, [&] { return installPart(s, "suv", "wheel_fr", removed.front().second); }); });
    test("Purchase checks funds, stock and unknown SKUs without charges", [] { auto s = newGame(); s.money = 0; rejects(s, [&] { return purchase(s, "hatch:battery:standard"); }); rejects(s, [&] { return purchase(s, "unknown"); }); s.money = 150000; for (int i = 0; i < 8; ++i) ok(purchase(s, "hatch:battery:standard")); rejects(s, [&] { return purchase(s, "hatch:battery:standard"); }); ok(validate(s)); });
    test("Refund returns one unused purchase exactly once", [] { auto s = newGame(); auto before = serialize(s); const auto bought = purchase(s, "hatch:air_filter:standard"); ok(bought); ok(refund(s, bought.item)); require(s.money == 150000 && s.inventory.empty(), "Refund did not undo cost/item."); require(s.stock.at("hatch:air_filter:standard") == 8, "Refund did not restore stock."); rejects(s, [&] { return refund(s, bought.item); }); require(serialize(s) != before, "Serial generation was incorrectly rewound."); });
    test("Installed/removed purchased parts lose refund eligibility", [] { auto s = newGame(); prepare(s, "hatch"); replaceOne(s, "hatch", "air_filter"); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "hatch", "air_filter", removed); rejects(s, [&] { return refund(s, removed.back().second); }); });
    test("Wrong vehicle and wrong component fitting preserve inventory", [] { auto s = newGame(); prepare(s, "hatch"); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "hatch", "battery", removed); auto wrongVehicle = purchase(s, "suv:battery:standard"); auto wrongKind = purchase(s, "hatch:air_filter:standard"); ok(wrongVehicle); ok(wrongKind); rejects(s, [&] { return installPart(s, "hatch", "battery", wrongVehicle.item); }); rejects(s, [&] { return installPart(s, "hatch", "battery", wrongKind.item); }); });
    test("Mid-fastener repair roundtrip preserves every byte of state", [] { auto s = newGame(); ok(acceptOrder(s, "01-battery")); prepare(s, "hatch"); ok(fastener(s, "hatch", "battery", Tool::Socket, false)); auto bytes = serialize(s); State restored; ok(deserialize(bytes, restored)); require(serialize(restored) == bytes, "Partial fastener state changed after load."); });
    test("Removed part, serial, evidence and order survive serialization", [] { auto s = newGame(); ok(acceptOrder(s, "06-brakes")); prepare(s, "coupe"); ok(inspect(s, "coupe", "pads_fl", Tool::Scanner)); std::vector<std::pair<std::string, std::uint64_t>> removed; detach(s, "coupe", "disc_fl", removed); auto bytes = serialize(s); State restored; ok(deserialize(bytes, restored)); require(serialize(restored) == bytes && restored.inventory.size() == 3, "Interrupted repair state lost."); });
    test("Corrupted saves fail without replacing the current session", [] { auto s = newGame(); auto bytes = serialize(s); bytes[100] ^= 0x80; State live = newGame(Mode::Sandbox); const auto before = serialize(live); require(!deserialize(bytes, live), "Checksum corruption accepted."); require(serialize(live) == before, "Failed load changed live state."); });
    test("Unknown schema and semantic tampering are rejected", [] { auto s = newGame(); auto bytes = serialize(s); State out = s; bytes[8] = 2; require(!deserialize(bytes, out), "Unknown schema accepted."); bytes = serialize(s); bytes[16] = 8; refreshCrc(bytes); require(!deserialize(bytes, out), "Invalid mode with valid checksum accepted."); bytes = serialize(s); for (int i = 17; i < 25; ++i) bytes[i] = 0xFF; refreshCrc(bytes); require(!deserialize(bytes, out), "Overflowing money accepted."); });
    test("State validator catches duplicate serials, NaNs and broken prerequisites", [] { auto s = newGame(); auto invalid = s; invalid.vehicles.at("hatch").parts.at("starter").installed->serial = invalid.vehicles.at("hatch").parts.at("battery").installed->serial; require(!validate(invalid), "Duplicate serial accepted."); invalid = s; invalid.vehicles.at("coupe").parts.at("plugs_a").installed->health = std::numeric_limits<double>::quiet_NaN(); require(!validate(invalid), "NaN accepted."); invalid = s; invalid.vehicles.at("hatch").parts.at("alternator").installed.reset(); invalid.vehicles.at("hatch").parts.at("alternator").secured = 0; require(!validate(invalid), "Broken belt prerequisite accepted."); });
    test("All truncated save lengths reject atomically", [] { auto s = newGame(); const auto bytes = serialize(s); State out = newGame(Mode::Sandbox); const auto before = serialize(out); for (std::size_t n = 0; n < bytes.size(); ++n) require(!deserialize({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n)}, out), "Truncated save accepted."); require(serialize(out) == before, "Truncated read changed state."); });
    test("Unicode and spaces path supports real file save/load on this OS", [&] { auto s = newGame(); ok(acceptOrder(s, "01-battery")); prepare(s, "hatch"); const auto path = root / std::filesystem::path(u8"کاربر امیر") / "Garage Rush save.bin"; ok(save(s, path)); State restored; auto r = load(restored, path); ok(r.result); require(!r.recoveredBackup && serialize(restored) == serialize(s), "Unicode file roundtrip failed."); });
    test("Corrupt primary recovers the last valid backup", [&] { const auto path = root / "backup-test.bin"; auto first = newGame(); ok(save(first, path)); auto second = first; ok(acceptOrder(second, "01-battery")); ok(save(second, path)); writeBytes(path, {1, 2, 3}); State restored; const auto r = load(restored, path); ok(r.result); require(r.recoveredBackup && serialize(restored) == serialize(first), "Backup recovery restored the wrong state."); });
    test("Saving after a corrupt primary preserves the valid backup", [&] { const auto path = root / "preserve-backup.bin"; auto first = newGame(); ok(save(first, path)); auto second = first; ok(acceptOrder(second, "02-charge")); ok(save(second, path)); writeBytes(path, std::vector<std::uint8_t>(2 * 1024 * 1024 + 1, 0)); ok(save(second, path)); writeBytes(path, {9}); State out; auto r = load(out, path); ok(r.result); require(r.recoveredBackup && serialize(out) == serialize(first), "Corrupt primary overwrote valid backup."); });
    test("Missing primary and backup do not reset the session", [&] { auto s = newGame(); const auto before = serialize(s); auto r = load(s, root / "never-created.bin"); require(!r.result && serialize(s) == before, "Missing save replaced current session."); });
    test("Diagnostic tool choice and inspection evidence gate completion", [] { auto s = newGame(); ok(acceptOrder(s, "01-battery")); rejects(s, [&] { return inspect(s, "hatch", "battery", Tool::Hand); }); rejects(s, [&] { return runVerification(s); }); rejects(s, [&] { return completeOrder(s); }); prepare(s, "hatch"); replaceOne(s, "hatch", "battery"); ok(setLift(s, "hatch", false)); ok(setHood(s, "hatch", false)); rejects(s, [&] { return runVerification(s); }); ok(inspect(s, "hatch", "battery", Tool::Multimeter)); ok(runVerification(s)); ok(completeOrder(s)); });
    test("Mechanical changes invalidate verified payment", [] { auto s = newGame(); ok(acceptOrder(s, "01-battery")); repairOrder(s); ok(runVerification(s)); prepare(s, "hatch"); ok(fastener(s, "hatch", "battery", Tool::Socket, false)); ok(fastener(s, "hatch", "battery", Tool::TorqueWrench, true)); ok(setLift(s, "hatch", false)); ok(setHood(s, "hatch", false)); rejects(s, [&] { return completeOrder(s); }); ok(runVerification(s)); ok(completeOrder(s)); rejects(s, [&] { return completeOrder(s); }); });
    test("Session counter limits reject purchases/orders before overflow", [] { auto s = newGame(); s.nextSerial = UINT64_MAX - 1; rejects(s, [&] { return purchase(s, "hatch:battery:standard"); }); s.nextJob = UINT64_MAX - 1; rejects(s, [&] { return acceptOrder(s, "01-battery"); }); });
    test("Economy cap prevents refund/payment from creating invalid saves", [] { auto s = newGame(); auto bought = purchase(s, "hatch:battery:standard"); ok(bought); s.money = 1000000000000LL; rejects(s, [&] { return refund(s, bought.item); }); ok(acceptOrder(s, "01-battery")); repairOrder(s); ok(runVerification(s)); s.money = 1000000000000LL; rejects(s, [&] { return completeOrder(s); }); ok(validate(s)); });
    test("Save validation rejects fabricated verification on an unresolved fault", [] { auto s = newGame(); ok(acceptOrder(s, "01-battery")); s.job->verifiedRevision = s.vehicles.at("hatch").revision; require(!validate(s), "Uninspected faulty order accepted as verified."); });
    test("Braking order requires track location, then bay handover", [] { auto s = newGame(); ok(acceptOrder(s, "06-brakes")); repairOrder(s); rejects(s, [&] { return runVerification(s); }); ok(moveVehicle(s, "coupe", Location::Track)); ok(runVerification(s)); rejects(s, [&] { return completeOrder(s); }); ok(moveVehicle(s, "coupe", Location::Bay)); ok(completeOrder(s)); });
    for (const auto& order : orderCatalog()) test("Playable-core order repair/verify/reward: " + order.id, [&order] {
        auto s = newGame(); ok(acceptOrder(s, order.id)); const auto before = diagnose(s, order.vehicle); require(!before.faults.empty(), "Order produced no fault."); repairOrder(s); const auto after = diagnose(s, order.vehicle); require(after.faults.empty(), "Correct repair left fault indications."); verifyAndReturn(s); const auto money = s.money; ok(completeOrder(s)); require(s.money == money + order.payment && s.reputation == order.reputation, "Incorrect reward."); rejects(s, [&] { return completeOrder(s); }); ok(validate(s));
    });
    test("Eight orders in one career progress and a repeat order receives a new identity", [] { auto s = newGame(); for (const auto& order : orderCatalog()) { ok(acceptOrder(s, order.id)); repairOrder(s); verifyAndReturn(s); ok(completeOrder(s)); } require(s.reputation == 104 && s.money > 150000, "Career economy/progression failed."); const auto counter = s.nextJob; ok(acceptOrder(s, "01-battery")); require(s.job->instance == counter, "Repeated order reused old ID."); repairOrder(s); verifyAndReturn(s); ok(completeOrder(s)); require(s.completions.at("01-battery") == 2, "Repeat completion was lost."); ok(validate(s)); });
    test("Sandbox purchases are free and cannot mint career money", [] { auto s = newGame(Mode::Sandbox); const auto funds = s.money; const auto stock = s.stock; for (int i = 0; i < 12; ++i) { auto r = purchase(s, "coupe:tire:sport"); ok(r); rejects(s, [&] { return refund(s, r.item); }); } require(s.money == funds && s.stock == stock, "Sandbox changed economy."); rejects(s, [&] { return acceptOrder(s, "01-battery"); }); ok(validate(s)); });
    test("Compatible upgrades change grip, braking, damping and power", [] { auto s = newGame(Mode::Sandbox); const auto original = diagnose(s, "coupe"); prepare(s, "coupe"); for (const std::string corner : {"fl", "fr", "rl", "rr"}) { replaceOne(s, "coupe", "tire_" + corner, "coupe:tire:sport"); replaceOne(s, "coupe", "pads_" + corner, "coupe:pads:sport"); replaceOne(s, "coupe", "disc_" + corner, "coupe:disc:sport"); replaceOne(s, "coupe", "shock_" + corner, "coupe:shock:sport"); } replaceOne(s, "coupe", "air_filter", "coupe:air_filter:sport"); const auto tuned = diagnose(s, "coupe"); require(tuned.grip > original.grip && tuned.brakeDecel > original.brakeDecel && tuned.damping > original.damping && tuned.powerKw > original.powerKw && tuned.brake100Meters < original.brake100Meters, "Upgrades are cosmetic."); ok(validate(s)); });
    test("Settings, control bindings and independent effects roundtrip", [] { auto s = newGame(); Settings p; p.fov = 110; p.sensitivity = 2; p.textScale = 1.5; p.invertLook = true; p.assisted = false; p.motionBlur = true; p.depthOfField = true; p.preset = 4; p.volumes = {{.2, .3, .4, .5, .6}}; p.bindings.at("interact") = "F"; ok(updateSettings(s, p)); State out; const auto bytes = serialize(s); ok(deserialize(bytes, out)); require(serialize(out) == bytes, "Settings were not persisted."); p.fov = 300; rejects(s, [&] { return updateSettings(s, p); }); p.fov = 85; p.bindings.at("back") = "W"; rejects(s, [&] { return updateSettings(s, p); }); });
    test("Track recovery and save preserve installed modifications and active order", [] { auto s = newGame(); ok(acceptOrder(s, "07-tires")); repairOrder(s); ok(moveVehicle(s, "hatch", Location::Track)); ok(setEngine(s, "hatch", true)); const auto parts = s.vehicles.at("hatch").revision; const auto inventory = s.inventory.size(); ok(recoverVehicle(s, "hatch")); require(s.vehicles.at("hatch").revision == parts && s.inventory.size() == inventory && s.job, "Recovery reset mechanical state."); State out; const auto bytes = serialize(s); ok(deserialize(bytes, out)); require(serialize(out) == bytes && out.vehicles.at("hatch").location == Location::Track, "Track session did not roundtrip."); });
    test("5,000 random operations keep state valid; rejected transactions stay atomic", [] {
        auto s = newGame(Mode::Sandbox); std::mt19937 random(71923);
        for (int i = 0; i < 5000; ++i) {
            const auto& v = vehicleCatalog()[random() % 3]; const auto& p = v.parts[random() % v.parts.size()]; const auto& sku = shopCatalog()[random() % shopCatalog().size()];
            const auto before = serialize(s); Result r;
            const auto tool = static_cast<Tool>(random() % 5);
            switch (random() % 11) {
                case 0: r = setHood(s, v.id, random() % 2); break;
                case 1: r = setLift(s, v.id, random() % 2); break;
                case 2: r = fastener(s, v.id, p.id, tool, random() % 2); break;
                case 3: r = removePart(s, v.id, p.id, tool); break;
                case 4: r = installPart(s, v.id, p.id, s.inventory.empty() ? 0 : s.inventory.begin()->first); break;
                case 5: r = purchase(s, sku.id); break;
                case 6: r = refund(s, s.inventory.empty() ? 0 : s.inventory.begin()->first); break;
                case 7: r = setEngine(s, v.id, random() % 2); break;
                case 8: r = moveVehicle(s, v.id, static_cast<Location>(random() % 3)); break;
                case 9: r = inspect(s, v.id, p.id, tool); break;
                default: r = recoverVehicle(s, v.id); break;
            }
            const auto check = validate(s); require(check.ok, "Random operation violated invariant at iteration " + std::to_string(i) + ": " + check.message);
            if (!r.ok) require(serialize(s) == before, "Rejected randomized operation mutated state.");
        }
    });
    std::cout << "RESULT " << passed << " passed, " << failed << " failed. Core-only Linux/host checks; no Unreal/Windows gameplay validation.\n";
    return failed ? 1 : 0;
}
