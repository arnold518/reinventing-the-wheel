#pragma once

#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include <array>
#include <map>

namespace rv32i::baseline {
enum class Block { FetchStage, DecodeStage, ExecuteStage, MemoryStage, DependencyUnit, Control };
const circuit::ComponentFamily& family(Block block);
std::shared_ptr<Component> direct(Block block, const std::string& name,
    const std::shared_ptr<circuit::BuildContext>& context);
std::shared_ptr<Component> directCore(const std::string& name,
    const std::shared_ptr<circuit::BuildContext>& context);

inline const std::vector<std::string> fetchFields{"PC","INSTRUCTION","FETCH_STATUS"};
inline const std::vector<std::string> decodeFields{"PC","INSTRUCTION","RS1_VALUE","RS2_VALUE",
    "IMMEDIATE","CONTROL","REGISTER_ADDRESSES","FETCH_STATUS"};
inline const std::vector<std::string> executeFields{"PC","INSTRUCTION","ALU_RESULT","STORE_DATA",
    "PC_PLUS_4","NEXT_PC","CONTROL","REGISTER_ADDRESSES","FETCH_STATUS","EXECUTION_STATUS"};
inline const std::vector<std::string> memoryFields{"PC","INSTRUCTION","ALU_RESULT","MEMORY_DATA","STORE_DATA",
    "PC_PLUS_4","NEXT_PC","CONTROL","REGISTER_ADDRESSES","FETCH_STATUS","EXECUTION_STATUS","MEMORY_STATUS"};

// Independent binary settled-value equations used by compact stage models.
// Sequential state (PC, register file, pipeline packets, retirement) is kept by
// the caller. This is not the architectural instruction oracle.
using Values = std::map<std::string,uint32_t>;
Values decodePacket(const Values& in, const std::array<uint32_t,32>& registers);
Values executePacket(const Values& in);
Values memoryPacket(const Values& in);
Values dependency(const Values& in);
Values control(const Values& in);
}
