#pragma once

#include <string>
#include <vector>
#include <memory>
#include "ForwardDeclarations.hpp"

// Forward declarations
class ComponentBuilder;

/**
 * @brief Fluent API for building wires with a chainable interface.
 *
 * Usage example:
 *   builder.wire("A_internal")
 *       .fromInput("A")
 *       .to<XORGate>("XOR1", "A")
 *       .to<ANDGate>("AND1", "A");
 *
 * The wire is automatically built when the WireBuilder is destroyed,
 * or you can call .build() explicitly.
 */
class WireBuilder {
private:
    ComponentBuilder* builder_;
    std::string wire_name_;
    std::shared_ptr<Pin<>> source_pin_;
    std::vector<std::shared_ptr<Pin<>>> sink_pins_;
    bool built_ = false;

public:
    /**
     * @brief Construct a WireBuilder for a named wire
     * @param builder The ComponentBuilder context
     * @param name The wire name
     */
    WireBuilder(ComponentBuilder* builder, std::string name);

    // Disable copy (we own state)
    WireBuilder(const WireBuilder&) = delete;
    WireBuilder& operator=(const WireBuilder&) = delete;

    // Enable move
    WireBuilder(WireBuilder&& other) noexcept;
    WireBuilder& operator=(WireBuilder&& other) noexcept;

    /**
     * @brief Destructor - auto-commits the wire if not already built
     */
    ~WireBuilder();

    // ========== Source Specification ==========

    /**
     * @brief Set source to a specific pin
     * @param pin The source pin
     */
    WireBuilder& from(std::shared_ptr<Pin<>> pin);

    /**
     * @brief Set source to a child component's output pin
     * @tparam CompType The component type
     * @param comp_name The component instance name
     * @param pin_name The output pin name
     */
    template<typename CompType>
    WireBuilder& from(const std::string& comp_name, const std::string& pin_name);

    /**
     * @brief Set source to this component's input pin (at boundary)
     * @param pin_name The input pin name
     */
    WireBuilder& fromInput(const std::string& pin_name);

    /**
     * @brief Set source to this component's output pin (for internal→output wiring)
     * @param pin_name The output pin name
     */
    WireBuilder& fromOutput(const std::string& pin_name);

    // ========== Sink Specification ==========

    /**
     * @brief Add a sink pin
     * @param pin The sink pin
     */
    WireBuilder& to(std::shared_ptr<Pin<>> pin);

    /**
     * @brief Add a child component's input pin as a sink
     * @tparam CompType The component type
     * @param comp_name The component instance name
     * @param pin_name The input pin name
     */
    template<typename CompType>
    WireBuilder& to(const std::string& comp_name, const std::string& pin_name);

    /**
     * @brief Add this component's output pin as a sink (at boundary)
     * @param pin_name The output pin name
     */
    WireBuilder& toOutput(const std::string& pin_name);

    /**
     * @brief Add this component's input pin as a sink (for input→input passthrough)
     * @param pin_name The input pin name
     */
    WireBuilder& toInput(const std::string& pin_name);

    // ========== Build ==========

    /**
     * @brief Explicitly build the wire and return it
     * @return The created wire
     */
    std::shared_ptr<Wire<>> build();
};

// Template implementations must be in header or separate .tpp
#include "WireBuilder.tpp"
