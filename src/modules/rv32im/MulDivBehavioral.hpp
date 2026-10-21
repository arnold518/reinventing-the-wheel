#pragma once
#include "components/selection/ComponentFamily.hpp"
namespace rv32im::detail {
std::shared_ptr<Component> createMulDivBehavioral(const std::string&,
    const std::shared_ptr<circuit::BuildContext>&);
}
