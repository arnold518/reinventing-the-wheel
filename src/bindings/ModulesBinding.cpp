#include "Bindings.hpp"
#include "components/TimingProfile.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/memory/SRAM.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "visualizer/MemoryInspector.hpp"
#include "simulator/Simulator.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "ModuleBindingHelpers.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

void bindModules(py::module_& m) {
    // The timing profile for circuits built afterwards (components/TimingProfile.hpp).
    m.def("set_timing_profile", [](const std::string& name) { circuit::timing::setActiveTimingProfile(name); },
          py::arg("name"), "Select the leaf-delay timing profile for circuits built afterwards");
    m.def("timing_profile", [] { return circuit::timing::activeTimingProfile().name; },
          "Name of the active timing profile");
    m.def("timing_profile_names", &circuit::timing::timingProfileNames, "Names of all timing profiles");
    m.def("timing_profile_info", [](const std::string& name) {
              const auto& profile = circuit::timing::timingProfile(name);
              py::dict info;
              info["name"] = profile.name;
              info["description"] = profile.description;
              info["tick_picoseconds"] = profile.tick_picoseconds;
              return info;
          }, py::arg("name"), "Name, description and tick length in picoseconds (0: an abstract tick)");
    m.def("memory_activity_timestamps", &circuit::visualizer::memoryActivityTimestamps,
          py::arg("root"));
    py::class_<circuit::visualizer::MemoryInspector>(m, "MemoryInspector")
        .def(py::init<std::shared_ptr<Component>, const Simulator&>(), py::keep_alive<1, 3>())
        .def("state_json", &circuit::visualizer::MemoryInspector::stateJson,
             py::arg("index"), py::arg("time"), py::arg("scenario") = "",
             py::arg("address") = -1, py::arg("count") = 64);
    m.def("initialize_memory_bytes", [](const std::shared_ptr<Component>& component,
                                        uint32_t address, const std::vector<uint8_t>& bytes) {
        auto view = std::dynamic_pointer_cast<MemoryStateView>(component);
        if (!view) throw std::invalid_argument("Component does not expose memory storage");
        view->initializeMemoryBytes(address, bytes);
    }, py::arg("component"), py::arg("address"), py::arg("bytes"),
       "Provision an image before simulation; never a runtime memory write.");
    m.def("create_memory_device_component", [](const std::string& family, const std::string& kind,
            const std::string& name, const circuit::ParameterMap& parameters, const std::string& fidelity) {
        if (family != "dram" && family != "rom" && family != "nor-flash" && family != "matrix")
            throw std::invalid_argument("Expected dram, rom, nor-flash or matrix family");
        if (fidelity != "structural" && fidelity != "behavioral") throw std::invalid_argument("Invalid fidelity");
        const auto selected = fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral;
        const auto id = circuit::canonicalContractId("memory." + family + "." + kind);
        const auto& catalog = circuit::builtinComponentCatalog();
        const auto implementations = catalog.implementationsFor(id);
        if (std::none_of(implementations.begin(), implementations.end(), [&](const auto& impl) {
                return impl.fidelity == selected && impl.supports(parameters);
            })) throw std::invalid_argument("Unsupported memory component, parameters or fidelity");
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({id, name, parameters, {}, {}, {}},
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name, selected)).root);
    }, py::arg("family"), py::arg("kind"), py::arg("name"), py::arg("parameters") = circuit::ParameterMap{},
       py::arg("fidelity") = "structural");
    m.def("create_sram_component", [](const std::string& kind, const std::string& name,
            const circuit::ParameterMap& parameters, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral") throw std::invalid_argument("Invalid SRAM fidelity");
        const auto selected = fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral;
        const auto id = "memory.sram." + kind;
        const auto& catalog = circuit::builtinComponentCatalog();
        const auto implementations = catalog.implementationsFor(id);
        if (std::none_of(implementations.begin(), implementations.end(), [&](const auto& impl) {
            return impl.fidelity == selected && impl.supports(parameters);
        })) throw std::invalid_argument("Requested SRAM component/fidelity/parameters are not supported");
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({id, name, parameters, {}, {}, {}},
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name, selected)).root);
    }, py::arg("kind"), py::arg("name"), py::arg("parameters") = circuit::ParameterMap{}, py::arg("fidelity") = "structural");
    const auto create_sram = [](const std::string& kind, const std::string& name, size_t width, size_t depth,
            size_t words_per_row, SimulationTime phase_ticks, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral") throw std::invalid_argument("Invalid SRAM fidelity");
        SRAMDeviceConfig config{depth, words_per_row, phase_ticks};
        config.validate(width, fidelity == "structural" || kind == "bank");
        auto profile = circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name,
            fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral);
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
            {"memory.sram." + kind + "." + std::to_string(width), name, sramParameters(config), {}, {}, {}}, profile).root);
    };
    m.def("create_sram", [create_sram](const std::string& name, size_t width, size_t depth, size_t k, SimulationTime ticks, const std::string& fidelity) {
        return create_sram("device", name, width, depth, k, ticks, fidelity);
    }, py::arg("name"), py::arg("data_width") = 8, py::arg("depth_words") = 4, py::arg("words_per_row") = 2,
       py::arg("phase_ticks") = 32, py::arg("fidelity") = "structural");
    m.def("create_sram_storage_bank", [create_sram](const std::string& name, size_t width, size_t depth, size_t k, const std::string& fidelity) {
        return create_sram("bank", name, width, depth, k, 32, fidelity);
    }, py::arg("name"), py::arg("data_width") = 8, py::arg("depth_words") = 4, py::arg("words_per_row") = 2,
       py::arg("fidelity") = "structural");
    auto create_endpoint = [](const std::string& kind, const std::string& name, const std::string& model,
            size_t width, size_t depth, circuit::ParameterMap parameters, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral") throw std::invalid_argument("Unknown memory fidelity");
        parameters["depth_words"] = std::to_string(depth);
        auto choice = fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral;
        // Python callers pass the layer first ("endpoint", "scalable", "controller", "device").
        circuit::ComponentBuildRequest request{circuit::canonicalContractId("memory." + kind + "." + model + "." + std::to_string(width)),name,parameters,{},{},{}};
        const auto& catalog = circuit::builtinComponentCatalog();
        const auto implementations = catalog.implementationsFor(request.contract_id);
        if (std::none_of(implementations.begin(), implementations.end(), [&](const auto& i) { return i.fidelity == choice && i.supports(parameters); }))
            throw std::invalid_argument("Unsupported memory endpoint configuration/fidelity");
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot(request,
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(),name,choice)).root);
    };
    m.def("create_memory_device", [create_endpoint](const std::string& name, const std::string& model,
        size_t width, size_t depth, circuit::ParameterMap parameters) {
        return create_endpoint("device",name,model,width,depth,std::move(parameters),"behavioral");
    }, py::arg("name"), py::arg("model"), py::arg("data_width")=32, py::arg("depth_words")=4096, py::arg("parameters")=circuit::ParameterMap{});
    m.def("create_memory_controller", [create_endpoint](const std::string& name, const std::string& model,
        size_t width, size_t depth, circuit::ParameterMap parameters) {
        return create_endpoint("controller",name,model,width,depth,std::move(parameters),"structural");
    }, py::arg("name"), py::arg("model"), py::arg("data_width")=32, py::arg("depth_words")=4, py::arg("parameters")=circuit::ParameterMap{});
    m.def("create_memory_endpoint", [create_endpoint](const std::string& name, const std::string& model,
        size_t width, size_t depth, circuit::ParameterMap parameters, const std::string& fidelity) {
        return create_endpoint("endpoint",name,model,width,depth,std::move(parameters),fidelity);
    }, py::arg("name"), py::arg("model"), py::arg("data_width")=32, py::arg("depth_words")=4,
       py::arg("parameters")=circuit::ParameterMap{}, py::arg("fidelity")="structural");
    m.def("create_ff_memory", [](const std::string& name, size_t width, size_t depth,
            SimulationTime delay, bool history, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral")
            throw std::invalid_argument("FF memory fidelity must be structural or behavioral");
        const auto choice = fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral;
        const auto id = "memory.ff.device." + std::to_string(width);
        circuit::ParameterMap parameters{{"depth_words", std::to_string(depth)},
            {"read_delay_ticks", std::to_string(delay)}, {"record_history", history ? "1" : "0"}};
        const auto& catalog = circuit::builtinComponentCatalog();
        const auto implementations = catalog.implementationsFor(id);
        if (std::none_of(implementations.begin(), implementations.end(), [&](const auto& i) {
                return i.fidelity == choice && i.supports(parameters);
            })) throw std::invalid_argument("Unsupported FF memory geometry, delay or fidelity");
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({id,name,parameters,{},{},{}},
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(),name,choice)).root);
    }, py::arg("name"), py::arg("data_width") = 32, py::arg("depth_words") = 4,
       py::arg("read_delay_ticks") = 1, py::arg("record_history") = true,
       py::arg("fidelity") = "structural");
    m.def("create_ff_word", [](const std::string& name, size_t width, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral")
            throw std::invalid_argument("FF word fidelity must be structural or behavioral");
        auto profile = circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name,
            fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral);
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
            {"memory.ff.word." + std::to_string(width), name, {}, {}, {}, {}}, profile).root);
    }, py::arg("name"), py::arg("data_width") = 32, py::arg("fidelity") = "structural");
    m.def("create_ff_storage_bank", [](const std::string& name, size_t width, size_t depth, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral")
            throw std::invalid_argument("FF bank fidelity must be structural or behavioral");
        auto profile = circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name,
            fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral);
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
            {"memory.ff.bank." + std::to_string(width), name,
             {{"depth_words", std::to_string(depth)}}, {}, {}, {}}, profile).root);
    }, py::arg("name"), py::arg("data_width") = 32, py::arg("depth_words") = 4,
       py::arg("fidelity") = "structural");
    m.def("load_memory_bytes", [](const std::shared_ptr<Component>& component, uint32_t address,
                                  const std::vector<uint8_t>& bytes) {
        auto memory = std::dynamic_pointer_cast<MemoryStateView>(component);
        if (!memory) throw std::invalid_argument("Component does not support memory image loading");
        memory->loadMemoryBytes(address, bytes);
    });
    m.def("memory_configuration", [](const std::shared_ptr<Component>& component) {
        auto memory = std::dynamic_pointer_cast<MemoryStateView>(component);
        if (!memory) throw std::invalid_argument("Component does not expose memory storage");
        const auto& storage = memory->memoryStorage();
        py::dict result;
        result["capacity_bytes"] = storage.capacityBytes();
        result["depth_words"] = storage.capacityWords();
        result["data_width"] = storage.wordBits();
        result["record_history"] = storage.isHistoryRecordingEnabled();
        return result;
    });
    m.def("memory_snapshot", [](const std::shared_ptr<Component>& component, SimulationTime time,
                                uint32_t address, size_t count) {
        auto view = std::dynamic_pointer_cast<MemoryStateView>(component);
        if (!view) throw std::invalid_argument("Component does not expose memory storage");
        const auto& memory = view->memoryStorage();
        py::dict result;
        result["words"] = memory.getWordsAtTime(time, address, std::min(count, size_t{256}));
        result["touched_words"] = memory.getTouchedWordsAtTime(time, 256);
        result["touched_count"] = memory.getTouchedWordCountAtTime(time);
        return result;
    }, py::arg("component"), py::arg("time"), py::arg("address") = 0, py::arg("count") = 1);
    m.def(
        "disassemble_rv32i_instruction",
        &rv32i::RV32IDecoder::disassemble,
        py::arg("raw"),
        py::arg("pc") = 0,
        "Returns a compact RV32I assembly string for a 32-bit instruction word.");
    m.def(
        "get_register_state_at_time",
        [](const std::shared_ptr<Component>& component, SimulationTime target_time) {
            auto view = std::dynamic_pointer_cast<RegisterStateView>(component);
            if (!view) {
                throw std::invalid_argument(
                    "Component does not provide the register-state-view capability");
            }
            return view->getRegisterStateAtTime(target_time);
        },
        py::arg("component"),
        py::arg("time"),
        "Returns register state through the shared register-state-view capability.");

    bindBasicModules(m);
    bindStorageModules(m);
    bindPipelineModules(m);
    bindCpuModules(m);
    bindArithmeticModules(m);
    bindUtilityModules(m);

}
