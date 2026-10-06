#include "Core/GarageCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace gr {
namespace {
Result good(std::string message = "OK", std::uint64_t item = 0) { return {true, std::move(message), item}; }
Result bad(std::string message) { return {false, std::move(message), 0}; }
std::string kindId(Kind k) {
    static const std::array<const char*, 16> names{{"battery", "starter", "alternator", "belt", "tensioner", "air_filter", "oil_filter", "plugs", "radiator", "hose", "fuse", "wheel", "tire", "disc", "pads", "shock"}};
    return names.at(static_cast<std::size_t>(k));
}
std::vector<PartDef> makeParts(bool v8) {
    std::vector<PartDef> p;
    auto add = [&](std::string id, std::string label, Kind kind, int bolts, bool hood, bool lift,
                   std::vector<std::string> remove = {}, std::vector<std::string> install = {}) {
        p.push_back({std::move(id), std::move(label), kind, bolts ? Tool::Socket : Tool::Hand, bolts, hood, lift, std::move(remove), std::move(install)});
    };
    add("battery", "12 V starter battery", Kind::Battery, 3, true, false);
    add("starter", "Starter motor", Kind::Starter, 3, false, true);
    add("alternator", "Alternator", Kind::Alternator, 3, true, false, {"belt"});
    add("belt", "Accessory drive belt", Kind::Belt, 0, true, false, {}, {"alternator", "tensioner"});
    add("tensioner", "Belt tensioner", Kind::Tensioner, 2, true, false, {"belt"});
    add("air_filter", "Engine air filter", Kind::AirFilter, 2, true, false);
    add("oil_filter", "Oil filter assembly", Kind::OilFilter, 1, false, true);
    add("plugs_a", v8 ? "Spark plugs: bank A" : "Spark plug assembly", Kind::Plugs, v8 ? 4 : 6, true, false);
    if (v8) add("plugs_b", "Spark plugs: bank B", Kind::Plugs, 4, true, false);
    add("radiator", "Coolant radiator", Kind::Radiator, 4, true, false, {"hose_upper", "hose_lower"});
    add("hose_upper", "Upper coolant hose", Kind::Hose, 2, true, false, {}, {"radiator"});
    add("hose_lower", "Lower coolant hose", Kind::Hose, 2, false, true, {}, {"radiator"});
    add("fuse", "Main fuse assembly", Kind::Fuse, 0, true, false);
    for (const std::string corner : {"fl", "fr", "rl", "rr"}) {
        const std::string label = corner == "fl" ? "Front left" : corner == "fr" ? "Front right" : corner == "rl" ? "Rear left" : "Rear right";
        const std::string wheel = "wheel_" + corner, tire = "tire_" + corner, disc = "disc_" + corner, pad = "pads_" + corner, shock = "shock_" + corner;
        add(wheel, label + " wheel", Kind::Wheel, 5, false, true, {}, {tire, disc, pad, shock});
        add(tire, label + " tire", Kind::Tire, 0, false, true, {wheel});
        add(disc, label + " brake disc", Kind::Disc, 1, false, true, {wheel, pad});
        add(pad, label + " brake pad set", Kind::Pads, 2, false, true, {wheel}, {disc});
        add(shock, label + " shock absorber", Kind::Shock, 3, false, true, {wheel});
    }
    return p;
}
Vehicle* mutableVehicle(State& s, const std::string& id) {
    auto it = s.vehicles.find(id); return it == s.vehicles.end() ? nullptr : &it->second;
}
const Vehicle* getVehicle(const State& s, const std::string& id) {
    auto it = s.vehicles.find(id); return it == s.vehicles.end() ? nullptr : &it->second;
}
void changed(Vehicle& v) { ++v.revision; }
bool secured(const Vehicle& v, const PartDef& p) {
    auto it = v.parts.find(p.id);
    return it != v.parts.end() && it->second.installed && it->second.secured == p.fasteners;
}
Result access(const Vehicle& v, const PartDef& p) {
    if (v.revision >= UINT64_MAX - 1) return bad("Vehicle revision limit reached.");
    if (v.location != Location::Bay) return bad("Return to the service bay.");
    if (v.engineOn) return bad("Switch the engine off first.");
    if (p.hood && !v.hoodOpen) return bad("Open the hood first.");
    if (p.lift && !v.lifted) return bad("Raise the vehicle on the lift first.");
    for (const auto& id : p.removeFirst)
        if (v.parts.at(id).installed) return bad("Remove " + id + " first.");
    return good();
}
double health(const Vehicle& v, const std::string& id) {
    auto it = v.parts.find(id);
    return it != v.parts.end() && it->second.installed ? it->second.installed->health : 0;
}
int grade(const Vehicle& v, const std::string& id) {
    auto it = v.parts.find(id);
    if (it == v.parts.end() || !it->second.installed) return 0;
    const auto* sku = findSku(it->second.installed->sku); return sku ? sku->grade : 0;
}
bool finiteRange(double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; }
Result validateSettings(const Settings& s) {
    if (s.preset < 0 || s.preset > 4 || !finiteRange(s.fov, 60, 120) || !finiteRange(s.sensitivity, 0.1, 5) || !finiteRange(s.textScale, 0.75, 2))
        return bad("Invalid display/input settings.");
    for (double v : s.volumes) if (!finiteRange(v, 0, 1)) return bad("Invalid audio volume.");
    const Settings defaults;
    if (s.bindings.size() != defaults.bindings.size()) return bad("Incomplete controls.");
    for (const auto& [action, key] : s.bindings) {
        if (!defaults.bindings.count(action) || key.empty() || key.size() > 64 ||
            !std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
            return bad("Invalid control binding.");
    }
    // Different contexts may share keys, but opposite garage axes cannot.
    std::set<std::string> movement;
    for (const auto* action : {"forward", "back", "left", "right"})
        if (!movement.insert(s.bindings.at(action)).second) return bad("Movement bindings conflict.");
    return good();
}
}

const std::vector<VehicleDef>& vehicleCatalog() {
    static const std::vector<VehicleDef> data{
        {"hatch", "Aster H4", "Front-drive 2.0 L inline-four", 1320, 125, 260, 0.95, 8.5, makeParts(false)},
        {"coupe", "Vektor C8", "Rear-drive 4.8 L V8", 1680, 330, 560, 1.08, 9.8, makeParts(true)},
        {"suv", "Terra S6", "All-wheel-drive 3.0 L inline-six", 2180, 210, 440, 0.85, 8.0, makeParts(false)}};
    return data;
}
const VehicleDef* findVehicle(const std::string& id) {
    for (const auto& v : vehicleCatalog()) if (v.id == id) return &v;
    return nullptr;
}
const PartDef* findPart(const std::string& vehicle, const std::string& id) {
    const auto* v = findVehicle(vehicle); if (!v) return nullptr;
    for (const auto& p : v->parts) if (p.id == id) return &p;
    return nullptr;
}
std::string standardSku(const std::string& vehicle, Kind k) { return vehicle + ":" + kindId(k) + ":standard"; }
const std::vector<Sku>& shopCatalog() {
    static const std::vector<Sku> catalog = [] {
        std::vector<Sku> result;
        const std::array<std::int64_t, 16> prices{{11000, 13500, 18000, 2400, 4500, 1900, 1400, 5200, 18000, 2200, 900, 18000, 8500, 7500, 4500, 9800}};
        for (const auto& v : vehicleCatalog()) {
            std::set<Kind> added;
            for (const auto& p : v.parts) if (added.insert(p.kind).second) {
                const auto price = prices.at(static_cast<std::size_t>(p.kind));
                result.push_back({standardSku(v.id, p.kind), v.name + " " + kindId(p.kind), v.id, p.kind, 0, price, 8});
                if (p.kind == Kind::Tire || p.kind == Kind::Pads || p.kind == Kind::Disc || p.kind == Kind::Shock || p.kind == Kind::AirFilter)
                    result.push_back({v.id + ":" + kindId(p.kind) + ":sport", v.name + " performance " + kindId(p.kind), v.id, p.kind, 1, price * 3 / 2, 8});
            }
        }
        return result;
    }();
    return catalog;
}
const Sku* findSku(const std::string& id) {
    for (const auto& p : shopCatalog()) if (p.id == id) return &p;
    return nullptr;
}
const std::vector<OrderDef>& orderCatalog() {
    static const std::vector<OrderDef> orders{
        {"01-battery", "A slow morning", "Starter turns slowly; the battery test is weak.", "hatch", {{"battery", .2}}, Test::Start, 26000, 10},
        {"02-charge", "Charge warning", "Battery warning stays on; inspect charging output and drive belt.", "suv", {{"alternator", .25}, {"belt", .3}}, Test::Charge, 46000, 12},
        {"03-misfire", "Uneven idle", "The coupe shakes at idle; both plug banks show ignition wear.", "coupe", {{"plugs_a", .2}, {"plugs_b", .3}}, Test::Idle, 35000, 12},
        {"04-intake", "Breathing room", "Hatchback feels sluggish; intake restriction is high.", "hatch", {{"air_filter", .15}}, Test::Intake, 21000, 10},
        {"05-cooling", "Running hot", "SUV temperature rises under load; radiator and upper hose need inspection.", "suv", {{"radiator", .3}, {"hose_upper", .25}}, Test::Cooling, 52000, 15},
        {"06-brakes", "Brake vibration", "Coupe braking is noisy with pedal vibration; check the front-left assembly.", "coupe", {{"pads_fl", .2}, {"disc_fl", .25}}, Test::Braking, 42000, 15},
        {"07-tires", "Grip restored", "The hatchback slides early; all four tires have insufficient tread.", "hatch", {{"tire_fl", .2}, {"tire_fr", .2}, {"tire_rl", .3}, {"tire_rr", .3}}, Test::Grip, 61000, 15},
        {"08-shock", "Settle down", "SUV bounces after corners; front-right damper is damaged.", "suv", {{"shock_fr", .2}}, Test::Handling, 35000, 15}};
    return orders;
}
const OrderDef* findOrder(const std::string& id) {
    for (const auto& j : orderCatalog()) if (j.id == id) return &j;
    return nullptr;
}
State newGame(Mode mode) {
    State s; s.mode = mode;
    for (const auto& definition : vehicleCatalog()) {
        Vehicle v; v.id = definition.id;
        for (const auto& part : definition.parts) {
            Item item{s.nextSerial++, standardSku(v.id, part.kind), 1, false, 0};
            v.parts.emplace(part.id, Slot{item, part.fasteners});
        }
        s.vehicles.emplace(v.id, std::move(v));
    }
    for (const auto& sku : shopCatalog()) s.stock[sku.id] = sku.initialStock;
    for (const auto& job : orderCatalog()) s.completions[job.id] = 0;
    return s;
}
bool driveReady(const State& s, const std::string& id) {
    const auto* v = getVehicle(s, id); const auto* def = findVehicle(id);
    if (!v || !def) return false;
    return std::all_of(def->parts.begin(), def->parts.end(), [&](const PartDef& p) { return secured(*v, p); });
}
Measurements diagnose(const State& s, const std::string& id) {
    Measurements m; const auto* v = getVehicle(s, id); const auto* def = findVehicle(id);
    if (!v || !def) return m;
    m.assembled = driveReady(s, id);
    const double battery = health(*v, "battery"), starter = health(*v, "starter");
    const double charging = std::min({health(*v, "alternator"), health(*v, "belt"), health(*v, "tensioner")});
    m.batteryVolts = 11 + 1.7 * battery;
    m.chargingVolts = 10.8 + 3.6 * charging;
    m.crankSeconds = .6 + 4 * (1 - battery) + 2 * (1 - starter);
    m.starts = m.assembled && battery >= .35 && starter >= .35 && health(*v, "fuse") >= .4;
    double plugs = health(*v, "plugs_a");
    if (id == "coupe") plugs = std::min(plugs, health(*v, "plugs_b"));
    m.misfirePercent = (1 - plugs) * 35;
    m.intakePercent = 40 + 60 * health(*v, "air_filter");
    m.coolantC = 88 + 48 * (1 - std::min({health(*v, "radiator"), health(*v, "hose_upper"), health(*v, "hose_lower")}));
    double tires = 1, brakes = 1, shocks = 1, tireUpgrade = 0, brakeUpgrade = 0, shockUpgrade = 0;
    for (const std::string c : {"fl", "fr", "rl", "rr"}) {
        tires = std::min(tires, health(*v, "tire_" + c));
        brakes = std::min({brakes, health(*v, "pads_" + c), health(*v, "disc_" + c)});
        shocks = std::min(shocks, health(*v, "shock_" + c));
        tireUpgrade += grade(*v, "tire_" + c) * .025;
        brakeUpgrade += (grade(*v, "pads_" + c) + grade(*v, "disc_" + c)) * .0125;
        shockUpgrade += grade(*v, "shock_" + c) * .025;
    }
    m.massKg = def->massKg;
    m.powerKw = def->powerKw * (.65 + .35 * health(*v, "air_filter")) * (.5 + .5 * plugs) * (1 + .04 * grade(*v, "air_filter"));
    m.torqueNm = def->torqueNm * (m.powerKw / def->powerKw);
    m.grip = def->grip * (.35 + .65 * tires) * (1 + tireUpgrade);
    m.damping = (.3 + .7 * shocks) * (1 + shockUpgrade);
    m.brakeDecel = std::min(def->brakeDecel * (.3 + .7 * brakes) * (1 + brakeUpgrade), m.grip * 9.81);
    // Reference estimates, NOT recorded track runs. Real measured telemetry must
    // be obtained from the Chaos pawn once content and driving are integrated.
    m.zeroTo100 = m.massKg * (27.7778 * 27.7778) / (2 * std::max(1.0, m.powerKw * 1000 * .7));
    m.zeroTo100 = std::max(m.zeroTo100, 27.7778 / std::max(.1, m.grip * 9.81));
    m.brake100Meters = (27.7778 * 27.7778) / (2 * std::max(.1, m.brakeDecel));
    if (!m.assembled) m.faults.push_back("Incomplete or unsecured assembly");
    if (battery < .75) m.faults.push_back("Weak battery");
    if (charging < .75) m.faults.push_back("Charging circuit output low");
    if (plugs < .75) m.faults.push_back("Ignition misfire");
    if (health(*v, "air_filter") < .75) m.faults.push_back("Intake restriction");
    if (m.coolantC > 105) m.faults.push_back("Cooling efficiency low");
    if (brakes < .75) m.faults.push_back("Brake wear");
    if (tires < .75) m.faults.push_back("Low tire grip");
    if (shocks < .75) m.faults.push_back("Low suspension damping");
    return m;
}
Result acceptOrder(State& s, const std::string& id) {
    if (s.mode != Mode::Career) return bad("Customer orders are available in Career mode.");
    if (s.job) return bad("Finish the active customer order first.");
    const auto* order = findOrder(id); if (!order) return bad("Unknown order.");
    auto* v = mutableVehicle(s, order->vehicle);
    if (!v || v->location != Location::Bay || v->lifted || v->engineOn || !driveReady(s, v->id)) return bad("Return a fully assembled vehicle to the service bay first.");
    if (s.nextJob >= UINT64_MAX - 1 || v->revision >= UINT64_MAX - 1) return bad("Order/revision limit reached.");
    for (const auto& f : order->faults) v->parts.at(f.part).installed->health = f.health;
    changed(*v);
    s.job = Job{id, s.nextJob++, 0, {}};
    s.selectedVehicle = v->id;
    return good(order->customerSymptoms);
}
Result inspect(State& s, const std::string& id, const std::string& partId, Tool tool) {
    auto* v = mutableVehicle(s, id); const auto* p = findPart(id, partId);
    if (!v || !p) return bad("Unknown vehicle or component.");
    // Electrical diagnosis can happen with covers intact; direct part work still
    // enforces access independently. Inspection never repairs or creates an item.
    if (v->location != Location::Bay || v->engineOn) return bad("Inspect at the bay with the engine off.");
    if (p->kind == Kind::Battery || p->kind == Kind::Alternator || p->kind == Kind::Fuse) {
        if (tool != Tool::Multimeter) return bad("Use the multimeter for this electrical component.");
    } else if (tool != Tool::Scanner && tool != Tool::Hand) return bad("Use visual inspection or the diagnostic scanner.");
    if (!v->parts.at(partId).installed) return bad("The component is removed.");
    if (s.job) {
        const auto* j = findOrder(s.job->order);
        if (j && j->vehicle == id && std::any_of(j->faults.begin(), j->faults.end(), [&](const Fault& f) { return f.part == partId; }) &&
            std::find(s.job->inspected.begin(), s.job->inspected.end(), partId) == s.job->inspected.end()) s.job->inspected.push_back(partId);
    }
    return good(p->label + ": " + std::to_string(static_cast<int>(health(*v, partId) * 100)) + "% condition");
}
Result purchase(State& s, const std::string& id) {
    const auto* sku = findSku(id); if (!sku) return bad("Unknown shop item.");
    if (s.nextSerial >= UINT64_MAX - 1) return bad("Inventory serial limit reached.");
    if (s.inventory.size() >= 512) return bad("Inventory is full.");
    if (s.mode == Mode::Career && s.stock.at(id) <= 0) return bad("This part is out of stock.");
    if (s.mode == Mode::Career && s.money < sku->price) return bad("Insufficient funds.");
    const bool career = s.mode == Mode::Career;
    const auto serial = s.nextSerial;
    s.inventory.emplace(serial, Item{serial, id, 1, career, career ? sku->price : 0});
    ++s.nextSerial;
    if (career) { s.money -= sku->price; --s.stock.at(id); }
    return good("Part purchased.", serial);
}
Result refund(State& s, std::uint64_t serial) {
    const auto it = s.inventory.find(serial);
    if (it == s.inventory.end()) return bad("Inventory item not found.");
    if (s.mode != Mode::Career || !it->second.refundable || it->second.health != 1 || it->second.paid <= 0) return bad("Only unused purchased parts can be refunded.");
    if (s.money > 1000000000000LL - it->second.paid) return bad("Money limit reached.");
    s.money += it->second.paid;
    s.stock.at(it->second.sku) = std::min(100, s.stock.at(it->second.sku) + 1);
    s.inventory.erase(it);
    return good("Purchase refunded.");
}
Result setHood(State& s, const std::string& id, bool open) {
    auto* v = mutableVehicle(s, id); if (!v) return bad("Unknown vehicle.");
    if (v->location != Location::Bay || v->engineOn) return bad("Operate the hood at the bay with the engine off.");
    v->hoodOpen = open; return good(open ? "Hood opened." : "Hood closed.");
}
Result setLift(State& s, const std::string& id, bool up) {
    auto* v = mutableVehicle(s, id); if (!v) return bad("Unknown vehicle.");
    if (v->location != Location::Bay || v->engineOn) return bad("Park in the service bay and switch off the engine.");
    if (up && !v->lifted && !driveReady(s, id)) return bad("Assemble and secure the vehicle before raising the lift.");
    if (!up) {
        for (const auto& p : findVehicle(id)->parts)
            if ((p.kind == Kind::Wheel || p.kind == Kind::Shock || p.kind == Kind::Tire) && !secured(*v, p)) return bad("Restore and secure wheels and suspension before lowering.");
    }
    v->lifted = up; return good(up ? "Lift raised." : "Lift lowered.");
}
Result fastener(State& s, const std::string& id, const std::string& partId, Tool tool, bool tighten) {
    auto* v = mutableVehicle(s, id); const auto* p = findPart(id, partId);
    if (!v || !p) return bad("Unknown vehicle or component.");
    if (auto r = access(*v, *p); !r) return r;
    auto& slot = v->parts.at(partId);
    if (!slot.installed || p->fasteners == 0) return bad("No installed fastener operation is available.");
    if (tighten ? tool != Tool::TorqueWrench : tool != p->tool) return bad(tighten ? "Use the torque wrench." : "Use the socket tool.");
    if (tighten && slot.secured == p->fasteners) return bad("All fasteners are already secured.");
    if (!tighten && slot.secured == 0) return bad("All fasteners are already released.");
    slot.secured += tighten ? 1 : -1; changed(*v); return good(tighten ? "Fastener torqued." : "Fastener released.");
}
Result removePart(State& s, const std::string& id, const std::string& partId, Tool tool) {
    auto* v = mutableVehicle(s, id); const auto* p = findPart(id, partId);
    if (!v || !p) return bad("Unknown vehicle or component.");
    if (auto r = access(*v, *p); !r) return r;
    auto& slot = v->parts.at(partId);
    if (tool != p->tool) return bad("Incorrect removal tool.");
    if (!slot.installed) return bad("This part is already removed.");
    if (slot.secured) return bad("Release all fasteners first.");
    if (s.inventory.size() >= 512) return bad("Inventory is full.");
    const Item item = *slot.installed;
    if (s.inventory.count(item.serial)) return bad("Duplicate item detected; no state was changed.");
    s.inventory.emplace(item.serial, item);
    slot.installed.reset(); changed(*v); return good("Part moved to inventory.", item.serial);
}
Result installPart(State& s, const std::string& id, const std::string& partId, std::uint64_t serial) {
    auto* v = mutableVehicle(s, id); const auto* p = findPart(id, partId);
    if (!v || !p) return bad("Unknown vehicle or component.");
    if (auto r = access(*v, *p); !r) return r;
    auto& slot = v->parts.at(partId);
    if (slot.installed) return bad("Remove the existing part first.");
    auto it = s.inventory.find(serial); if (it == s.inventory.end()) return bad("Inventory item not found.");
    const auto* sku = findSku(it->second.sku);
    if (!sku || sku->vehicle != id || sku->kind != p->kind) return bad("This component does not fit this vehicle/slot.");
    for (const auto& dependency : p->installFirst)
        if (!secured(*v, *findPart(id, dependency))) return bad("Install and secure " + dependency + " first.");
    Item item = it->second; item.refundable = false;
    slot.installed = item; slot.secured = 0; s.inventory.erase(it); changed(*v);
    return good(p->fasteners ? "Part installed; torque its fasteners." : "Part installed.");
}
Result moveVehicle(State& s, const std::string& id, Location destination) {
    auto* v = mutableVehicle(s, id); if (!v) return bad("Unknown vehicle.");
    if (static_cast<int>(destination) > 2) return bad("Unknown location.");
    if (v->lifted || v->hoodOpen || !driveReady(s, id)) return bad("Lower the lift, close the hood, and secure every part before moving.");
    v->location = destination;
    s.playerLocation = destination;
    s.selectedVehicle = id;
    return good("Vehicle location updated without resetting mechanical state.");
}
Result setEngine(State& s, const std::string& id, bool on) {
    auto* v = mutableVehicle(s, id); if (!v) return bad("Unknown vehicle.");
    if (on && (v->lifted || v->hoodOpen || !diagnose(s, id).starts)) return bad("Starting requires a safe, assembled vehicle and sufficient battery condition.");
    v->engineOn = on; return good(on ? "Engine started." : "Engine stopped.");
}
Result recoverVehicle(State& s, const std::string& id) {
    auto* v = mutableVehicle(s, id); if (!v) return bad("Unknown vehicle.");
    if (v->location != Location::Track) return bad("Recovery is available on the test track.");
    v->engineOn = false; s.playerPosition = {{0, 0, 0}};
    return good("Track recovery requested; component state preserved.");
}
Result runVerification(State& s) {
    if (!s.job) return bad("No active order.");
    const auto* j = findOrder(s.job->order); auto* v = mutableVehicle(s, j->vehicle);
    const bool track = j->test == Test::Braking || j->test == Test::Grip || j->test == Test::Handling;
    if ((track && v->location != Location::Track) || (!track && v->location != Location::Bay)) return bad(track ? "Run this verification at the closed test facility." : "Run this verification at the diagnostic bay.");
    if (v->lifted || v->hoodOpen || !driveReady(s, v->id)) return bad("Prepare a fully secured, lowered vehicle with its hood closed.");
    for (const auto& f : j->faults) {
        if (std::find(s.job->inspected.begin(), s.job->inspected.end(), f.part) == s.job->inspected.end()) return bad("Inspect every reported fault before verification.");
        if (health(*v, f.part) < .85) return bad("A reported component is still below the acceptance condition.");
    }
    const auto m = diagnose(s, v->id);
    bool passes = false;
    switch (j->test) {
        case Test::Start: passes = m.starts && m.crankSeconds < 1.5; break;
        case Test::Charge: passes = m.starts && m.chargingVolts >= 13.5; break;
        case Test::Idle: passes = m.starts && m.misfirePercent < 5; break;
        case Test::Intake: passes = m.starts && m.intakePercent >= 90; break;
        case Test::Cooling: passes = m.starts && m.coolantC <= 98; break;
        case Test::Braking: passes = m.starts && m.brakeDecel >= findVehicle(v->id)->brakeDecel * .9; break;
        case Test::Grip: passes = m.starts && m.grip >= findVehicle(v->id)->grip * .9; break;
        case Test::Handling: passes = m.starts && m.damping >= .9; break;
    }
    if (!passes) return bad("The state-derived acceptance measurement is still outside its target.");
    s.job->verifiedRevision = v->revision;
    return good(track ? "Core acceptance passed. Real track telemetry integration is pending." : "State-derived diagnostic acceptance passed.");
}
Result completeOrder(State& s) {
    if (!s.job) return bad("No active order to reward.");
    const auto* j = findOrder(s.job->order); const auto* v = getVehicle(s, j->vehicle);
    if (v->location != Location::Bay || v->engineOn || v->lifted || v->hoodOpen || !driveReady(s, v->id)) return bad("Return the safely assembled vehicle to the bay and stop its engine.");
    if (!s.job->verifiedRevision || s.job->verifiedRevision != v->revision) return bad("Verify this mechanical revision before collecting payment.");
    if (s.money > 1000000000000LL - j->payment || s.reputation > 1000000 - j->reputation || s.completions.at(j->id) == std::numeric_limits<std::uint32_t>::max()) return bad("Progression limit reached.");
    s.money += j->payment; s.reputation += j->reputation; ++s.completions.at(j->id);
    // A supplier replenishes the catalog after a completed order, allowing repeat
    // jobs without an unbounded purchase/refund loop or unlimited paid inventory.
    for (const auto& sku : shopCatalog()) s.stock.at(sku.id) = std::max(s.stock.at(sku.id), sku.initialStock);
    const auto summary = j->title + " complete. Paid " + std::to_string(j->payment / 100) + "; reputation +" + std::to_string(j->reputation) + ".";
    s.job.reset(); return good(summary);
}
Result updateSettings(State& s, const Settings& settings) {
    if (auto r = validateSettings(settings); !r) return r;
    s.settings = settings; return good("Settings updated.");
}

Result validate(const State& s) {
    if (static_cast<int>(s.mode) > 1 || static_cast<int>(s.playerLocation) > 2 ||
        s.money < 0 || s.money > 1000000000000LL || s.reputation < 0 || s.reputation > 1000000 ||
        !s.nextSerial || !s.nextJob || s.nextSerial == UINT64_MAX || s.nextJob == UINT64_MAX)
        return bad("Invalid session counters or economy.");
    if (s.vehicles.size() != vehicleCatalog().size() || !s.vehicles.count(s.selectedVehicle) || s.inventory.size() > 512)
        return bad("Invalid vehicle roster or inventory size.");
    if (auto r = validateSettings(s.settings); !r) return r;
    for (double p : s.playerPosition) if (!finiteRange(p, -10000000, 10000000)) return bad("Invalid saved player position.");
    std::set<std::uint64_t> serials;
    auto itemValid = [&](const Item& i, const std::string* vehicle, const Kind* kind) -> bool {
        const auto* sku = findSku(i.sku);
        if (!sku || !i.serial || i.serial >= s.nextSerial || !serials.insert(i.serial).second || !finiteRange(i.health, 0, 1) ||
            i.paid < 0 || (i.paid != 0 && i.paid != sku->price)) return false;
        if (vehicle && (sku->vehicle != *vehicle || sku->kind != *kind || i.refundable)) return false;
        if (i.refundable && (i.health != 1 || i.paid != sku->price || s.mode != Mode::Career)) return false;
        return true;
    };
    for (const auto& d : vehicleCatalog()) {
        const auto* v = getVehicle(s, d.id);
        if (!v || v->id != d.id || v->parts.size() != d.parts.size() || !v->revision || v->revision == UINT64_MAX || static_cast<int>(v->location) > 2)
            return bad("Invalid vehicle/part state.");
        if ((v->lifted || v->hoodOpen) && (v->location != Location::Bay || v->engineOn)) return bad("Invalid lift/hood state.");
        for (const auto& p : d.parts) {
            const auto it = v->parts.find(p.id);
            if (it == v->parts.end() || it->second.secured < 0 || it->second.secured > p.fasteners) return bad("Unknown part or invalid fastener state.");
            const auto& slot = it->second;
            if (!slot.installed && slot.secured != 0) return bad("A removed part has secured fasteners.");
            if (slot.installed && !itemValid(*slot.installed, &d.id, &p.kind)) return bad("Invalid or duplicate installed item.");
            if (slot.installed) for (const auto& dependency : p.installFirst)
                if (!secured(*v, *findPart(d.id, dependency))) return bad("Installed assembly has a missing/unsecured prerequisite.");
            if (!v->lifted && (p.kind == Kind::Wheel || p.kind == Kind::Tire || p.kind == Kind::Shock) && !secured(*v, p))
                return bad("Wheel/suspension work requires the lift to remain raised.");
        }
        if (v->engineOn && !diagnose(s, d.id).starts) return bad("Engine is running on an unsafe/unstartable vehicle.");
    }
    for (const auto& [serial, item] : s.inventory)
        if (serial != item.serial || !itemValid(item, nullptr, nullptr)) return bad("Invalid or duplicate inventory item.");
    if (s.stock.size() != shopCatalog().size()) return bad("Incomplete shop stock.");
    for (const auto& sku : shopCatalog()) {
        const auto it = s.stock.find(sku.id);
        if (it == s.stock.end() || it->second < 0 || it->second > 100) return bad("Invalid shop stock.");
    }
    if (s.completions.size() != orderCatalog().size()) return bad("Invalid progression data.");
    std::uint64_t earnedReputation = 0;
    for (const auto& order : orderCatalog()) {
        const auto it = s.completions.find(order.id);
        if (it == s.completions.end()) return bad("Unknown order completion.");
        earnedReputation += static_cast<std::uint64_t>(it->second) * order.reputation;
    }
    if (earnedReputation != static_cast<std::uint64_t>(s.reputation)) return bad("Reputation does not match completed orders.");
    if (s.mode == Mode::Sandbox && (s.job || s.reputation)) return bad("Sandbox state contains career rewards.");
    if (s.job) {
        const auto* order = findOrder(s.job->order);
        if (!order || !s.job->instance || s.job->instance >= s.nextJob || s.job->verifiedRevision > s.vehicles.at(order->vehicle).revision ||
            s.job->inspected.size() > order->faults.size()) return bad("Invalid active order.");
        std::set<std::string> evidence;
        for (const auto& part : s.job->inspected)
            if (!evidence.insert(part).second || std::none_of(order->faults.begin(), order->faults.end(), [&](const Fault& f) { return f.part == part; })) return bad("Invalid diagnostic evidence.");
        if (s.job->verifiedRevision && s.job->verifiedRevision == s.vehicles.at(order->vehicle).revision) {
            if (!driveReady(s, order->vehicle)) return bad("Verified order contains an incomplete assembly.");
            for (const auto& f : order->faults)
                if (!evidence.count(f.part) || health(s.vehicles.at(order->vehicle), f.part) < .85) return bad("Verified order has unresolved/uninspected faults.");
        }
    }
    return good();
}

namespace {
constexpr std::size_t MaxSave = 2 * 1024 * 1024;
constexpr std::uint32_t Schema = 1;
struct Writer {
    std::vector<std::uint8_t> bytes;
    void integer(std::uint64_t v, int n) { for (int i = 0; i < n; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (i * 8))); }
    void boolean(bool v) { integer(v ? 1 : 0, 1); }
    void number(double d) { std::uint64_t bits = 0; static_assert(sizeof(bits) == sizeof(d)); std::memcpy(&bits, &d, sizeof(d)); integer(bits, 8); }
    void string(const std::string& s) {
        if (s.size() > 256) throw std::runtime_error("Save string exceeds schema limit.");
        integer(s.size(), 4); bytes.insert(bytes.end(), s.begin(), s.end());
    }
    void item(const Item& i) { integer(i.serial, 8); string(i.sku); number(i.health); boolean(i.refundable); integer(static_cast<std::uint64_t>(i.paid), 8); }
};
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t pos = 0;
    std::uint64_t integer(int n) {
        if (pos + static_cast<std::size_t>(n) > bytes.size()) throw std::runtime_error("Truncated save.");
        std::uint64_t v = 0;
        for (int i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(bytes[pos++]) << (i * 8);
        return v;
    }
    std::uint32_t count(std::uint32_t max) {
        const auto v = integer(4); if (v > max) throw std::runtime_error("Save collection exceeds schema limit.");
        return static_cast<std::uint32_t>(v);
    }
    bool boolean() { const auto v = integer(1); if (v > 1) throw std::runtime_error("Invalid boolean."); return v == 1; }
    double number() { const auto bits = integer(8); double d = 0; std::memcpy(&d, &bits, sizeof(d)); return d; }
    std::string string() {
        const auto size = count(256); if (pos + size > bytes.size()) throw std::runtime_error("Truncated string.");
        std::string s(bytes.begin() + static_cast<std::ptrdiff_t>(pos), bytes.begin() + static_cast<std::ptrdiff_t>(pos + size)); pos += size; return s;
    }
    Item item() {
        Item i; i.serial = integer(8); i.sku = string(); i.health = number(); i.refundable = boolean();
        const auto paid = integer(8);
        if (paid > static_cast<std::uint64_t>(INT64_MAX)) throw std::runtime_error("Invalid item payment.");
        i.paid = static_cast<std::int64_t>(paid); return i;
    }
};
std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::uint8_t b : bytes) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return ~crc;
}
std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Save file cannot be opened.");
    const auto size = stream.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(MaxSave)) throw std::runtime_error("Save file has an invalid size.");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size)); stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Save read failed.");
    return bytes;
}
void syncFile(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Unable to open temporary save for durable flush.");
    const BOOL ok = FlushFileBuffers(handle); CloseHandle(handle);
    if (!ok) throw std::runtime_error("Temporary save flush failed.");
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("Unable to open temporary save for durable flush.");
    const int result = ::fsync(fd); ::close(fd);
    if (result != 0) throw std::runtime_error("Temporary save flush failed.");
#endif
}
void atomicReplace(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Atomic save replacement failed.");
#else
    std::filesystem::rename(from, to);
#endif
}
}

std::vector<std::uint8_t> serialize(const State& s) {
    if (auto result = validate(s); !result) throw std::runtime_error(result.message);
    Writer w;
    w.integer(static_cast<std::uint8_t>(s.mode), 1); w.integer(static_cast<std::uint64_t>(s.money), 8);
    w.integer(static_cast<std::uint32_t>(s.reputation), 4); w.integer(s.nextSerial, 8); w.integer(s.nextJob, 8);
    w.integer(s.vehicles.size(), 4);
    for (const auto& [id, v] : s.vehicles) {
        w.string(id); w.integer(v.revision, 8); w.integer(static_cast<std::uint8_t>(v.location), 1);
        w.boolean(v.hoodOpen); w.boolean(v.lifted); w.boolean(v.engineOn); w.integer(v.parts.size(), 4);
        for (const auto& [part, slot] : v.parts) {
            w.string(part); w.boolean(slot.installed.has_value()); if (slot.installed) w.item(*slot.installed); w.integer(static_cast<std::uint32_t>(slot.secured), 4);
        }
    }
    w.integer(s.inventory.size(), 4); for (const auto& [serial, item] : s.inventory) { (void)serial; w.item(item); }
    w.integer(s.stock.size(), 4); for (const auto& [id, stock] : s.stock) { w.string(id); w.integer(static_cast<std::uint32_t>(stock), 4); }
    w.integer(s.completions.size(), 4); for (const auto& [id, count] : s.completions) { w.string(id); w.integer(count, 4); }
    w.boolean(s.job.has_value());
    if (s.job) {
        w.string(s.job->order); w.integer(s.job->instance, 8); w.integer(s.job->verifiedRevision, 8);
        w.integer(s.job->inspected.size(), 4); for (const auto& id : s.job->inspected) w.string(id);
    }
    const auto& p = s.settings;
    w.integer(static_cast<std::uint32_t>(p.preset), 4); w.number(p.fov); w.number(p.sensitivity); w.number(p.textScale);
    w.boolean(p.invertLook); w.boolean(p.assisted); w.boolean(p.motionBlur); w.boolean(p.filmGrain); w.boolean(p.depthOfField); w.boolean(p.cameraShake);
    for (double v : p.volumes) w.number(v);
    w.integer(p.bindings.size(), 4); for (const auto& [action, key] : p.bindings) { w.string(action); w.string(key); }
    w.string(s.selectedVehicle); w.integer(static_cast<std::uint8_t>(s.playerLocation), 1); for (double v : s.playerPosition) w.number(v);
    Writer out; for (char c : std::string("GRSAVE01")) out.integer(static_cast<std::uint8_t>(c), 1);
    out.integer(Schema, 4); out.integer(w.bytes.size(), 4); out.bytes.insert(out.bytes.end(), w.bytes.begin(), w.bytes.end()); out.integer(crc32(w.bytes), 4);
    return out.bytes;
}
Result deserialize(const std::vector<std::uint8_t>& bytes, State& destination) {
    try {
        if (bytes.size() < 20 || bytes.size() > MaxSave) return bad("Save has an invalid size.");
        Reader header{bytes}; std::string magic; for (int i = 0; i < 8; ++i) magic.push_back(static_cast<char>(header.integer(1)));
        if (magic != "GRSAVE01") return bad("Unknown save format.");
        if (header.integer(4) != Schema) return bad("Unsupported save schema. No automatic migration is implemented.");
        const auto size = header.integer(4);
        if (size != bytes.size() - 20) return bad("Save payload length mismatch.");
        std::vector<std::uint8_t> payload(bytes.begin() + 16, bytes.end() - 4);
        header.pos = bytes.size() - 4;
        if (header.integer(4) != crc32(payload)) return bad("Save checksum failed.");
        Reader r{payload}; State s;
        s.mode = static_cast<Mode>(r.integer(1));
        const auto money = r.integer(8); if (money > static_cast<std::uint64_t>(INT64_MAX)) return bad("Invalid saved money.");
        s.money = static_cast<std::int64_t>(money);
        s.reputation = static_cast<int>(r.count(1000000)); s.nextSerial = r.integer(8); s.nextJob = r.integer(8);
        const auto vehicles = r.count(3);
        for (std::uint32_t n = 0; n < vehicles; ++n) {
            Vehicle v; v.id = r.string(); v.revision = r.integer(8); v.location = static_cast<Location>(r.integer(1));
            v.hoodOpen = r.boolean(); v.lifted = r.boolean(); v.engineOn = r.boolean();
            const auto parts = r.count(64);
            for (std::uint32_t k = 0; k < parts; ++k) {
                auto id = r.string(); Slot slot;
                if (r.boolean()) slot.installed = r.item();
                slot.secured = static_cast<int>(r.count(32));
                if (!v.parts.emplace(id, slot).second) return bad("Duplicate part in save.");
            }
            const auto id = v.id; if (!s.vehicles.emplace(id, std::move(v)).second) return bad("Duplicate vehicle in save.");
        }
        const auto items = r.count(512);
        for (std::uint32_t n = 0; n < items; ++n) { auto item = r.item(); if (!s.inventory.emplace(item.serial, item).second) return bad("Duplicate inventory serial in save."); }
        s.stock.clear(); const auto skus = r.count(128);
        for (std::uint32_t n = 0; n < skus; ++n) {
            auto id = r.string(); const auto stock = static_cast<int>(r.count(100)); if (!s.stock.emplace(id, stock).second) return bad("Duplicate stock entry in save.");
        }
        s.completions.clear(); const auto orders = r.count(8);
        for (std::uint32_t n = 0; n < orders; ++n) {
            auto id = r.string(); const auto count = static_cast<std::uint32_t>(r.integer(4)); if (!s.completions.emplace(id, count).second) return bad("Duplicate order count in save.");
        }
        if (r.boolean()) {
            Job j; j.order = r.string(); j.instance = r.integer(8); j.verifiedRevision = r.integer(8);
            const auto evidence = r.count(8); for (std::uint32_t n = 0; n < evidence; ++n) j.inspected.push_back(r.string()); s.job = std::move(j);
        }
        auto& p = s.settings;
        p.preset = static_cast<int>(r.count(4)); p.fov = r.number(); p.sensitivity = r.number(); p.textScale = r.number();
        p.invertLook = r.boolean(); p.assisted = r.boolean(); p.motionBlur = r.boolean(); p.filmGrain = r.boolean(); p.depthOfField = r.boolean(); p.cameraShake = r.boolean();
        for (double& volume : p.volumes) volume = r.number();
        p.bindings.clear(); const auto bindings = r.count(16);
        for (std::uint32_t n = 0; n < bindings; ++n) {
            auto action = r.string(); auto key = r.string(); if (!p.bindings.emplace(action, key).second) return bad("Duplicate control in save.");
        }
        s.selectedVehicle = r.string(); s.playerLocation = static_cast<Location>(r.integer(1)); for (double& v : s.playerPosition) v = r.number();
        if (r.pos != payload.size()) return bad("Unexpected trailing save data.");
        if (auto check = validate(s); !check) return check;
        destination = std::move(s); return good("Save loaded.");
    } catch (const std::exception& e) { return bad(e.what()); }
}
Result save(const State& s, const std::filesystem::path& path) {
    auto temporary = path; temporary += ".tmp";
    auto backup = path; backup += ".bak";
    try {
        const auto bytes = serialize(s);
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream || !stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())) || !stream.flush()) return bad("Writing the temporary save failed.");
        }
        syncFile(temporary);
        if (std::filesystem::exists(path)) {
            State previous;
            // Never replace a valid backup with a corrupted primary.
            bool validPrimary = false;
            try { validPrimary = static_cast<bool>(deserialize(readFile(path), previous)); }
            catch (const std::exception&) { /* Keep the existing backup. */ }
            if (validPrimary) {
                auto backupTemp = backup; backupTemp += ".tmp";
                std::filesystem::copy_file(path, backupTemp, std::filesystem::copy_options::overwrite_existing);
                syncFile(backupTemp); atomicReplace(backupTemp, backup);
            }
        }
        atomicReplace(temporary, path); return good("Save written with recovery backup.");
    } catch (const std::exception& e) {
        std::error_code ignored; std::filesystem::remove(temporary, ignored); return bad(std::string("Save failed: ") + e.what());
    }
}
LoadResult load(State& s, const std::filesystem::path& path) {
    std::string primaryError;
    try {
        auto result = deserialize(readFile(path), s);
        if (result) return {result, false};
        primaryError = result.message;
    } catch (const std::exception& e) { primaryError = e.what(); }
    auto backup = path; backup += ".bak";
    try {
        auto result = deserialize(readFile(backup), s);
        if (result) return {good("Primary save unavailable (" + primaryError + "); recovered last valid backup."), true};
        return {bad("Primary: " + primaryError + "; backup: " + result.message), false};
    } catch (const std::exception& e) { return {bad("Primary: " + primaryError + "; backup: " + e.what()), false}; }
}
}
