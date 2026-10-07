#pragma once

#include "components/Component.hpp"
#include <memory>
#include <string>

// Private to the advanced-pipeline control translation units: behavioral
// implementations selected by the control-unit families.
namespace rv32i::pipeline_control {
using Context = std::shared_ptr<circuit::BuildContext>;
std::shared_ptr<Component> directForwardingUnit(const std::string& name, const Context& context);
std::shared_ptr<Component> directHazardDetectionUnit(const std::string& name, const Context& context);
std::shared_ptr<Component> directMemoryAlignmentUnit(const std::string& name, const Context& context);
std::shared_ptr<Component> directPipelineRetirementUnit(const std::string& name, const Context& context);
std::shared_ptr<Component> directPipelineCoordinator(const std::string& name, const Context& context);
} // namespace rv32i::pipeline_control
