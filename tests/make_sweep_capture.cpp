#include "../src/sweep/sweep_dsp.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

int main(int argc,char**argv){
    using namespace trollfx::sweep;
    if(argc!=2)return 2;
    SweepOptions options;options.sampleRate=44100;options.durationSeconds=.5;options.startHz=20;
    options.stopHz=std::nextafter(.45*options.sampleRate,0.0);options.peakDbfs=-30;options.fadeSeconds=.01;
    auto signal=generateMeasurementSignal(options);if(signal.samples.empty()){std::cerr<<"test sweep generation failed\n";return 1;}
    std::vector<double> reference;if(!resampleMono(signal.samples,options.sampleRate,48000,reference)){std::cerr<<"test resampling failed\n";return 1;}
    std::vector<double> capture(3u*48000u);constexpr std::size_t lead=1375,echo=280;
    if(lead+reference.size()+echo>=capture.size()){std::cerr<<"test capture bound too small\n";return 1;}
    for(std::size_t i=0;i<reference.size();i++){
        capture[lead+i]+=.5*reference[i];capture[lead+echo+i]+=.125*reference[i];
    }
    std::string error;if(!writeFloatWav(argv[1],capture,48000,&error)){std::cerr<<error<<'\n';return 1;}
    return 0;
}
