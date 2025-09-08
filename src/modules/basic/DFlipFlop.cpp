#include "modules/basic/DFlipFlop.hpp"

DFlipFlop::DFlipFlop(std::string name) : BasicComponent(std::move(name), 3),
                               current_q_state(LogicValue::UNKNOWN),
                               current_q_bar_state(LogicValue::UNKNOWN),
                               prev_clk_state(LogicValue::UNKNOWN)
{}

void DFlipFlop::initPins(std::shared_ptr<IOComponent> self_ptr) {
    _addPin("D", PinType::INPUT, self_ptr);
    _addPin("CLK", PinType::INPUT, self_ptr);
    _addPin("RST", PinType::INPUT, self_ptr);
    _addPin("Q", PinType::OUTPUT, self_ptr);
    _addPin("Q_BAR", PinType::OUTPUT, self_ptr);
}

void DFlipFlop::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue d_input = getInputValue("D");
    LogicValue clk_input = getInputValue("CLK");
    LogicValue rst_input = getInputValue("RST");

    LogicValue next_q_state = current_q_state;

    if (rst_input == LogicValue::HIGH) {
        next_q_state = LogicValue::LOW;
    } 
    else if (clk_input == LogicValue::HIGH && prev_clk_state == LogicValue::LOW) {
         if (d_input == LogicValue::HIGH || d_input == LogicValue::LOW) {
            next_q_state = d_input;
        } else {
            next_q_state = LogicValue::UNKNOWN;
        }
    }
    
    LogicValue next_q_bar_state = (next_q_state == LogicValue::HIGH) ? LogicValue::LOW : 
                                  ((next_q_state == LogicValue::LOW) ? LogicValue::HIGH : LogicValue::UNKNOWN);

    if (next_q_state != current_q_state) {
        current_q_state = next_q_state;
        _updateOutputWire(simulator, "Q", current_q_state, current_time);
    }
    if (next_q_bar_state != current_q_bar_state) {
        current_q_bar_state = next_q_bar_state;
        _updateOutputWire(simulator, "Q_BAR", current_q_bar_state, current_time);
    }
    
    prev_clk_state = clk_input;
}