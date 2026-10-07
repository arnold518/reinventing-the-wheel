#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/ComponentFamily.hpp"
#include <memory>
#include <string>
#include <utility>

namespace rv32i::detail {
// A profiled build selects the family by parameters; the context-free builder
// cannot forward them, so it constructs the structural class directly with the
// equivalent constructor arguments.
template<class Structural, class... DirectArgs>
std::shared_ptr<IOComponent> addFamilyOrDirect(
    ComponentBuilder& builder, const circuit::ComponentFamily& family,
    const std::string& name, circuit::ParameterMap parameters,
    DirectArgs&&... direct_args) {
    if (builder.getBuildContext()) {
        return builder.add(family, name, std::move(parameters));
    }
    return builder.addNewComponent<Structural>(
        name, std::forward<DirectArgs>(direct_args)...);
}

// Core instances use the compact packet schema of a pipeline register.
template<class Register>
std::shared_ptr<IOComponent> addCompactPipelineRegister(
    ComponentBuilder& builder, const circuit::ComponentFamily& family,
    const std::string& name) {
    return addFamilyOrDirect<Register>(
        builder, family, name, {{"compact_packet", "1"}}, true);
}
} // namespace rv32i::detail
