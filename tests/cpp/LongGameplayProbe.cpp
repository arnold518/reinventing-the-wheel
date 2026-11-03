#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"
#include <map>
#include "visualizer/VisualizerSession.hpp"
#include "TickTraceWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

// Native equipment for long games: a long fixture (its ELF and its computer
// player's batches, one after each frame) runs through the checked runner and
// its circuit is written in parts for a playlist. No game rule runs here.
//
//   LongGameplayProbe FIXTURE NEW-DIRECTORY [--period TICKS] [--fidelity compact|visual|logic|gate]
//                     [--part-frames N] [--ticks]
//
// The timing profile is CIRCUITSIM_TIMING_PROFILE. The fidelity is a named
// preset (Compact: compact parts on their proven delays).
int main(int argc,char** argv) {
    try {
        if(argc<3)throw std::invalid_argument("Usage: LongGameplayProbe FIXTURE NEW-DIRECTORY [--period TICKS] [--fidelity compact|visual|logic|gate] [--part-frames N] [--ticks]");
        std::map<std::string,std::string> a{{"--period","1000"},{"--fidelity","compact"},{"--part-frames","16"}};
        bool ticks=false;
        for(int i=3;i<argc;++i){
            const std::string key=argv[i];
            if(key=="--ticks"){ticks=true;continue;}
            if(!a.contains(key)||i+1>=argc)throw std::invalid_argument("Unknown or incomplete option "+key);
            a[key]=argv[++i];
        }
        const size_t part_frames=std::stoull(a.at("--part-frames"));
        if(!part_frames||part_frames>32)throw std::invalid_argument("Invalid circuit part size");
        const std::filesystem::path directory=argv[2];
        if(std::filesystem::exists(directory))throw std::invalid_argument("Output directory already exists");
        auto fixture=circuit::runtime::recordedGuestFixture(argv[1]);
        if(fixture.options.presentation_inputs.empty())throw std::invalid_argument("Not a long fixture: "+fixture.name);
        const SimulationTime period=std::stoull(a.at("--period"));
        if(period<100||period%2)throw std::invalid_argument("The period must be even and at least 100 ticks");
        circuit::runtime::retime(fixture,period);
        auto& options=fixture.options;
        // A named preset, so the recording carries the profile the visualizer
        // offers (Compact is the one the short versions open in).
        options.fidelity_profile=a.at("--fidelity");
        const auto& elf=fixture.elf;
        std::filesystem::create_directories(directory);
        std::unique_ptr<circuit::visualizer::VisualizerSession> view;
        std::shared_ptr<TickTraceWriter> tick_trace;
        if(ticks) options.signal_trace=[&](const auto& root,const Simulator& sim) {
            view=std::make_unique<circuit::visualizer::VisualizerSession>(root,sim,"long-gameplay");
            tick_trace=std::make_shared<TickTraceWriter>(directory,*view);
            return tick_trace;
        };
        std::vector<std::string> checkpoints;
        size_t retained=0,parts=0,presentations=0;
        auto flush=[&]{
            if(checkpoints.empty())return;
            const auto path=directory/("part-"+std::to_string(parts++)+".partial.json");
            std::ofstream stream(path.string()+".tmp");
            stream<<"{\"schemaVersion\":2,\"topologyRef\":\"graph.json\",\"checkpoints\":[";
            for(size_t i=0;i<checkpoints.size();++i){if(i)stream<<',';stream<<checkpoints[i];}
            stream<<"]}\n";stream.close();if(!stream)throw std::runtime_error("Cannot write circuit part");
            std::filesystem::rename(path.string()+".tmp",path);
            checkpoints.clear();retained=0;
            std::cerr<<"PART "<<parts<<" presentations "<<presentations<<'\n';
        };
        options.progress=[](uint64_t cycles,uint64_t retired,uint64_t frames){
            std::cerr<<cycles<<" cycles, "<<retired<<" retired, "<<frames<<" presented\n";
        };
        options.presentation_observer=[&](const auto& root,const Simulator& sim,SimulationTime time){
            if(sim.isHistoryRecordingEnabled())throw std::logic_error("History must remain off");
            const auto before=sim.getPerformanceCounters().processed_events;
            const auto now=sim.getCurrentTime();const auto next=sim.nextEventTime();
            if(!std::filesystem::exists(directory/"topology.json")){
                if(!view)view=std::make_unique<circuit::visualizer::VisualizerSession>(root,sim,"long-gameplay");
                std::ofstream topology_file(directory/"topology.json");
                const auto topology=view->topologyResponseJson();
                if(topology.size()>32*1024*1024)throw std::runtime_error("Topology header exceeds byte budget");
                topology_file<<topology<<'\n';topology_file.close();
                if(!topology_file)throw std::runtime_error("Cannot write topology header");
                size_t scope_number=0;
                view->forEachTopologySlice([&](const std::string& slice){
                    if(slice.size()>32*1024*1024)throw std::runtime_error("Topology scope exceeds byte budget");
                    std::ofstream file(directory/("scope-"+std::to_string(scope_number++)+".json"));
                    file<<slice<<'\n';file.close();if(!file)throw std::runtime_error("Cannot write topology scope");
                });
            }
            if(tick_trace)tick_trace->presentation(time);
            auto checkpoint=view->currentCheckpointJson(time);
            if(checkpoints.size()>=part_frames||retained+checkpoint.size()>24*1024*1024)flush();
            if(retained+checkpoint.size()>24*1024*1024)throw std::runtime_error("Circuit part exceeds byte limit");
            retained+=checkpoint.size();checkpoints.push_back(std::move(checkpoint));++presentations;
            if(sim.getCurrentTime()!=now||sim.nextEventTime()!=next||sim.getPerformanceCounters().processed_events!=before)
                throw std::logic_error("Observation advanced simulation");
        };
        const auto result=circuit::runtime::runRecordedGuest(elf,fixture.input,options);flush();
        if(tick_trace)tick_trace->finish();
        if(presentations!=result.frames.size())throw std::runtime_error("A presented frame was not retained");
        std::ofstream result_file(directory/"recording.json");
        auto json=circuit::runtime::recordedGuestJson(result);
        json.insert(1,"\"guest\":\""+fixture.name+"\",\"system\":\""+options.system+"\",\"timingProfile\":\""+
            circuit::timing::activeTimingProfile().name+"\",\"elfSha256\":\""+fixture.elf_sha256+"\",\"scriptSha256\":\""+fixture.script_sha256+"\",");
        result_file<<json<<'\n';result_file.close();
        if(!result_file)throw std::runtime_error("Cannot write recording");
        std::cerr<<"PASS "<<presentations<<" frames in "<<parts<<" circuit parts\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
