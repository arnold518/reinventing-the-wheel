#include "Bindings.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "visualizer/MemoryInspector.hpp"
#include "simulator/Simulator.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <utility>

#include "components/BasicComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/memory/FFMemoryCatalog.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/SRAM.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/storage/Register.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "modules/rv32i/RV32IMemoryRequest.hpp"

#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/ALU32FlagUnit.hpp"
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "rv32i/RV32IDecoder.hpp"

namespace py = pybind11;

template<typename T, typename Base>
void bindModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, Base, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<T>(instance_name);
        }), py::arg("name"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal);
}

template<typename T>
void bindBasicModule(py::module_& m, const char* name, const char* description) {
    bindModule<T, BasicComponent>(m, name, description);
}

template<typename T>
void bindConstantModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, BasicComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name, uint64_t constant_value) {
            return Component::create<T>(instance_name, constant_value);
        }), py::arg("name"), py::arg("constant_value"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal)
        .def("get_constant_value", &T::getConstantValue);
}

template<typename T>
void bindCompositeModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, IOComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<T>(instance_name);
        }), py::arg("name"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal);
}

void bindModules(py::module_& m) {
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
            size_t words_per_row, size_t phase_ticks, const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral") throw std::invalid_argument("Invalid SRAM fidelity");
        SRAMDeviceConfig config{depth, words_per_row, phase_ticks};
        config.validate(width, fidelity == "structural" || kind == "bank");
        auto profile = circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name,
            fidelity == "structural" ? circuit::Fidelity::Structural : circuit::Fidelity::Behavioral);
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
            {"memory.sram." + kind + "." + std::to_string(width), name, sramParameters(config), {}, {}, {}}, profile).root);
    };
    m.def("create_sram", [create_sram](const std::string& name, size_t width, size_t depth, size_t k, size_t ticks, const std::string& fidelity) {
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
    m.def("create_memory_container", [create_endpoint](const std::string& name, const std::string& model,
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
            size_t delay, bool history, const std::string& fidelity) {
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
    m.def("memory_snapshot", [](const std::shared_ptr<Component>& component, size_t time,
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
        [](const std::shared_ptr<Component>& component, size_t target_time) {
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

    // --- NOTGate Binding ---
    py::class_<NOTGate, BasicComponent, std::shared_ptr<NOTGate>>(m, "NOTGate", "A standard 1-input NOT gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NOTGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NOTGate::getName)
        .def("get_parent", &NOTGate::getParent)
        .def("get_children", &NOTGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NOTGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NOTGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- ANDGate Binding ---
    py::class_<ANDGate, BasicComponent, std::shared_ptr<ANDGate>>(m, "ANDGate", "A standard 2-input AND gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<ANDGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &ANDGate::getName)
        .def("get_parent", &ANDGate::getParent)
        .def("get_children", &ANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ANDGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ANDGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- NANDGate Binding ---
    py::class_<NANDGate, BasicComponent, std::shared_ptr<NANDGate>>(m, "NANDGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NANDGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NANDGate::getName)
        .def("get_parent", &NANDGate::getParent)
        .def("get_children", &NANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NANDGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NANDGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- ORGate Binding ---
    py::class_<ORGate, BasicComponent, std::shared_ptr<ORGate>>(m, "ORGate", "A standard 2-input OR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<ORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &ORGate::getName)
        .def("get_parent", &ORGate::getParent)
        .def("get_children", &ORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- NORGate Binding ---
    py::class_<NORGate, BasicComponent, std::shared_ptr<NORGate>>(m, "NORGate", "A standard 2-input NOR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NORGate::getName)
        .def("get_parent", &NORGate::getParent)
        .def("get_children", &NORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- XORGate Binding ---
    py::class_<XORGate, BasicComponent, std::shared_ptr<XORGate>>(m, "XORGate", "A standard 2-input XOR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<XORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &XORGate::getName)
        .def("get_parent", &XORGate::getParent)
        .def("get_children", &XORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &XORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &XORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    bindCompositeModule<SRLatch>(m, "SRLatch", "Structural active-low SR latch built from cross-coupled NAND gates.");
    bindCompositeModule<GatedDLatch>(m, "GatedDLatch", "Structural gated D latch with reset.");
    bindCompositeModule<DFlipFlop>(m, "DFlipFlop", "Structural rising-edge master-slave D flip-flop.");

    // --- ClockGenerator Binding ---
    py::class_<ClockGenerator, BasicComponent, std::shared_ptr<ClockGenerator>>(m, "ClockGenerator", "Generates a periodic clock signal.", py::module_local(false))
        .def(py::init([](const std::string& instance_name, size_t half_period) {
            return Component::create<ClockGenerator>(instance_name, half_period);
        }), py::arg("name"), py::arg("half_period"))
        .def("get_name", &ClockGenerator::getName)
        .def("get_parent", &ClockGenerator::getParent)
        .def("get_children", &ClockGenerator::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ClockGenerator::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ClockGenerator::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- HalfAdder Binding ---
    py::class_<HalfAdder, IOComponent, std::shared_ptr<HalfAdder>>(m, "HalfAdder", "A standard 2-input half-adder.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<HalfAdder>(instance_name);
        }), py::arg("name"))
        .def("get_name", &HalfAdder::getName)
        .def("get_parent", &HalfAdder::getParent)
        .def("get_children", &HalfAdder::getChildren, py::return_value_policy::reference_internal);
    
    // --- FullAdder Binding ---
    py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>(m, "FullAdder", "A standard full-adder.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<FullAdder>(instance_name);
        }), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren, py::return_value_policy::reference_internal);


    bindCompositeModule<Mux2to1>(m, "Mux2to1", "2:1 one-bit multiplexer.");
    bindCompositeModule<Mux4to1>(m, "Mux4to1", "4:1 one-bit multiplexer.");
    bindCompositeModule<Mux8to1>(m, "Mux8to1", "8:1 one-bit multiplexer.");
    bindCompositeModule<Mux32to1>(m, "Mux32to1", "32:1 one-bit multiplexer.");
    bindCompositeModule<Mux2to1_4bit>(m, "Mux2to1_4bit", "2:1 4-bit multiplexer.");
    bindCompositeModule<Mux2to1_32bit>(m, "Mux2to1_32bit", "2:1 32-bit multiplexer.");
    bindCompositeModule<Mux4to1_32bit>(m, "Mux4to1_32bit", "4:1 32-bit multiplexer.");
    bindCompositeModule<Mux8to1_8bit>(m, "Mux8to1_8bit", "8:1 8-bit multiplexer.");
    bindCompositeModule<Mux8to1_32bit>(m, "Mux8to1_32bit", "8:1 32-bit multiplexer.");
    bindCompositeModule<Mux32to1_32bit>(m, "Mux32to1_32bit", "32:1 32-bit multiplexer.");
    bindCompositeModule<Decoder5to32>(m, "Decoder5to32", "5-bit enabled one-hot decoder.");

    bindCompositeModule<FlipFlopCell>(m, "FlipFlopCell", "Structural one-bit storage cell with write enable and reset.");
    bindCompositeModule<Register<1>>(m, "RegisterW1", "Whole-word 1-bit register built from flip-flop cells.");
    bindCompositeModule<Register<2>>(m, "RegisterW2", "Whole-word 2-bit register built from flip-flop cells.");
    bindCompositeModule<Register<3>>(m, "RegisterW3", "Whole-word 3-bit register built from flip-flop cells.");
    bindCompositeModule<Register<4>>(m, "RegisterW4", "Whole-word 4-bit register built from flip-flop cells.");
    bindCompositeModule<Register<5>>(m, "RegisterW5", "Whole-word 5-bit register built from flip-flop cells.");
    bindCompositeModule<Register<8>>(m, "RegisterW8", "Whole-word 8-bit register built from flip-flop cells.");
    bindCompositeModule<Register<16>>(m, "RegisterW16", "Whole-word 16-bit register built from flip-flop cells.");
    bindCompositeModule<Register<32>>(m, "RegisterW32", "Whole-word 32-bit register built from flip-flop cells.");
    bindCompositeModule<RegisterFile32x32>(m, "RegisterFile32x32", "RV32I register file built from 31 Register<32> words and hardwired x0.");
    bindCompositeModule<RV32IDecodeControlUnit>(m, "RV32IDecodeControlUnit", "Structural RV32I field, immediate, and control decoder.");
    bindCompositeModule<RV32IExecutionControlStatusUnit>(m, "RV32IExecutionControlStatusUnit", "Structural RV32I commit permission, memory handshake, halt, and trap-state block.");
    bindCompositeModule<RV32IIFIDPipelineRegister>(m, "RV32IIFIDPipelineRegister", "RV32I IF/ID pipeline state.");
    bindCompositeModule<RV32IIDEXPipelineRegister>(m, "RV32IIDEXPipelineRegister", "RV32I ID/EX pipeline state.");
    bindCompositeModule<RV32IEXMEMPipelineRegister>(m, "RV32IEXMEMPipelineRegister", "RV32I EX/MEM pipeline state.");
    bindCompositeModule<RV32IMEMWBPipelineRegister>(m, "RV32IMEMWBPipelineRegister", "RV32I MEM/WB pipeline state.");
    bindCompositeModule<RV32IForwardingUnit>(m, "RV32IForwardingUnit", "RV32I EX/MEM and MEM/WB forwarding selector.");
    bindCompositeModule<RV32IPredictionCounter>(m, "RV32IPredictionCounter", "One-bit or saturating two-bit update logic.");
    bindCompositeModule<RV32IBranchPredecode>(m, "RV32IBranchPredecode", "Fetch-time B/J target extraction.");
    bindCompositeModule<RV32IBranchPredictor>(m, "RV32IBranchPredictor", "Configurable branch direction prediction.");
    bindCompositeModule<RV32IBranchRecovery>(m, "RV32IBranchRecovery", "Next-PC comparison and qualified training.");
    bindCompositeModule<RV32IPredictionPath>(m, "RV32IPredictionPath", "Grouped prediction metadata and recovery.");
    bindCompositeModule<RV32IHazardDetectionUnit>(m, "RV32IHazardDetectionUnit", "RV32I load-use hazard detector.");
    bindCompositeModule<RV32IMemoryAlignmentUnit>(m, "RV32IMemoryAlignmentUnit", "RV32I byte, halfword, and word alignment checker.");
    bindCompositeModule<RV32IPipelineRetirementUnit>(m, "RV32IPipelineRetirementUnit", "RV32I precise retirement and trap-priority unit.");
    bindCompositeModule<RV32IPipelineCoordinator>(m, "RV32IPipelineCoordinator", "RV32I pipeline stall, bubble, flush, and stage-write coordinator.");
    bindCompositeModule<RV32IFetchStage>(m, "RV32IFetchStage", "RV32I instruction-fetch stage.");
    bindCompositeModule<RV32IDecodeStage>(m, "RV32IDecodeStage", "RV32I decode and register-read stage.");
    bindCompositeModule<RV32IExecuteStage>(m, "RV32IExecuteStage", "RV32I forwarding, ALU, and control-flow stage.");
    bindCompositeModule<RV32IMemoryStage>(m, "RV32IMemoryStage", "RV32I data-memory and alignment stage.");
    bindCompositeModule<RV32IPipelineExecutionStatus>(m, "RV32IPipelineExecutionStatus", "Sticky pipeline halt and trap status.");
    bindCompositeModule<RV32IWritebackStage>(m, "RV32IWritebackStage", "Combinational RV32I result selection and retirement permission.");
    bindCompositeModule<RV32IFiveStageCore>(m, "RV32IFiveStageCore", "Educational RV32I five-stage pipelined core.");
    bindCompositeModule<RV32IBaselinePipelineCore>(m, "RV32IBaselinePipelineCore", "Stall-only, non-speculative RV32I pipeline with direct memories.");
    bindCompositeModule<RV32IBaselineFetchStage>(m, "RV32IBaselineFetchStage", "Direct fetch and redirect selection.");
    bindCompositeModule<RV32IBaselineDecodeStage>(m, "RV32IBaselineDecodeStage", "Decode and register file without writeback bypass.");
    bindCompositeModule<RV32IBaselineExecuteStage>(m, "RV32IBaselineExecuteStage", "Captured-operand ALU and branch resolution.");
    bindCompositeModule<RV32IBaselineMemoryStage>(m, "RV32IBaselineMemoryStage", "Direct load/store access with fault qualification.");
    bindCompositeModule<RV32IBaselineDependencyUnit>(m, "RV32IBaselineDependencyUnit", "RAW dependency checks against EX, MEM and WB.");
    bindCompositeModule<RV32IBaselineControl>(m, "RV32IBaselineControl", "Stall, bubble, branch-wait and precise-stop control.");
    py::class_<RV32IBitPatternMatcher, IOComponent, std::shared_ptr<RV32IBitPatternMatcher>>(
        m,
        "RV32IBitPatternMatcher",
        "Structural masked RV32I instruction-pattern matcher.",
        py::module_local(false))
        .def(py::init([](const std::string& instance_name, uint32_t mask, uint32_t value) {
            return Component::create<RV32IBitPatternMatcher>(instance_name, mask, value);
        }), py::arg("name"), py::arg("mask"), py::arg("value"))
        .def("get_name", &RV32IBitPatternMatcher::getName)
        .def("get_parent", &RV32IBitPatternMatcher::getParent)
        .def("get_children", &RV32IBitPatternMatcher::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &RV32IBitPatternMatcher::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &RV32IBitPatternMatcher::getAllOutputPins, py::return_value_policy::reference_internal)
        .def_property_readonly("mask", &RV32IBitPatternMatcher::mask)
        .def_property_readonly("value", &RV32IBitPatternMatcher::value);
    bindCompositeModule<RV32ISingleCycleCore>(m, "RV32ISingleCycleCore", "Non-pipelined RV32I core composed from six logical stages and execution control.");
    bindCompositeModule<RV32ISingleCycleFetchStage>(m, "RV32ISingleCycleFetchStage", "PC, incrementer and universal instruction requester.");
    bindCompositeModule<RV32ISingleCycleDecodeStage>(m, "RV32ISingleCycleDecodeStage", "Instruction decode and architectural register file.");
    bindCompositeModule<RV32ISingleCycleExecuteStage>(m, "RV32ISingleCycleExecuteStage", "Shared ALU and concurrent branch target calculation.");
    bindCompositeModule<RV32ISingleCycleMemoryStage>(m, "RV32ISingleCycleMemoryStage", "Data requester and load/store lanes.");
    bindCompositeModule<RV32ISingleCycleWritebackStage>(m, "RV32ISingleCycleWritebackStage", "Combinational writeback selection.");
    bindCompositeModule<RV32ISingleCycleNextPCStage>(m, "RV32ISingleCycleNextPCStage", "Combinational next-PC selection.");
    bindCompositeModule<RV32ISingleCycleSystem>(m, "RV32ISingleCycleSystem", "Structural RV32I core with separate behavioral instruction and data memories.");
    bindCompositeModule<RV32IMemoryRequest>(m, "RV32IMemoryRequest", "CPU-owned universal requester with stable payload and redirect tracking.");
    bindCompositeModule<RV32IMemoryLanes>(m, "RV32IMemoryLanes", "Structural RV32I byte-lane routing, masks, alignment and extension.");

    bindCompositeModule<Adder8>(m, "Adder8", "8-bit adder.");
    bindCompositeModule<ZeroDetect8>(m, "ZeroDetect8", "8-bit zero detector.");
    bindCompositeModule<ALU8>(m, "ALU8", "8-bit arithmetic logic unit.");
    bindCompositeModule<ALU8ControlDecoder>(m, "ALU8ControlDecoder", "Structural ALU integration block.");
    bindCompositeModule<ALU8ResultSelector>(m, "ALU8ResultSelector", "Structural ALU integration block.");
    bindCompositeModule<ALU32ControlDecoder>(m, "ALU32ControlDecoder", "Structural ALU integration block.");
    bindCompositeModule<ALU32CompareUnit>(m, "ALU32CompareUnit", "Structural ALU integration block.");
    bindCompositeModule<ALU32ResultSelector>(m, "ALU32ResultSelector", "Structural ALU integration block.");
    bindCompositeModule<ALU32FlagUnit>(m, "ALU32FlagUnit", "Structural ALU integration block.");
    bindCompositeModule<AddSub8>(m, "AddSub8", "8-bit structural add/sub with independent carry input.");
    bindCompositeModule<ArithmeticUnit8>(m, "ArithmeticUnit8", "Shared 8-bit arithmetic unit.");
    bindCompositeModule<LogicUnit8>(m, "LogicUnit8", "8-bit structural logic unit.");
    bindCompositeModule<BarrelShifter8>(m, "BarrelShifter8", "8-bit variable shifts and rotations.");
    bindCompositeModule<CompareSelect8>(m, "CompareSelect8", "Predicates and min/max from shared subtraction.");
    bindCompositeModule<ALU8FlagUnit>(m, "ALU8FlagUnit", "8-bit ALU result and arithmetic flags.");
    bindCompositeModule<Adder32>(m, "Adder32", "Structural 32-bit adder.");
    bindCompositeModule<AddSub32>(m, "AddSub32", "Structural 32-bit add/subtract unit.");
    bindCompositeModule<Logic32>(m, "Logic32", "Structural 32-bit bitwise logic unit.");
    bindCompositeModule<ZeroDetect32>(m, "ZeroDetect32", "Structural 32-bit zero detector.");
    bindCompositeModule<Shifter32>(m, "Shifter32", "Structural 32-bit barrel shifter.");
    bindCompositeModule<ALU32>(m, "ALU32", "Structural RV32I-oriented 32-bit ALU.");

    py::enum_<Rewire::UnmappedBitValue>(m, "UnmappedBitValue")
        .value("UNKNOWN", Rewire::UnmappedBitValue::UNKNOWN)
        .value("LOW", Rewire::UnmappedBitValue::LOW)
        .value("HIGH", Rewire::UnmappedBitValue::HIGH);

    py::class_<Rewire::WireSpec>(m, "RewireWireSpec")
        .def(py::init<std::string, size_t>(), py::arg("name"), py::arg("width") = 1)
        .def_readwrite("name", &Rewire::WireSpec::name)
        .def_readwrite("width", &Rewire::WireSpec::width);

    py::class_<Rewire::BitMap>(m, "RewireBitMap")
        .def(py::init<std::string, size_t, std::string, size_t>(),
             py::arg("src_wire"), py::arg("src_bit"), py::arg("dst_wire"), py::arg("dst_bit"))
        .def_readwrite("src_wire", &Rewire::BitMap::src_wire)
        .def_readwrite("src_bit", &Rewire::BitMap::src_bit)
        .def_readwrite("dst_wire", &Rewire::BitMap::dst_wire)
        .def_readwrite("dst_bit", &Rewire::BitMap::dst_bit);

    py::class_<Rewire, BasicComponent, std::shared_ptr<Rewire>>(m, "Rewire", "Generic bit-level routing adapter.", py::module_local(false))
        .def(py::init([](std::string instance_name,
                         std::vector<Rewire::WireSpec> inputs,
                         std::vector<Rewire::WireSpec> outputs,
                         std::vector<Rewire::BitMap> mappings,
                         Rewire::UnmappedBitValue unmapped) {
            return Component::create<Rewire>(
                std::move(instance_name),
                std::move(inputs),
                std::move(outputs),
                std::move(mappings),
                unmapped);
        }),
             py::arg("name"), py::arg("inputs"), py::arg("outputs"), py::arg("mappings"),
             py::arg("unmapped") = Rewire::UnmappedBitValue::UNKNOWN)
        .def("get_name", &Rewire::getName)
        .def("get_parent", &Rewire::getParent)
        .def("get_children", &Rewire::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &Rewire::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &Rewire::getAllOutputPins, py::return_value_policy::reference_internal)
        .def("validate_mappings", &Rewire::validateMappings)
        .def("get_total_input_bits", &Rewire::getTotalInputBits)
        .def("get_total_output_bits", &Rewire::getTotalOutputBits)
        .def("get_input_specs", &Rewire::getInputSpecs)
        .def("get_output_specs", &Rewire::getOutputSpecs)
        .def("get_bit_mappings", &Rewire::getBitMappings)
        .def("get_unmapped_default", &Rewire::getUnmappedDefault);

    m.def("identity_mapping", &identity_mapping, py::arg("src_wire"), py::arg("src_offset"),
          py::arg("count"), py::arg("dst_wire"), py::arg("dst_offset") = 0);
    m.def("unpack_mapping", &unpack_mapping, py::arg("src_bus"), py::arg("width"), py::arg("dst_wires"));
    m.def("pack_mapping", &pack_mapping, py::arg("src_wires"), py::arg("dst_bus"));
    m.def("slice_mapping", &slice_mapping, py::arg("src_wire"), py::arg("start_bit"),
          py::arg("length"), py::arg("dst_wire"));
    m.def("sign_extend_mapping", &sign_extend_mapping, py::arg("src_wire"), py::arg("src_width"),
          py::arg("dst_wire"), py::arg("dst_width"));

    bindBasicModule<BitSplitter<1>>(m, "BitSplitter1", "Split one 1-bit input into one single-bit output.");
    bindBasicModule<BitJoiner<1>>(m, "BitJoiner1", "Join one single-bit input into one 1-bit output.");
    bindBasicModule<BitSplitter<2>>(m, "BitSplitter2", "Split one 2-bit input into two single-bit outputs.");
    bindBasicModule<BitJoiner<2>>(m, "BitJoiner2", "Join two single-bit inputs into one 2-bit output.");
    bindBasicModule<BitSplitter<3>>(m, "BitSplitter3", "Split one 3-bit input into three single-bit outputs.");
    bindBasicModule<BitJoiner<3>>(m, "BitJoiner3", "Join three single-bit inputs into one 3-bit output.");
    bindBasicModule<BitSplitter<4>>(m, "BitSplitter4", "Split one 4-bit input into four single-bit outputs.");
    bindBasicModule<BitJoiner<4>>(m, "BitJoiner4", "Join four single-bit inputs into one 4-bit output.");
    bindBasicModule<BitSplitter<8>>(m, "BitSplitter8", "Split one 8-bit input into eight single-bit outputs.");
    bindBasicModule<BitJoiner<8>>(m, "BitJoiner8", "Join eight single-bit inputs into one 8-bit output.");
    bindBasicModule<BitSplitter<5>>(m, "BitSplitter5", "Split one 5-bit input into five single-bit outputs.");
    bindBasicModule<BitJoiner<5>>(m, "BitJoiner5", "Join five single-bit inputs into one 5-bit output.");
    bindBasicModule<BitSplitter<16>>(m, "BitSplitter16", "Split one 16-bit input into sixteen single-bit outputs.");
    bindBasicModule<BitJoiner<16>>(m, "BitJoiner16", "Join sixteen single-bit inputs into one 16-bit output.");
    bindBasicModule<BitSplitter<32>>(m, "BitSplitter32", "Split one 32-bit input into thirty-two single-bit outputs.");
    bindBasicModule<BitJoiner<32>>(m, "BitJoiner32", "Join thirty-two single-bit inputs into one 32-bit output.");
    bindConstantModule<ConstantValue<1>>(m, "ConstantValue1", "Single-bit constant source.");
    bindConstantModule<ConstantValue<2>>(m, "ConstantValue2", "2-bit constant source.");
    bindConstantModule<ConstantValue<4>>(m, "ConstantValue4", "4-bit constant source.");
    bindConstantModule<ConstantValue<8>>(m, "ConstantValue8", "8-bit constant source.");
    bindConstantModule<ConstantValue<32>>(m, "ConstantValue32", "32-bit constant source.");
}
