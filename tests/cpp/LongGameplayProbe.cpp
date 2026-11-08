#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"
#include <map>
#include "visualizer/VisualizerSession.hpp"
#include "TickTraceWriter.hpp"
#include <algorithm>
#include <filesystem>
#include <zlib.h>
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
//                     [--part-frames N] [--ticks] [--save-every CYCLES] [--save-at-frames]
//   LongGameplayProbe FIXTURE DIRECTORY --resume [same options]
//
// The timing profile is CIRCUITSIM_TIMING_PROFILE. The fidelity is a named
// preset (Compact: compact parts on their proven delays).
//
// --save-every writes a machine save (circuit::state) about every CYCLES
// cycles to saves/, for replaying the session in the browser;
// --save-at-frames writes one right after each frame (the first cycle after
// its PRESENT has settled), so a replay starts at the frame the reader sees. Every run also
// keeps resume.bin, a save with the results so far, taken each time a circuit
// part is written; --resume continues an interrupted run from it.
int main(int argc,char** argv) {
    try {
        if(argc<3)throw std::invalid_argument("Usage: LongGameplayProbe FIXTURE NEW-DIRECTORY [--period TICKS] [--fidelity compact|visual|logic|gate] [--part-frames N] [--ticks] [--save-every CYCLES] [--resume]");
        std::map<std::string,std::string> a{{"--period","1000"},{"--fidelity","compact"},{"--part-frames","16"},{"--save-every","0"}};
        bool ticks=false,resume=false,save_frames=false;
        for(int i=3;i<argc;++i){
            const std::string key=argv[i];
            if(key=="--ticks"){ticks=true;continue;}
            if(key=="--resume"){resume=true;continue;}
            if(key=="--save-at-frames"){save_frames=true;continue;}
            if(!a.contains(key)||i+1>=argc)throw std::invalid_argument("Unknown or incomplete option "+key);
            a[key]=argv[++i];
        }
        const size_t part_frames=std::stoull(a.at("--part-frames"));
        if(!part_frames||part_frames>32)throw std::invalid_argument("Invalid circuit part size");
        const std::filesystem::path directory=argv[2];
        if(!resume&&std::filesystem::exists(directory))throw std::invalid_argument("Output directory already exists");
        if(resume&&!std::filesystem::exists(directory/"resume.bin"))throw std::invalid_argument("Nothing to resume: no resume.bin in the directory");
        if(resume&&ticks)throw std::invalid_argument("A signal-trace run cannot resume");
        const uint64_t save_every=std::stoull(a.at("--save-every"));
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
        uint64_t resumed_cycle=0,kept_frame_saves=0;
        if(resume){
            std::ifstream state(directory/"resume.json");
            std::string word;state>>word>>parts>>word>>presentations>>word>>resumed_cycle;
            if(!state)throw std::runtime_error("Cannot read resume.json");
            // Replay saves after the resume point are made again.
            if(std::filesystem::exists(directory/"saves/index.jsonl")){
                std::ifstream index(directory/"saves/index.jsonl");std::string line,kept;
                while(std::getline(index,line)){
                    const auto at=line.find("\"cycle\":");
                    if(at!=std::string::npos&&std::stoull(line.substr(at+8))<resumed_cycle){
                        kept+=line+"\n";
                        const auto frame=line.find("\"frame\":");
                        if(frame!=std::string::npos)kept_frame_saves=std::max<uint64_t>(kept_frame_saves,std::stoull(line.substr(frame+8)));
                    }
                }
                std::ofstream(directory/"saves/index.jsonl",std::ios::trunc)<<kept;
            }
            std::ifstream file(directory/"resume.bin",std::ios::binary);
            options.resume=std::make_shared<const std::vector<uint8_t>>(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
            if(!file.eof()&&!file)throw std::runtime_error("Cannot read resume.bin");
            std::cerr<<"RESUME at part "<<parts<<", "<<presentations<<" presentations\n";
        }
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
        // Saves. A replay save at a frame comes first: it is taken at the first
        // cycle after the frame, where the replay of that frame starts. A
        // resume save is due when a full part waits to be written (the part is
        // written first, so the save and the files on disk agree); it waits a
        // cycle when a frame save takes this one. A periodic replay save that
        // falls on a resume save's cycle moves to the next.
        using Content=circuit::runtime::RecordedGuestOptions::SaveContent;
        uint64_t next_replay=save_every?(resumed_cycle/save_every+1)*save_every:0;
        const auto saves=directory/"saves";
        if(save_every||save_frames)std::filesystem::create_directories(saves);
        // Resumed: frame saves after the resume point are made again.
        uint64_t saved_frames=kept_frame_saves;
        options.save_at=[&](uint64_t cycle,uint64_t presented){
            if(save_frames&&presented>saved_frames){saved_frames=presented;return Content::Machine;}
            if(checkpoints.size()==part_frames){flush();return Content::WithResults;}
            if(save_every&&cycle>=next_replay){next_replay=(cycle/save_every+1)*save_every;return Content::Machine;}
            return Content::None;
        };
        options.on_save=[&](uint64_t cycle,SimulationTime settled,Content content,std::vector<uint8_t> bytes){
            const auto put=[&](const std::filesystem::path& path){
                std::ofstream file(path.string()+".tmp",std::ios::binary);
                file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();
                if(!file)throw std::runtime_error("Cannot write "+path.string());
                std::filesystem::rename(path.string()+".tmp",path);
            };
            if(content==Content::WithResults){
                put(directory/"resume.bin");
                std::ofstream state(directory/"resume.json.tmp");state<<"parts "<<parts<<" presentations "<<presentations<<" cycle "<<cycle<<"\n";state.close();
                if(!state)throw std::runtime_error("Cannot write resume.json");
                std::filesystem::rename(directory/"resume.json.tmp",directory/"resume.json");
                return;
            }
            // Replay saves are written gzip-compressed (about 25 times smaller).
            const auto name="save-"+std::to_string(cycle)+".bin.gz";
            {
                const auto path=saves/name;const auto temporary=path.string()+".tmp";
                gzFile file=gzopen(temporary.c_str(),"wb9");
                if(!file||gzwrite(file,bytes.data(),static_cast<unsigned>(bytes.size()))!=static_cast<int>(bytes.size())||gzclose(file)!=Z_OK)
                    throw std::runtime_error("Cannot write "+path.string());
                std::filesystem::rename(temporary,path);
            }
            std::ofstream index(saves/"index.jsonl",std::ios::app);
            index<<"{\"cycle\":"<<cycle<<",\"settled\":"<<settled<<",\"frame\":"<<presentations<<",\"file\":\""<<name<<"\",\"bytes\":"<<bytes.size()<<"}\n";
            if(!index)throw std::runtime_error("Cannot append the save index");
        };
        options.progress=[](uint64_t cycles,uint64_t retired,uint64_t frames){
            std::cerr<<cycles<<" cycles, "<<retired<<" retired, "<<frames<<" presented\n";
        };
        options.presentation_observer=[&](const auto& root,const Simulator& sim,SimulationTime time){
            if(sim.isHistoryRecordingEnabled())throw std::logic_error("History must remain off");
            const auto before=sim.getPerformanceCounters().processed_events;
            const auto now=sim.getCurrentTime();const auto next=sim.nextEventTime();
            // A resumed run has its topology on disk already but still needs the view.
            if(!view)view=std::make_unique<circuit::visualizer::VisualizerSession>(root,sim,"long-gameplay");
            if(!std::filesystem::exists(directory/"topology.json")){
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
