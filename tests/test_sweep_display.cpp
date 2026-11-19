#include "../src/sweep/sweep_display.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace trollfx::sweep;

template <typename Fn>
static ResponseCurve makeCurve(const Fn& fn) {
    ResponseCurve r;
    constexpr std::size_t count=8193;
    for(std::size_t i=0;i<count;i++) {
        const double hz=20.0*std::pow(1000.0,static_cast<double>(i)/(count-1));
        const double db=fn(hz);
        r.frequencyHz.push_back(hz);
        r.magnitude.push_back(std::pow(10.0,db/20.0));
        r.phaseRadians.push_back(0.2);
    }
    return r;
}

int main() {
    std::string error;
    auto flat=makeCurve([](double){return 0.0;});
    const auto originalMagnitude=flat.magnitude,originalPhase=flat.phaseRadians;
    SweepDisplayResponse d;
    for(unsigned n:{0u,48u,24u,12u,6u,3u}) {
        assert(makeSweepDisplayResponse(flat,n,d,&error));
        assert(d.displayMinimumDb==-36.0&&d.displayMaximumDb==12.0&&d.displayRangeDb==48.0);
        for(std::size_t i=0;i<31;i++) {
            assert(std::abs(d.smoothedRelativeDb[i])<1e-8);
            assert(std::abs(d.led[i]-18.0f)<1e-5f); // common-scale flat response
            assert(d.clipping[i]==0);
        }
        for(double db:d.graphRelativeDb)assert(std::abs(db)<1e-7);
        for(unsigned frame:d.graphSegmentFrame)assert(frame==32u*129u+32u); // 0 dB at the same adjacent endpoints
    }
    assert(flat.magnitude==originalMagnitude&&flat.phaseRadians==originalPhase);

    auto windowEdges=makeCurve([](double hz){
        if(std::abs(std::log2(hz/500.0))<0.01)return -36.0;
        if(std::abs(std::log2(hz/2000.0))<0.01)return 12.0;
        return 0.0;
    });
    assert(makeSweepDisplayResponse(windowEdges,0,d,&error));
    assert(d.led[14]==0.0f&&d.clipping[14]==2);
    assert(d.led[20]==24.0f&&d.clipping[20]==1);

    auto notched=makeCurve([](double hz){
        if(std::abs(std::log2(hz/500.0))<0.018)return -24.0;
        if(std::abs(std::log2(hz/2000.0))<0.012)return -6.0;
        if(std::abs(std::log2(hz/8000.0))<0.010)return 30.0;
        return 0.0;
    });
    assert(makeSweepDisplayResponse(notched,0,d,&error));
    assert(std::abs(d.smoothedRelativeDb[14]+24.0)<0.15); // 500 Hz = band 14
    assert(std::abs(d.led[14]-6.0)<0.08); // -24 dB of 48 dB span
    assert(d.clipping[14]==0);
    assert(std::abs(d.smoothedRelativeDb[20]+6.0)<0.15);
    assert(std::abs(d.led[20]-15.0)<0.08); // -6 dB = 3 LEDs below 1 kHz
    assert(std::abs(d.led[17]-18.0)<0.08); // 1 kHz reference
    assert(d.smoothedRelativeDb[26]>20.0&&d.clipping[26]==1&&d.led[26]==24.0f);
    assert(d.graphSegmentFrame.size()==255&&d.graphRelativeDb.size()==256);
    assert(std::abs(d.graphRelativeDb[85]-d.smoothedRelativeDb[17])<1e-8); // 1 kHz overview matches its high-res point
    assert(std::abs(d.graphRelativeDb[204]-d.smoothedRelativeDb[24])<0.5); // 5 kHz is represented by the dense curve
    assert(d.graphSegmentFrame[0]<kSweepGraphMissingFrame);
    bool graphUsesManyLevels=false;
    for(auto code:d.graphSegmentFrame)if(code!=d.graphSegmentFrame[0]){graphUsesManyLevels=true;break;}
    assert(graphUsesManyLevels);

    // Vertical scaling/center are display-only: same true relative values,
    // different clipped rendering, unchanged response and phase source arrays.
    const auto rawDb48=d.smoothedRelativeDb;
    const auto led48=d.led;
    assert(makeSweepDisplayResponse(notched,0,d,&error,24.0,-12.0));
    assert(d.displayMinimumDb==-24.0&&d.displayMaximumDb==0.0);
    assert(d.smoothedRelativeDb==rawDb48);
    assert(d.clipping[14]==2&&d.led[14]==0.0f);
    assert(d.clipping[17]==1&&d.led[17]==24.0f);
    assert(led48[17]<d.led[17]);
    assert(flat.magnitude==originalMagnitude); // display transforms did not touch original input
    assert(notched.phaseRadians.size()==8193&&notched.phaseRadians.front()==0.2);
    assert(makeSweepDisplayResponse(notched,0,d,&error,72.0,0.0));
    assert(d.displayMinimumDb==-36.0&&d.displayMaximumDb==36.0);
    assert(std::abs(d.smoothedRelativeDb[26]-rawDb48[26])<1e-9);
    assert(d.clipping[26]==0);
    assert(!makeSweepDisplayResponse(notched,0,d,&error,30.0,-12.0));

    // A partial valid bandwidth produces explicit no-data flags and transparent
    // line frames instead of fake floor values.
    auto partial=makeCurve([](double){return 0.0;});
    partial.frequencyHz.erase(partial.frequencyHz.begin(),partial.frequencyHz.begin()+2000);
    partial.magnitude.erase(partial.magnitude.begin(),partial.magnitude.begin()+2000);
    partial.phaseRadians.erase(partial.phaseRadians.begin(),partial.phaseRadians.begin()+2000);
    assert(makeSweepDisplayResponse(partial,6,d,&error));
    assert(d.clipping[0]==3&&std::isnan(d.smoothedRelativeDb[0]));
    assert(d.graphSegmentFrame.front()==kSweepGraphMissingFrame);
    assert(d.graphSegmentFrame.back()<kSweepGraphMissingFrame);

    std::cout<<"PASS V0.5.4 sweep view transform: common dB scale, 24-segment mapping, clipping, 256-point line frames, range/center invariance\n";
}
