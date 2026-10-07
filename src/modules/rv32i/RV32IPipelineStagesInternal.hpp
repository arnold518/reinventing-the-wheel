#pragma once

#include "components/ComponentBuilder.hpp"
#include "modules/utility/Rewire.hpp"
#include <memory>
#include <string>
#include <vector>

// Private to the advanced-pipeline stage translation units.
namespace rv32i::pipeline_stages {
// Packed-field Rewire helpers shared by the structural stages.
void addControlPack(ComponentBuilder& builder, const char* name);
void addControlUnpack(ComponentBuilder& builder, const char* name);
void addAddressPack(ComponentBuilder& builder, const char* name);
void addAddressUnpack(ComponentBuilder& builder, const char* name);
void addStatusPack(
    ComponentBuilder& builder,
    const char* name,
    const std::vector<std::string>& fields);
void addStatusUnpack(
    ComponentBuilder& builder,
    const char* name,
    const std::vector<std::string>& fields);
std::shared_ptr<Rewire> addPassThrough(
    ComponentBuilder& builder,
    const char* name,
    std::vector<Rewire::WireSpec> fields);

// Behavioral implementations selected by the stage families.
using Context = std::shared_ptr<circuit::BuildContext>;
std::shared_ptr<Component> directFetchStage(const std::string& name, const Context& context, bool external_next_pc);
std::shared_ptr<Component> directDecodeStage(const std::string& name, const Context& context);
std::shared_ptr<Component> directExecuteStage(const std::string& name, const Context& context);
std::shared_ptr<Component> directMemoryStage(const std::string& name, const Context& context);
std::shared_ptr<Component> directWritebackStage(const std::string& name, const Context& context);
std::shared_ptr<Component> directExecutionStatus(const std::string& name, const Context& context);
} // namespace rv32i::pipeline_stages
