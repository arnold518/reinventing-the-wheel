// Prints the guest header of a system description:
//   system_header <built-in name | path/to/description.json>
#include "system/GuestHeader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("Usage: system_header <built-in name | description.json>");
        const std::string what = argv[1];
        std::string text, source;
        if (what.ends_with(".json")) {
            std::ifstream in(what);
            if (!in) throw std::invalid_argument("Cannot read " + what);
            std::ostringstream s;
            s << in.rdbuf();
            text = s.str();
            source = what.substr(what.find_last_of('/') == std::string::npos ? 0 : what.find_last_of('/') + 1);
        } else {
            text = circuit::system::builtinSystemJson(what);
            source = "systems/" + what + ".json";
        }
        std::cout << circuit::system::guestHeader(circuit::system::SystemDescription::parse(text), source);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
