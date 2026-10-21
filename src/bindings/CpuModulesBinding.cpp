#include "ModuleBindingHelpers.hpp"
#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "modules/rv32i/RV32IMemoryRequest.hpp"

void bindCpuModules(py::module_& m) {
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


}
