#include "runtime/RecordedGuestFixtures.hpp"
#include "visualizer/VisualizerSession.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::invalid_argument("Usage: RecordedCircuitProbe <guest> <new-output.json>");
        const std::filesystem::path output=argv[2];
        if(std::filesystem::exists(output))throw std::invalid_argument("Output already exists");
        auto fixture=circuit::runtime::recordedGuestFixture(argv[1]);
        std::unique_ptr<circuit::visualizer::VisualizerSession> view;
        std::string topology,scopes;std::vector<std::string> checkpoints;
        size_t retained=0;
        fixture.options.progress=[](uint64_t cycles,uint64_t retired,uint64_t frames) {
            std::cerr<<cycles<<" cycles, "<<retired<<" retired, "<<frames<<" presented\n";
        };
        fixture.options.presentation_observer=[&](const auto& root,const Simulator& sim,SimulationTime time) {
            if(sim.isHistoryRecordingEnabled())throw std::logic_error("Circuit capture must keep history bounded");
            const auto before=sim.getPerformanceCounters().processed_events;
            const auto current=sim.getCurrentTime();const auto next=sim.nextEventTime();
            if(!view)view=std::make_unique<circuit::visualizer::VisualizerSession>(root,sim,"recorded-game-"+fixture.name);
            auto checkpoint=view->currentCheckpointJson(time);
            if(topology.empty()){topology=view->topologyResponseJson();scopes=view->topologySlicesJson();retained=topology.size()+scopes.size();}
            retained+=checkpoint.size();
            if(retained>24*1024*1024 || checkpoints.size()>=64)throw std::runtime_error("Circuit checkpoint byte/count budget exhausted");
            checkpoints.push_back(std::move(checkpoint));
            if(sim.getCurrentTime()!=current || sim.nextEventTime()!=next || sim.getPerformanceCounters().processed_events!=before)
                throw std::logic_error("Checkpoint observation advanced the simulator");
        };
        const auto result=circuit::runtime::runRecordedGuest(fixture.elf,fixture.input,fixture.options);
        auto recording=circuit::runtime::recordedGuestJson(result);
        recording.insert(1,"\"guest\":\""+fixture.name+"\",\"elfSha256\":\""+fixture.elf_sha256+"\",\"scriptSha256\":\""+fixture.script_sha256+"\",");
        if(checkpoints.empty() || checkpoints.size()!=result.frames.size())throw std::runtime_error("Missing presentation checkpoints");
        std::ofstream stream(output);if(!stream)throw std::runtime_error("Cannot create output");
        stream<<"{\"schemaVersion\":1,\"kind\":\"circuitsim-circuit-recording\",\"capturePolicy\":\"presentation-checkpoints\",\"sourceRuntime\":\"native\",\"topology\":"<<topology
              <<",\"topologySlices\":"<<scopes<<",\"checkpoints\":[";
        for(size_t i=0;i<checkpoints.size();++i){if(i)stream<<',';stream<<checkpoints[i];}
        stream<<"],\"recording\":"<<recording<<"}\n";stream.close();
        if(!stream)throw std::runtime_error("Cannot finish output");
        std::cerr<<"PASS "<<fixture.name<<": "<<checkpoints.size()<<" actual circuit checkpoints; "<<retained<<" retained JSON bytes\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
