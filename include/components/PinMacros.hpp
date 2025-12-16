#pragma once

#include "components/IOComponent.hpp"
#include "components/BasicComponent.hpp"
#include "basic/Pin.hpp"

/**
 * @file PinMacros.hpp
 * @brief Macros to simplify pin specification in component constructors.
 *
 * These macros reduce boilerplate when defining component pin interfaces.
 *
 * Usage example:
 * @code
 * // In FullAdder.cpp:
 * BEGIN_PINS(FullAdder, IOComponent)
 *     INPUT_PIN("A")
 *     INPUT_PIN("B")
 *     INPUT_PIN("Carry_in")
 *     OUTPUT_PIN("Sum")
 *     OUTPUT_PIN("Carry_out")
 * END_PINS()
 * @endcode
 *
 * This is equivalent to:
 * @code
 * FullAdder::FullAdder(std::string name)
 *     : IOComponent(std::move(name),
 *       [](IOComponent* self) {
 *           self->addPin("A", PinType::INPUT);
 *           self->addPin("B", PinType::INPUT);
 *           self->addPin("Carry_in", PinType::INPUT);
 *           self->addPin("Sum", PinType::OUTPUT);
 *           self->addPin("Carry_out", PinType::OUTPUT);
 *       })
 * {}
 * @endcode
 */

// ========== IOComponent Pin Macros ==========

/**
 * @brief Begin pin specification for an IOComponent-derived class.
 * @param component_class The component class name (e.g., FullAdder)
 * @param base_class The base class (IOComponent or BasicComponent)
 */
#define BEGIN_PINS(component_class, base_class) \
    component_class::component_class(std::string name) \
        : base_class(std::move(name), \
          [](IOComponent* self) {

/**
 * @brief Declare an input pin with the given name.
 * @param name The pin name (string literal)
 */
#define INPUT_PIN(name) \
            self->addPin(name, PinType::INPUT);

/**
 * @brief Declare an output pin with the given name.
 * @param name The pin name (string literal)
 */
#define OUTPUT_PIN(name) \
            self->addPin(name, PinType::OUTPUT);

/**
 * @brief End pin specification.
 */
#define END_PINS() \
          }) \
    {}


// ========== BasicComponent Pin Macros with Delay ==========

/**
 * @brief Begin pin specification for a BasicComponent-derived class.
 * @param component_class The component class name (e.g., ANDGate)
 * @param delay The gate delay in time units
 */
#define BEGIN_BASIC_PINS(component_class, delay) \
    component_class::component_class(std::string name) \
        : BasicComponent(std::move(name), delay, \
          [](IOComponent* self) {

/**
 * @brief End pin specification for BasicComponent.
 */
#define END_BASIC_PINS() \
          }) \
    {}


// ========== Alternative: Combined Pin + Delay Macro ==========

/**
 * @brief Define a complete BasicComponent constructor with pins and delay.
 *
 * Usage:
 * @code
 * DEFINE_GATE(ANDGate, 1,
 *     INPUT_PIN("A")
 *     INPUT_PIN("B")
 *     OUTPUT_PIN("OUT")
 * )
 * @endcode
 */
#define DEFINE_GATE(component_class, delay, ...) \
    component_class::component_class(std::string name) \
        : BasicComponent(std::move(name), delay, \
          [](IOComponent* self) { \
              __VA_ARGS__ \
          }) \
    {}
