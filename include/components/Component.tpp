#pragma once

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include <utility>

template<typename T, typename... Args>
std::shared_ptr<T> Component::create(Args&&... args) {
    // 1. Construct the component
    auto obj = std::make_shared<T>(std::forward<Args>(args)...);

    // 2. Create a builder scoped to the new component
    ComponentBuilder builder(obj);

    // 3. Build its internal hierarchy
    obj->buildInternals(builder);

    // 4. If it's an IOComponent, initialize its pins
    if (auto io_obj = std::dynamic_pointer_cast<IOComponent>(obj)) {
        io_obj->initPins(io_obj);
    }
    
    return obj;
}