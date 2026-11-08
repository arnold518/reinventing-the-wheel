#include "modules/basic/WordGate.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Rewire.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace circuit::word {
namespace {
struct KindInfo {
    const char* name;  // gate and type prefix: AND -> ANDGate<W>, children AND_<i>
    const char* id;    // contract segment: logic.and.<W>
    bool unary;
    circuit::timing::Leaf leaf;
};
KindInfo info(GateKind kind) {
    using circuit::timing::Leaf;
    switch (kind) {
    case GateKind::And: return {"AND", "and", false, Leaf::And};
    case GateKind::Nand: return {"NAND", "nand", false, Leaf::Nand};
    case GateKind::Or: return {"OR", "or", false, Leaf::Or};
    case GateKind::Nor: return {"NOR", "nor", false, Leaf::Nor};
    case GateKind::Xor: return {"XOR", "xor", false, Leaf::Xor};
    case GateKind::Not: return {"NOT", "not", true, Leaf::Not};
    }
    throw std::invalid_argument("Invalid word gate kind");
}
std::string typeName(GateKind kind, size_t width) {
    return std::string(info(kind).name) + "Gate<" + std::to_string(width) + ">";
}
void gatePins(IOComponent* p, GateKind kind, size_t width) {
    if (!gateWidth(width)) throw std::invalid_argument("Word gates support 2 to 32 bits");
    if (info(kind).unary) p->addPinDynamic("IN", PinType::INPUT, width);
    else { p->addPinDynamic("A", PinType::INPUT, width); p->addPinDynamic("B", PinType::INPUT, width); }
    p->addPinDynamic("OUT", PinType::OUTPUT, width);
}

// The compact form: the one-bit gate rule on every bit, on the packed word.
class WordGateBehavioral final : public BasicComponent {
    GateKind kind_;
    size_t width_;
    std::string type_;
    std::shared_ptr<PinBase> a_, b_, out_;
    bool resolved_ = false;
protected:
    void pinsChanged() noexcept override { resolved_ = false; a_.reset(); b_.reset(); out_.reset(); }
public:
    WordGateBehavioral(std::string name, GateKind kind, size_t width)
        : BasicComponent(std::move(name), circuit::timing::leafDelay(info(kind).leaf),
              [kind, width](IOComponent* p) { gatePins(p, kind, width); }),
          kind_(kind), width_(width), type_(typeName(kind, width)) {}
    const char* getTypeName() const override { return type_.c_str(); }
    StateSupport stateSupport() const override { return StateSupport::Stateless; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override {
        namespace packed = circuit::logic_values::packed;
        if (!resolved_) {
            const bool unary = kind_ == GateKind::Not;
            a_ = getInputPinDynamic(unary ? "IN" : "A");
            b_ = unary ? nullptr : getInputPinDynamic("B");
            out_ = getOutputPinDynamic("OUT");
            resolved_ = true;
        }
        // A missing input reads as UNKNOWN, as an unconnected pin does.
        const auto read = [this](const std::shared_ptr<PinBase>& pin) {
            return pin && pin->getWidth() == width_ ? pin->packed() : PackedLogic::allUnknown(width_);
        };
        const PackedLogic a = read(a_);
        PackedLogic out;
        switch (kind_) {
        case GateKind::Not: out = packed::notGate(a, width_); break;
        case GateKind::And: out = packed::andGate(a, read(b_), width_); break;
        case GateKind::Nand: out = packed::notGate(packed::andGate(a, read(b_), width_), width_); break;
        case GateKind::Or: out = packed::orGate(a, read(b_), width_); break;
        case GateKind::Nor: out = packed::notGate(packed::orGate(a, read(b_), width_), width_); break;
        case GateKind::Xor: out = packed::xorGate(a, read(b_), width_); break;
        }
        if (out_ && out_->getWidth() == width_) _updateOutputPacked(simulator, out_, out, current_time);
    }
};

std::shared_ptr<Rewire> bitSplit(ComponentBuilder& b, const std::string& name, size_t width) {
    std::vector<Rewire::WireSpec> outs;
    std::vector<Rewire::BitMap> map;
    for (size_t i = 0; i < width; ++i) {
        outs.emplace_back("B" + std::to_string(i), 1);
        map.emplace_back("IN", i, "B" + std::to_string(i), 0);
    }
    return b.addNewComponent<Rewire>(name, std::vector<Rewire::WireSpec>{{"IN", width}}, outs, map);
}
} // namespace

WordGate::WordGate(std::string name, GateKind kind, size_t width)
    : IOComponent(std::move(name), [kind, width](IOComponent* p) { gatePins(p, kind, width); }),
      kind_(kind), width_(width), type_(typeName(kind, width)) {}

void WordGate::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    const auto& kind = info(kind_);
    // Zero-delay bit Rewires, so the word gate times exactly like its gates.
    std::vector<Rewire::WireSpec> ins;
    std::vector<Rewire::BitMap> map;
    for (size_t i = 0; i < width_; ++i) {
        ins.emplace_back("B" + std::to_string(i), 1);
        map.emplace_back("B" + std::to_string(i), 0, "OUT", i);
    }
    auto join = b.addNewComponent<Rewire>("OUT_WORD", ins, std::vector<Rewire::WireSpec>{{"OUT", width_}}, map);
    w.connect(join->getOutputPinDynamic("OUT"), getOutputPinDynamic("OUT"));
    auto first = bitSplit(b, kind.unary ? "IN_BITS" : "A_BITS", width_);
    w.connect(getInputPinDynamic(kind.unary ? "IN" : "A"), first->getInputPinDynamic("IN"));
    std::shared_ptr<Rewire> second;
    if (!kind.unary) {
        second = bitSplit(b, "B_BITS", width_);
        w.connect(getInputPinDynamic("B"), second->getInputPinDynamic("IN"));
    }
    for (size_t i = 0; i < width_; ++i) {
        const auto bit = "B" + std::to_string(i);
        const auto name = std::string(kind.name) + "_" + std::to_string(i);
        circuit::detail::StructuralNetBuilder::PinPtr out;
        auto x = first->getOutputPin(bit);
        switch (kind_) {
        case GateKind::And: out = w.binary<ANDGate>(name, x, second->getOutputPin(bit)); break;
        case GateKind::Nand: out = w.binary<NANDGate>(name, x, second->getOutputPin(bit)); break;
        case GateKind::Or: out = w.binary<ORGate>(name, x, second->getOutputPin(bit)); break;
        case GateKind::Nor: out = w.binary<NORGate>(name, x, second->getOutputPin(bit)); break;
        case GateKind::Xor: out = w.binary<XORGate>(name, x, second->getOutputPin(bit)); break;
        case GateKind::Not: out = w.invert(name, x); break;
        }
        w.connect(out, join->getInputPin(bit));
    }
    w.finish();
}

const ComponentFamily& gate(GateKind kind, size_t width) {
    if (!gateWidth(width)) throw std::invalid_argument("Word gates support 2 to 32 bits");
    struct Entry {
        std::string id, type;
        ComponentFamily family;
        Entry(GateKind k, size_t w)
            : id(std::string("logic.") + info(k).id + "." + std::to_string(w)), type(typeName(k, w)),
              family(id, type, [k, w](IOComponent* p) { gatePins(p, k, w); },
                  [k, w](const std::string& name, const std::shared_ptr<BuildContext>& c) {
                      return Component::createWithContext<WordGate>(c, name, k, w);
                  },
                  [k, w](const std::string& name, const std::shared_ptr<BuildContext>& c) -> std::shared_ptr<Component> {
                      return Component::createWithContext<WordGateBehavioral>(c, name, k, w);
                  }) {}
    };
    static std::map<std::pair<int, size_t>, std::unique_ptr<Entry>> entries;
    auto& entry = entries[{static_cast<int>(kind), width}];
    if (!entry) entry = std::make_unique<Entry>(kind, width);
    return entry->family;
}

void registerGates(ComponentCatalog& catalog) {
    for (size_t width = 2; width <= 32; ++width)
        for (auto kind : {GateKind::And, GateKind::Nand, GateKind::Or, GateKind::Nor, GateKind::Xor, GateKind::Not}) {
            const auto& f = gate(kind, width);
            circuit::registration::registerStructural(catalog, f, "WordGateSweepTest");
            circuit::registration::registerBehavioral(catalog, f, "WordGateSweepTest", "WordGateSweepTest", {},
                                                      "stable-after-settle");
        }
}
} // namespace circuit::word
