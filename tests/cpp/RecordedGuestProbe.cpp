#include "runtime/RecordedGuestFixtures.hpp"
#include <charconv>
#include <iostream>
int main(int argc,char** argv){
    try{
        if(argc<2||argc>6)throw std::invalid_argument("Usage: RecordedGuestProbe <guest|--list> [ff|cached-ff|cached-dram] [max-cycles] [quantum-events] [compact|expanded-core|structural]");
        const std::string guest=argv[1];if(guest=="--list"){for(const auto& n:circuit::runtime::recordedGuestNames())std::cout<<n<<'\n';return 0;}
        const std::string mode=argc>2?argv[2]:"ff";if(mode!="ff"&&mode!="cached-ff"&&mode!="cached-dram")throw std::invalid_argument("Invalid machine mode");
        size_t cycles=500000;if(argc>3){std::string_view value=argv[3];auto r=std::from_chars(value.data(),value.data()+value.size(),cycles);if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid cycle limit");}
        size_t quantum=100000;if(argc>4){std::string_view value=argv[4];auto r=std::from_chars(value.data(),value.data()+value.size(),quantum);if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid event quantum");}
        const std::string fidelity=argc>5?argv[5]:"compact";
        if(fidelity!="compact"&&fidelity!="expanded-core"&&fidelity!="structural")throw std::invalid_argument("Invalid logic profile");
        std::cout<<circuit::runtime::runRecordedGuestFixtureJson(guest,mode!="ff",mode=="cached-dram",cycles,
            [](uint64_t c,uint64_t i,uint64_t f){std::cerr<<c<<" cycles, "<<i<<" retired, "<<f<<" presented\n";},quantum,fidelity=="expanded-core",fidelity!="structural")<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
