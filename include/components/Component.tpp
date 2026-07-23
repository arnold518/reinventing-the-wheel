#pragma once

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuildContext.hpp"
#include <utility>

template<typename T, typename... Args>
std::shared_ptr<T> Component::create(Args&&... args) {
    return createWithContext<T>(nullptr, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
std::shared_ptr<T> Component::createWithContext(
    const std::shared_ptr<circuit::BuildContext>& context,
    Args&&... args) {
    // 1. Construct the component
    auto obj = std::make_shared<T>(std::forward<Args>(args)...);

    if (context) {
        context->applyMetadata(*obj);
    }

    // 2. FIRST, initialize the component's own interface (its pins).
    //    This makes pins like "A", "B", "Sum", etc., available for the next step.
    if (auto io_obj = std::dynamic_pointer_cast<IOComponent>(obj)) {
        io_obj->initPins(io_obj);
    }

    // 3. SECOND, create a builder scoped to the new component.
    ComponentBuilder builder(obj, context);

    // 4. FINALLY, build its internal hierarchy, which can now successfully
    //    connect to the now-existing pins of the component.
    obj->buildInternals(builder);

    if (context) {
        context->recordCreated(*obj);
    }
    
    return obj;
}
