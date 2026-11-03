#include "runtime/RecordedGuestFixtures.hpp"
#include <charconv>
#include <iostream>
int main(int argc,char** argv){
    try{
        if(argc<2||argc>9)throw std::invalid_argument("Usage: RecordedGuestProbe <guest|--list> [ff|cached-ff|cached-dram] [max-cycles] [quantum-events] [gate|logic|visual|compact|expanded-core|structural] [width] [height] [system]");
        const std::string guest=argv[1];if(guest=="--list"){for(const auto& n:circuit::runtime::recordedGuestNames())std::cout<<n<<'\n';return 0;}
        const std::string mode=argc>2?argv[2]:"ff";if(mode!="ff"&&mode!="cached-ff"&&mode!="cached-dram")throw std::invalid_argument("Invalid machine mode");
        size_t cycles=500000;if(argc>3){std::string_view value=argv[3];auto r=std::from_chars(value.data(),value.data()+value.size(),cycles);if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid cycle limit");}
        size_t quantum=100000;if(argc>4){std::string_view value=argv[4];auto r=std::from_chars(value.data(),value.data()+value.size(),quantum);if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid event quantum");}
        const std::string fidelity=argc>5?argv[5]:"compact";
        if(fidelity!="compact"&&fidelity!="expanded-core"&&fidelity!="structural"&&fidelity!="gate"&&fidelity!="logic"&&fidelity!="visual")throw std::invalid_argument("Invalid logic profile");
        size_t width=0,height=0;   // 0: the fixture's own screen
        auto dimension=[](const char* text){size_t value=0;std::string_view input=text;auto parsed=std::from_chars(input.data(),input.data()+input.size(),value);
            if(parsed.ec!=std::errc{}||parsed.ptr!=input.data()+input.size()||!value||value>4096)throw std::invalid_argument("Invalid surface dimension");return value;};
        if(argc>6)width=dimension(argv[6]);if(argc>7)height=dimension(argv[7]);
        std::cout<<circuit::runtime::runRecordedGuestFixtureJson(guest,mode!="ff",mode=="cached-dram",cycles,
            [](uint64_t c,uint64_t i,uint64_t f){std::cerr<<c<<" cycles, "<<i<<" retired, "<<f<<" presented\n";},quantum,fidelity=="expanded-core",fidelity!="structural",width,height,
            fidelity=="expanded-core"||fidelity=="structural"?"":fidelity,argc>8?argv[8]:"")<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
