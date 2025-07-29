#include "components/BasicComponent.hpp"

BasicComponent::BasicComponent(std::string name, size_t delay_val)
    : IOComponent(std::move(name), delay_val) {}