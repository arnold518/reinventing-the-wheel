#pragma once
#include "modules/rv32i/RV32ISingleCycleStages.hpp"

namespace rv32i::single_cycle {
enum class Stage { Fetch, Decode, Execute, Memory, Writeback, NextPC };
const circuit::ComponentFamily& family(Stage);
std::shared_ptr<IOComponent> directStage(Stage, const std::string&,
    const std::shared_ptr<circuit::BuildContext>&);
}
