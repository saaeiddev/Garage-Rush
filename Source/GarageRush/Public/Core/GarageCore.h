#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Engine-independent gameplay state. No rendering, traffic or Chaos physics is
// approximated here: derived measurements are intentionally simplified game data.
namespace gr {
enum class Kind : std::uint8_t {
    Battery, Starter, Alternator, Belt, Tensioner, AirFilter, OilFilter,
    Plugs, Radiator, Hose, Fuse, Wheel, Tire, Disc, Pads, Shock
};
enum class Tool : std::uint8_t { Hand, Socket, TorqueWrench, Multimeter, Scanner };
enum class Location : std::uint8_t { Bay, Yard, Track };
enum class Test : std::uint8_t { Start, Charge, Idle, Intake, Cooling, Braking, Grip, Handling };
enum class Mode : std::uint8_t { Career, Sandbox };

struct Result {
    bool ok = false;
    std::string message;
    std::uint64_t item = 0;
    explicit operator bool() const { return ok; }
};
struct PartDef {
    std::string id, label;
    Kind kind{};
    Tool tool{};
    int fasteners = 0;
    bool hood = false, lift = false;
    std::vector<std::string> removeFirst, installFirst;
};
struct VehicleDef {
    std::string id, name, architecture;
    double massKg = 0, powerKw = 0, torqueNm = 0, grip = 0, brakeDecel = 0;
    std::vector<PartDef> parts;
};
struct Sku {
    std::string id, label, vehicle;
    Kind kind{};
    int grade = 0;
    std::int64_t price = 0; // Integer cents; never a floating-point currency.
    int initialStock = 0;
};
struct Item {
    std::uint64_t serial = 0;
    std::string sku;
    double health = 1;
    bool refundable = false;
    std::int64_t paid = 0;
};
struct Slot {
    std::optional<Item> installed;
    int secured = 0;
};
struct Vehicle {
    std::string id;
    std::map<std::string, Slot> parts;
    std::uint64_t revision = 1;
    Location location = Location::Bay;
    bool hoodOpen = false, lifted = false, engineOn = false;
};
struct Fault { std::string part; double health = 0; };
struct OrderDef {
    std::string id, title, customerSymptoms, vehicle;
    std::vector<Fault> faults;
    Test test{};
    std::int64_t payment = 0;
    int reputation = 0;
};
struct Job {
    std::string order;
    std::uint64_t instance = 0, verifiedRevision = 0;
    std::vector<std::string> inspected;
};
struct Settings {
    int preset = 2;
    double fov = 85, sensitivity = 1, textScale = 1;
    bool invertLook = false, assisted = true;
    bool motionBlur = false, filmGrain = false, depthOfField = false, cameraShake = false;
    std::array<double, 5> volumes{{0.8, 1, 0.8, 0.6, 0.3}};
    std::map<std::string, std::string> bindings{
        {"forward", "W"}, {"back", "S"}, {"left", "A"}, {"right", "D"},
        {"interact", "E"}, {"tool", "LeftMouseButton"}, {"board", "Tab"},
        {"pause", "Escape"}, {"camera", "C"}, {"recover", "R"}, {"handbrake", "SpaceBar"}};
};
struct State {
    Mode mode = Mode::Career;
    std::int64_t money = 150000;
    int reputation = 0;
    std::uint64_t nextSerial = 1, nextJob = 1;
    std::map<std::string, Vehicle> vehicles;
    std::map<std::uint64_t, Item> inventory;
    std::map<std::string, int> stock;
    std::map<std::string, std::uint32_t> completions;
    std::optional<Job> job;
    Settings settings;
    std::string selectedVehicle = "hatch";
    Location playerLocation = Location::Bay;
    std::array<double, 3> playerPosition{{0, 0, 0}};
};
struct Measurements {
    bool assembled = false, starts = false;
    double batteryVolts = 0, chargingVolts = 0, crankSeconds = 0;
    double misfirePercent = 0, coolantC = 0, intakePercent = 0;
    double massKg = 0, powerKw = 0, torqueNm = 0, grip = 0;
    double brakeDecel = 0, damping = 0, zeroTo100 = 0, brake100Meters = 0;
    std::vector<std::string> faults;
};
struct LoadResult { Result result; bool recoveredBackup = false; };

const std::vector<VehicleDef>& vehicleCatalog();
const std::vector<Sku>& shopCatalog();
const std::vector<OrderDef>& orderCatalog();
const VehicleDef* findVehicle(const std::string& id);
const PartDef* findPart(const std::string& vehicle, const std::string& id);
const Sku* findSku(const std::string& id);
const OrderDef* findOrder(const std::string& id);
std::string standardSku(const std::string& vehicle, Kind kind);
State newGame(Mode mode = Mode::Career);
Result validate(const State& state);
bool driveReady(const State& state, const std::string& vehicle);
Measurements diagnose(const State& state, const std::string& vehicle);
Result acceptOrder(State& state, const std::string& order);
Result inspect(State& state, const std::string& vehicle, const std::string& part, Tool tool);
Result purchase(State& state, const std::string& sku);
Result refund(State& state, std::uint64_t serial);
Result setHood(State& state, const std::string& vehicle, bool open);
Result setLift(State& state, const std::string& vehicle, bool up);
Result fastener(State& state, const std::string& vehicle, const std::string& part, Tool tool, bool tighten);
Result removePart(State& state, const std::string& vehicle, const std::string& part, Tool tool);
Result installPart(State& state, const std::string& vehicle, const std::string& part, std::uint64_t serial);
Result moveVehicle(State& state, const std::string& vehicle, Location location);
Result setEngine(State& state, const std::string& vehicle, bool on);
Result recoverVehicle(State& state, const std::string& vehicle);
Result runVerification(State& state);
Result completeOrder(State& state);
Result updateSettings(State& state, const Settings& settings);

// Bounded, checksummed, explicitly little-endian schema 1. Failed loads cannot
// partially replace a session. Save writes a temporary file and preserves the
// last valid backup before atomically replacing the primary on Windows/POSIX.
std::vector<std::uint8_t> serialize(const State& state);
Result deserialize(const std::vector<std::uint8_t>& bytes, State& destination);
Result save(const State& state, const std::filesystem::path& path);
LoadResult load(State& destination, const std::filesystem::path& path);
}
