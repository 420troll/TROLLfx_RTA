#include "../src/sweep/sweep_display.hpp"

#include <algorithm>
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
    SweepDisplayResponse d;
    auto flat=makeCurve([](double){return 0.0;});
    const auto rawBefore=flat.magnitude,phaseBefore=flat.phaseRadians;
    assert(makeSweepDisplayResponse(flat,d,&error));
    assert(kSweepDisplaySmoothingN==6);
    assert(std::abs(d.displayMinimumDb+1.0)<1e-10);
    assert(std::abs(d.displayMaximumDb-1.0)<1e-10);
    assert(std::abs(d.displayRangeDb-2.0)<1e-10);
    for(std::size_t i=0;i<31;i++) {
        assert(std::abs(d.smoothedRelativeDb[i])<1e-8);
        assert(std::abs(d.led[i]-12.0f)<1e-5f);
        assert(d.clipping[i]==0);
    }
    for(double db:d.graphRelativeDb)assert(std::abs(db)<1e-7);
    for(unsigned frame:d.graphSegmentFrame)assert(frame==15u*kSweepGraphVerticalSteps+15u);
    assert(flat.magnitude==rawBefore&&flat.phaseRadians==phaseBefore);

    // Fixed 1/6-octave smoothing and automatic range retain the complete
    // broadband response, with the same common mapping for LED and graph.
    auto wide=makeCurve([](double hz){return hz<100.0?-30.0:(hz>5000.0?12.0:0.0);});
    std::vector<double> expectedSmooth;
    assert(fractionalOctaveSmooth(wide.frequencyHz,wide.magnitude,6,expectedSmooth,&error));
    assert(makeSweepDisplayResponse(wide,d,&error));
    const double ref=responseDbAt(wide.frequencyHz,expectedSmooth,1000.0);
    double lo=std::numeric_limits<double>::infinity(),hi=-lo;
    for(std::size_t i=0;i<wide.frequencyHz.size();i++) {
        if(wide.frequencyHz[i]<20.0||wide.frequencyHz[i]>20000.0)continue;
        const double db=responseDbAt(wide.frequencyHz,expectedSmooth,wide.frequencyHz[i])-ref;
        if(std::isfinite(db)){lo=std::min(lo,db);hi=std::max(hi,db);}
    }
    const double margin=std::max(1.0,(hi-lo)*0.05);
    assert(std::abs(d.displayMinimumDb-(lo-margin))<1e-8);
    assert(std::abs(d.displayMaximumDb-(hi+margin))<1e-8);
    assert(d.displayMinimumDb < -30.0 && d.displayMaximumDb > 12.0);
    assert(std::abs(d.smoothedRelativeDb[17])<1e-8); // shared 1 kHz reference
    assert(d.led[0]>0.0f&&d.led[30]<24.0f); // headroom avoids clipping valid extrema
    assert(d.graphRelativeDb.size()==256&&d.graphSegmentFrame.size()==255);
    assert(d.graphSegmentFrame.front()<kSweepGraphMissingFrame);
    assert(d.graphSegmentFrame.back()<kSweepGraphMissingFrame);
    assert(std::any_of(d.graphSegmentFrame.begin(),d.graphSegmentFrame.end(),
                       [&](unsigned f){return f!=d.graphSegmentFrame.front();}));

    // Multiple saved sweeps share one 1 kHz-relative display range. Each
    // curve stays independent in color/parameter space, but never receives a
    // per-curve normalization.
    ResponseCurve flat2=flat;
    for(std::size_t i=0;i<flat2.magnitude.size();i++)
        if(flat2.frequencyHz[i]>2500.0)flat2.magnitude[i]*=std::pow(10.0,-6.0/20.0);
    ResponseCurve peaked=flat;
    for(std::size_t i=0;i<peaked.magnitude.size();i++){
        const double oct=std::log2(peaked.frequencyHz[i]/5000.0);
        peaked.magnitude[i]*=std::pow(10.0,(oct>-0.5&&oct<0.5?8.0:0.0)/20.0);
    }
    std::array<const ResponseCurve*,kSweepDisplayLayers> curves{};
    curves[0]=&flat;curves[1]=&flat2;curves[2]=&peaked;
    SweepOverlayDisplay overlays;
    assert(makeSweepOverlayDisplay(curves,0x07,overlays,&error));
    assert(overlays.validLayerMask==0x07);
    assert(overlays.displayMaximumDb>8.0&&overlays.displayMinimumDb<-6.0);
    assert(overlays.graphSegmentFrame[0]!=overlays.graphSegmentFrame[1]);
    assert(overlays.graphSegmentFrame[0]!=overlays.graphSegmentFrame[2]);
    assert(overlays.graphSegmentFrame[3][0]==kSweepGraphMissingFrame);
    assert(overlays.graphSegmentFrame[7][kSweepGraphSegments-1]==kSweepGraphMissingFrame);
    SweepOverlayDisplay oneCurve;
    curves[1]=nullptr;curves[2]=nullptr;
    assert(makeSweepOverlayDisplay(curves,0x07,oneCurve,&error));
    assert(oneCurve.validLayerMask==0x01);
    assert(oneCurve.displayMinimumDb>overlays.displayMinimumDb);
    assert(oneCurve.displayMaximumDb<overlays.displayMaximumDb);
    curves[0]=nullptr;
    assert(makeSweepOverlayDisplay(curves,0xff,oneCurve,&error));
    assert(oneCurve.validLayerMask==0);
    assert(wide.phaseRadians.front()==0.2);

    // A narrow resonance is reduced by fixed 1/6-octave smoothing; the full
    // display scale follows the smoothed curve and is identical for both views.
    auto resonant=makeCurve([](double hz){
        return 18.0*std::exp(-0.5*std::pow(std::log2(hz/2000.0)/0.018,2.0));
    });
    assert(makeSweepDisplayResponse(resonant,d,&error));
    assert(d.displayMaximumDb>d.displayMinimumDb);
    assert(std::abs(d.smoothedRelativeDb[17])<1e-7);
    assert(d.smoothedRelativeDb[20]>0.0&&d.smoothedRelativeDb[20]<18.0);
    for(double value:d.graphRelativeDb)assert(value>=d.displayMinimumDb&&value<=d.displayMaximumDb);

    // A partial valid bandwidth preserves no-data edges without flooring or
    // changing the original response/phase arrays.
    auto partial=makeCurve([](double){return 0.0;});
    partial.frequencyHz.erase(partial.frequencyHz.begin(),partial.frequencyHz.begin()+2000);
    partial.magnitude.erase(partial.magnitude.begin(),partial.magnitude.begin()+2000);
    partial.phaseRadians.erase(partial.phaseRadians.begin(),partial.phaseRadians.begin()+2000);
    assert(makeSweepDisplayResponse(partial,d,&error));
    assert(d.clipping[0]==3&&std::isnan(d.smoothedRelativeDb[0]));
    assert(d.graphSegmentFrame.front()==kSweepGraphMissingFrame);
    assert(d.graphSegmentFrame.back()<kSweepGraphMissingFrame);
    assert(flat.magnitude==rawBefore&&flat.phaseRadians==phaseBefore);

    std::cout<<"PASS V1.0 1/6 smoothing, full-range auto-fit, shared multi-layer dB axis and 256-point connected traces\n";
}
