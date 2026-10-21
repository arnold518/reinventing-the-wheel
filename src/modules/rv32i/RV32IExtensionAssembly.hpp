#pragma once
#include "components/selection/ComponentFamily.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include <functional>
namespace rv32i::detail {
struct FiveStageParts {
    std::shared_ptr<IOComponent> retirement, decode, writeback, status;
};
struct FiveStageNodes {
    std::shared_ptr<IOComponent> execute, coordinator, memory, ex_mem;
    FiveStageParts parts;
};
struct FiveStageFamilies {
    const circuit::ComponentFamily& decode;
    const circuit::ComponentFamily& execute;
    const circuit::ComponentFamily& coordinator;
    std::function<void(IOComponent&,ComponentBuilder&,circuit::detail::StructuralWiring&,const FiveStageNodes&)> connect = {};
};
FiveStageParts buildFiveStageBlueprint(IOComponent&,ComponentBuilder&,const PredictionConfig&,const FiveStageFamilies&);
RV32IArchitecturalState snapshotFiveStage(const FiveStageParts&,uint32_t observed_pc);
std::shared_ptr<IOComponent> buildDecodeBlueprint(IOComponent&,ComponentBuilder&,const circuit::ComponentFamily&);
std::shared_ptr<IOComponent> addForwardedOperands(ComponentBuilder&,const std::string&);
}
