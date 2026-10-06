#include "Core/GarageCore.h"
#include <iostream>
#include <iomanip>
#include <sstream>

static std::string quote(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"'; return out.str();
}
static void list(const std::vector<std::string>& values) {
    std::cout << '['; bool comma = false;
    for (const auto& s : values) { if (comma) std::cout << ','; std::cout << quote(s); comma = true; } std::cout << ']';
}
int main() {
    std::cout << "{\"schema\":1,\"vehicles\":["; bool vehicleComma = false;
    for (const auto& v : gr::vehicleCatalog()) {
        if (vehicleComma) std::cout << ',';
        vehicleComma = true;
        std::cout << "{\"id\":" << quote(v.id) << ",\"name\":" << quote(v.name) << ",\"architecture\":" << quote(v.architecture)
            << ",\"mass_kg\":" << v.massKg << ",\"power_kw\":" << v.powerKw << ",\"torque_nm\":" << v.torqueNm << ",\"parts\":[";
        bool partComma = false;
        for (const auto& p : v.parts) {
            if (partComma) std::cout << ',';
            partComma = true;
            std::cout << "{\"id\":" << quote(p.id) << ",\"label\":" << quote(p.label) << ",\"kind\":" << static_cast<int>(p.kind)
                << ",\"fasteners\":" << p.fasteners << ",\"requires_hood\":" << (p.hood ? "true" : "false")
                << ",\"requires_lift\":" << (p.lift ? "true" : "false") << ",\"remove_first\":";
            list(p.removeFirst); std::cout << ",\"install_first\":"; list(p.installFirst);
            std::cout << ",\"standard_sku\":" << quote(gr::standardSku(v.id, p.kind)) << '}';
        }
        std::cout << "]}";
    }
    std::cout << "],\"orders\":["; bool orderComma = false;
    for (const auto& j : gr::orderCatalog()) {
        if (orderComma) std::cout << ',';
        orderComma = true;
        std::cout << "{\"id\":" << quote(j.id) << ",\"title\":" << quote(j.title) << ",\"symptoms\":" << quote(j.customerSymptoms)
            << ",\"vehicle\":" << quote(j.vehicle) << ",\"payment_cents\":" << j.payment << ",\"reputation\":" << j.reputation
            << ",\"verification_protocol\":" << static_cast<int>(j.test) << ",\"faults\":[";
        bool faultComma = false;
        for (const auto& f : j.faults) { if (faultComma) std::cout << ','; faultComma = true; std::cout << "{\"part\":" << quote(f.part) << ",\"health\":" << f.health << '}'; }
        std::cout << "]}";
    }
    std::cout << "],\"shop\":["; bool skuComma = false;
    for (const auto& s : gr::shopCatalog()) {
        if (skuComma) std::cout << ',';
        skuComma = true;
        std::cout << "{\"id\":" << quote(s.id) << ",\"label\":" << quote(s.label) << ",\"vehicle\":" << quote(s.vehicle)
            << ",\"kind\":" << static_cast<int>(s.kind) << ",\"grade\":" << s.grade << ",\"price_cents\":" << s.price << ",\"stock\":" << s.initialStock << '}';
    }
    std::cout << "]}\n";
}
