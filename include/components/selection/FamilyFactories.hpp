#pragma once

#include "components/Component.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <memory>
#include <string>

namespace circuit {
// The usual structural factory of a family: build class T through the
// context-aware creator so pins, internals and metadata are set up.
template<class T>
FamilyFactory structuralFactory() {
    return [](const std::string& name, const std::shared_ptr<BuildContext>& context)
               -> std::shared_ptr<Component> {
        return Component::createWithContext<T>(context, name);
    };
}
} // namespace circuit
