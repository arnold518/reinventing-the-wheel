#include "system/Json.hpp"
#include <fstream>
#include <iterator>
#include "visualizer/VisualizerSession.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

size_t parseIndex(const char* value) {
    try {
        size_t consumed = 0;
        const auto result = std::stoull(value, &consumed);
        if (value[consumed] != '\0') throw std::invalid_argument("suffix");
        return static_cast<size_t>(result);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("Invalid index: ") + value);
    }
}

void usage() {
    std::cerr
        << "Usage:\n"
        << "  VisualizerSessionProbe --list\n"
        << "  VisualizerSessionProbe <topology|state|component|slice|examples|preview> "
           "<registered-test> <scenario> [command arguments] "
           "[--revision N] [--fidelity gate|logic|visual|compact|test] [--override PATH FID]...\n";
}

} // namespace

std::string defaultFidelity(const std::string& registered_test) {
    std::ifstream file(std::string(CIRCUITSIM_SOURCE_DIR) + "/visualizer-v2/static/scenarios.json");
    if (!file) throw std::runtime_error("Cannot read the scenario catalog");
    const auto catalog = circuit::system::Json::parse(std::string(std::istreambuf_iterator<char>(file), {}));
    if (const auto* entry = catalog.at("components").find(registered_test))
        return entry->at("fidelity").string("fidelity");
    if (const auto at = registered_test.find("/program-"); at != std::string::npos) {
        const auto id = registered_test.substr(at + 1, std::string("program-00").size());
        for (const auto& program : catalog.at("programs").array("programs"))
            if (program.at("id").string("id") == id) return program.at("fidelity").string("fidelity");
    }
    throw std::invalid_argument("No catalog fidelity for " + registered_test);
}

int main(int argc, char** argv) {
    try {
        using circuit::visualizer::VisualizerSession;
        if (argc == 2 && std::string(argv[1]) == "--list") {
            std::cout << VisualizerSession::visualizableTestsJson() << '\n';
            return EXIT_SUCCESS;
        }
        if (argc < 4) {
            usage();
            return EXIT_FAILURE;
        }

        const std::string command = argv[1];
        const std::string registered_test = argv[2];
        const std::string scenario = argv[3];
        size_t cursor = 4;
        size_t state_index = 0;
        std::string component_id;
        std::vector<std::string> owner_ids;
        if (command == "state") {
            if (cursor >= static_cast<size_t>(argc)) {
                throw std::invalid_argument("state requires an index");
            }
            state_index = parseIndex(argv[cursor++]);
        } else if (command == "component") {
            if (cursor + 1 >= static_cast<size_t>(argc)) {
                throw std::invalid_argument("component requires an index and component id");
            }
            state_index = parseIndex(argv[cursor++]);
            component_id = argv[cursor++];
        } else if (command == "slice") {
            while (cursor < static_cast<size_t>(argc)
                   && std::string(argv[cursor]).rfind("--", 0) != 0) {
                owner_ids.emplace_back(argv[cursor++]);
            }
            if (owner_ids.empty()) throw std::invalid_argument("slice requires at least one owner id");
        } else if (command != "topology" && command != "examples" && command != "preview") {
            throw std::invalid_argument("Unknown command: " + command);
        }

        size_t revision = 0;
        // Without --fidelity, the scenario's own profile from the catalog
        // (programs too), as the visualizer opens it; "test" keeps
        // the test's own build profile (the WebAssembly bridge's default,
        // which tools/run-web-tests.py selects for its parity checks).
        std::optional<std::string> requested_fidelity;
        int64_t address = -1;
        size_t count = 64;
        std::map<std::string, std::string> overrides;
        while (cursor < static_cast<size_t>(argc)) {
            const std::string option = argv[cursor++];
            if(option=="--fidelity"){
                if(cursor>=static_cast<size_t>(argc))throw std::invalid_argument("--fidelity requires a preset");
                requested_fidelity=argv[cursor++];
            } else if (option == "--address" || option == "--count") {
                if (cursor >= static_cast<size_t>(argc)) throw std::invalid_argument("Missing memory page argument");
                if (option == "--address") address = static_cast<int64_t>(parseIndex(argv[cursor++]));
                else count = parseIndex(argv[cursor++]);
            } else if (option == "--revision") {
                if (cursor >= static_cast<size_t>(argc)) {
                    throw std::invalid_argument("--revision requires a value");
                }
                revision = parseIndex(argv[cursor++]);
            } else if (option == "--override") {
                if (cursor + 1 >= static_cast<size_t>(argc)) {
                    throw std::invalid_argument("--override requires a path and fidelity");
                }
                const std::string path = argv[cursor++];
                overrides[path] = argv[cursor++];
            } else {
                throw std::invalid_argument("Unknown option: " + option);
            }
        }

        const char* probe_default = std::getenv("CIRCUITSIM_PROBE_DEFAULT_FIDELITY");
        std::string fidelity = requested_fidelity ? *requested_fidelity
                             : probe_default ? std::string(probe_default) : defaultFidelity(registered_test);
        if (fidelity == "test") fidelity.clear();  // the test's own build profile

        if (command == "preview") {
            // Build-only part list; no session, nothing simulated.
            std::cout << VisualizerSession::previewComponentsJson(registered_test, overrides, fidelity) << '\n';
            return EXIT_SUCCESS;
        }
        VisualizerSession session(
            registered_test,
            scenario,
            std::move(overrides),
            revision,fidelity);
        if (command == "topology") {
            std::cout << session.topologyResponseJson();
        } else if (command == "examples") {
            std::cout << session.exampleRowsJson();
        } else if (command == "state") {
            std::cout << session.stateJson(state_index);
        } else if (command == "component") {
            std::cout << session.componentStateJson(state_index, component_id, address, count);
        } else {
            std::cout << session.topologySliceJson(owner_ids);
        }
        std::cout << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
