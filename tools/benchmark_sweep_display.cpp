#include "../src/sweep/sweep_display.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <sys/resource.h>

int main() {
    using namespace trollfx::sweep;
    constexpr std::size_t count=131073; // 262144-point real FFT frequency grid.
    ResponseCurve r;
    r.frequencyHz.resize(count);r.magnitude.resize(count);r.phaseRadians.resize(count);
    for(std::size_t i=0;i<count;i++) {
        const double hz=20.0*std::pow(1000.0,static_cast<double>(i)/(count-1));
        const double ripple=2.0*std::sin(std::log2(hz/1000.0)*9.0)+
                            4.0*std::exp(-0.5*std::pow(std::log2(hz/83.0)/0.02,2));
        r.frequencyHz[i]=hz;r.magnitude[i]=std::pow(10.0,ripple/20.0);r.phaseRadians[i]=0.0;
    }
    SweepDisplayResponse view;std::string error;
    constexpr int iterations=5;
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<iterations;i++)if(!makeSweepDisplayResponse(r,6,view,&error)){
        std::cerr<<error<<"\n";return 1;
    }
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/iterations;
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::cout<<"V0.5.4 host display benchmark: fft_bins="<<count
             <<" smoothed_points="<<count<<" graph_points="<<kSweepGraphSamples
             <<" mean_transform_ms="<<elapsed
             <<" process_peak_rss_kib="<<usage.ru_maxrss
             <<" graph_atlas_rgba_bytes="<<(4ull*64ull*(kSweepGraphMissingFrame+1)*4ull)<<"\n";
}
