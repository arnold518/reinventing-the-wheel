#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
namespace circuit {class ComponentCatalog;}
#include <bit>
namespace circuit::word {
const ComponentFamily& mux(size_t inputs,size_t width);
void registerMuxes(ComponentCatalog&);
class WordMux : public IOComponent {
    size_t inputs_, width_; std::string type_;
public:
    WordMux(std::string, size_t inputs, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
};
}
template<size_t Inputs, size_t W> class Mux : public circuit::word::WordMux {
    static_assert(Inputs == 2 || Inputs == 4 || Inputs == 8 || Inputs == 16 || Inputs == 32);
    static_assert(W == 1 || W == 4 || W == 8 || W == 16 || W == 32);
public:
    static const circuit::ComponentFamily& family() { return circuit::word::mux(Inputs,W); }
    explicit Mux(std::string name) : WordMux(std::move(name), Inputs, W) {}
};
