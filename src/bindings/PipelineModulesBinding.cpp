#include "ModuleBindingHelpers.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"

void bindPipelineModules(py::module_& m) {
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

}
