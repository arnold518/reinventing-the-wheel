// Independent leaf truth checks and deterministic exact timing/value signatures.
#include "analysis/DelayTwin.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/TimingProfile.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using L = LogicValue;
uint64_t digest = 1469598103934665603ULL, cases = 0;
void mix(uint64_t value) { digest = (digest ^ value) * 1099511628211ULL; }
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
uint8_t code(L value) { return value == L::LOW ? 0 : value == L::HIGH ? 1 : 2; }

// Deliberately does not overwrite every output. The generic path must still
// receive fresh trial maps when the known concrete primitives reuse theirs.
class ProbeSelectivePrimitive final : public circuit::memory_device::Primitive {
public:
    explicit ProbeSelectivePrimitive(std::string name)
        : Primitive(std::move(name), "ProbeSelectivePrimitive", [](IOComponent* s) {
            s->addPin("IN", PinType::INPUT); s->addPin("OUT", PinType::OUTPUT);
        }) {}
    bool respondsOnlyToInputs() const override { return true; }
    bool settled(const Values& in, Values& out, Deciders&) const override {
        if (in.at("IN")[0] == L::HIGH) out["OUT"] = {L::LOW};
        return true;
    }
    void evaluate(SimulationTime, Simulator&) override {}
};

struct Fixture {
    std::shared_ptr<IOComponent> root;
    std::shared_ptr<const circuit::DelayTwin> twin;
};
template<class Part, class... Args>
Fixture fixture(Args... args) {
    Fixture f; f.root = Component::create<IOComponent>("TIMING_PROBE");
    ComponentBuilder builder(f.root);
    const auto part = builder.addNewComponent<Part>("LEAF", args...);
    for (const auto& [name,pin] : part->getAllInputPins()) {
        auto input = f.root->addPinDynamic(name, PinType::INPUT, pin->getWidth());
        builder.addNewWireDynamic("I_"+name, pin->getWidth(), input, {pin});
    }
    for (const auto& [name,pin] : part->getAllOutputPins()) {
        auto output = f.root->addPinDynamic(name, PinType::OUTPUT, pin->getWidth());
        builder.addNewWireDynamic("O_"+name, pin->getWidth(), pin, {output});
    }
    std::string why; f.twin = circuit::analysis::buildDelayTwin(f.root, &why);
    if (!f.twin) throw std::runtime_error("Cannot build probe twin: "+why);
    return f;
}

void check(const Fixture& f, const std::vector<std::vector<L>>& values,
           const std::vector<uint8_t>& expected, unsigned schedule, bool opaque = false) {
    std::vector<std::vector<long long>> changed;
    for (size_t pin=0; pin<values.size(); ++pin) {
        changed.emplace_back();
        for (size_t bit=0; bit<values[pin].size(); ++bit)
            changed.back().push_back(schedule == 0 ? -1 : schedule == 1 ? 17 :
                5+((bit*7+pin*11+schedule)%13));
    }
    std::vector<long long> times; std::vector<std::vector<uint8_t>> result;
    const auto state = f.twin->start();
    const bool answered = f.twin->settle(state.get(), 1000, values, changed, times, result);
    require(answered, "Pure leaf unexpectedly declined timing");
    require(result.size()==1 && result[0].size()==expected.size(), "Leaf output shape differs");
    for (size_t i=0; i<expected.size(); ++i)
        require((opaque && expected[i]>=2) ? result[0][i]>=2 : result[0][i]==expected[i],
                "Leaf output differs from independent truth rule");
    mix(answered); for (auto at:times) mix(uint64_t(at));
    for (const auto& word:result) for (auto bit:word) mix(bit);
    ++cases;
}

void buffers() {
    for (size_t width : {1u,8u,16u,32u}) {
        const auto f=fixture<circuit::memory_device::MemoryOutputBuffer>(width);
        for (unsigned pattern=0; pattern<64; ++pattern) for (unsigned enable=0; enable<4; ++enable) {
            std::vector<std::vector<L>> values;
            std::vector<L> data(width);
            for (size_t bit=0; bit<width; ++bit)
                data[bit]=L(pattern<16 ? ((pattern>>(bit%4))&1) : ((pattern+bit*3+(bit/3))%4));
            for (const auto& [name,w]:f.twin->inputs)
                values.push_back(name=="DATA" ? data : std::vector<L>(w,L(enable)));
            std::vector<uint8_t> expected(width,2);
            if (enable==1) for (size_t bit=0; bit<width; ++bit) expected[bit]=code(data[bit]);
            for (unsigned schedule=0; schedule<8; ++schedule) check(f,values,expected,schedule);
        }
    }
}

void columns() {
    for (size_t rows : {2u,4u,8u}) {
        const auto f=fixture<circuit::memory_device::StaticReadColumn>(rows);
        for (unsigned pattern=0; pattern<512; ++pattern) {
            std::vector<L> wl(rows),q(rows); uint64_t random=pattern+1;
            for (size_t i=0; i<rows; ++i) {
                random=random*6364136223846793005ULL+1442695040888963407ULL;
                wl[i]=L((random>>32)%(pattern<256 ? 2 : 4));
                q[i]=L((random>>40)%4);
            }
            // Explicit single-row selection, including unknown stored data.
            if (pattern<64) for (size_t i=0; i<rows; ++i) wl[i]=L(i==pattern%rows);
            size_t selected=0,count=0; bool binary=true;
            for (size_t i=0; i<rows; ++i) {
                binary=binary&&(wl[i]==L::LOW||wl[i]==L::HIGH);
                if (wl[i]==L::HIGH) {++count;selected=i;}
            }
            std::vector<uint8_t> expected{uint8_t(binary&&count==1 ? code(q[selected]) : 2)};
            std::vector<std::vector<L>> values;
            for (const auto& [name,width]:f.twin->inputs) {
                const size_t row=std::stoul(name.substr(name.find('_')+1));
                values.push_back({name.starts_with("WL_") ? wl[row] : q[row]});
            }
            for (unsigned schedule=0; schedule<8; ++schedule) check(f,values,expected,schedule);
        }
    }
}
}

int main() {
    try {
        for (const auto& profile:circuit::timing::timingProfileNames()) {
            circuit::timing::setActiveTimingProfile(profile);
            buffers(); columns();
            const auto fallback=fixture<ProbeSelectivePrimitive>();
            for (unsigned value=0; value<4; ++value) for (unsigned schedule=0; schedule<8; ++schedule)
                check(fallback,{{L(value)}},{uint8_t(value==1 ? 0 : 2)},schedule,true);
            std::cout<<profile<<" cases="<<cases<<" timing-value-digest="<<digest<<'\n';
        }
        std::cout<<"PASS independent four-state leaf truth, exact arrival signatures, never-changed inputs and selective primitive fallback\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
