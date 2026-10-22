#include "modules/rv32i/RV32IPipelineStages.hpp"
#include <string>
#include <vector>
#include <stdexcept>

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/storage/RegisterFile.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "RV32IPipelineStagesInternal.hpp"
#include "RV32IExtensionAssembly.hpp"

using namespace rv32i::pipeline_stages;

RV32IDecodeStage::RV32IDecodeStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IDecodeStage.pinInitializer()) {}

void RV32IDecodeStage::buildInternals(ComponentBuilder& builder) {
    register_file_=rv32i::detail::buildDecodeBlueprint(*this,builder,circuit::families::RV32IDecodeControl);
}

std::vector<std::vector<LogicValue>>
RV32IDecodeStage::getRegisterStateAtTime(SimulationTime time) const {
    const auto view =
        std::dynamic_pointer_cast<RegisterStateView>(register_file_);
    if (!view) {
        throw std::logic_error(
            "Selected decode-stage register file lacks observation");
    }
    return view->getRegisterStateAtTime(time);
}
