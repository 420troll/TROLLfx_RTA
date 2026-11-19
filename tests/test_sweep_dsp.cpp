#include "../src/sweep/sweep_dsp.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

int main(){
    using namespace trollfx::sweep;
    SweepOptions so;so.sampleRate=8000;so.durationSeconds=1;so.startHz=40;so.stopHz=3000;so.peakDbfs=-30;so.fadeSeconds=.01;
    std::string error;auto x=generate(so);assert(x.size()==8000);assert(std::abs(x.front())<1e-12&&std::abs(x.back())<1e-12);
    std::vector<double> tone441(44100);for(std::size_t i=0;i<tone441.size();i++)tone441[i]=0.5*std::sin(2*3.14159265358979323846*10000*i/44100.0);
    std::vector<double> tone480;assert(resampleMono(tone441,44100,48000,tone480,&error));assert(tone480.size()==48000);
    double rms=0;for(std::size_t i=500;i+500<tone480.size();i++)rms+=tone480[i]*tone480[i];rms=std::sqrt(rms/(tone480.size()-1000));
    assert(std::abs(rms-0.5/std::sqrt(2.0))<0.002);
    std::vector<double> down;assert(resampleMono(tone480,48000,44100,down,&error));assert(down.size()==44100);
    std::vector<double> invalid;assert(!resampleMono(tone441,0,48000,invalid,&error));
    so.stopHz=4000;assert(!validate(so,&error));so.stopHz=3000;
    // Known room: delayed direct arrival plus one reflection at +280 samples.
    const std::size_t lead=1000,direct=96,echo=376,tail=400;
    std::vector<double> y(lead+x.size()+tail,0);
    for(std::size_t i=0;i<x.size();i++){y[lead+direct+i]+=.5*x[i];y[lead+echo+i]+=.125*x[i];}
    AnalysisOptions ao;ao.sampleRate=8000;ao.startHz=40;ao.stopHz=3000;
    AnalysisResult result;assert(analyze(x,y,ao,result,&error));
    assert(result.impulsePeak>=lead+direct-1&&result.impulsePeak<=lead+direct+1);
    std::vector<std::pair<double,std::size_t>> peaks;for(std::size_t i=0;i<result.impulse.size();i++)if(std::abs(result.impulse[i])>.025)peaks.emplace_back(std::abs(result.impulse[i]),i);
    std::sort(peaks.begin(),peaks.end(),[](auto a,auto b){return a.first>b.first;});
    assert(peaks.size()>1);const std::size_t directAt=peaks[0].second;
    auto reflection=std::find_if(peaks.begin()+1,peaks.end(),[&](auto p){return p.second>directAt+100&&std::abs(static_cast<long>(p.second-directAt)-280)<=1;});
    assert(reflection!=peaks.end());assert(std::abs(20*std::log10(reflection->first/peaks[0].first)-20*std::log10(.25))<1.0);
    double combMin=1e9,combMax=-1e9;for(std::size_t i=0;i<result.frequencyHz.size();i++)if(result.frequencyHz[i]>100&&result.frequencyHz[i]<2500){combMin=std::min(combMin,result.magnitudeDb[i]);combMax=std::max(combMax,result.magnitudeDb[i]);}assert(combMax-combMin>3.0);
    // Flat response gain is measured from a separate pure-gain recording.
    std::vector<double> flat(x.size());for(std::size_t i=0;i<x.size();i++)flat[i]=.625*x[i];
    AnalysisResult flatResult;assert(analyze(x,flat,ao,flatResult,&error));
    std::vector<double> db;for(std::size_t i=0;i<flatResult.frequencyHz.size();i++)if(flatResult.frequencyHz[i]>100&&flatResult.frequencyHz[i]<2500)db.push_back(flatResult.magnitudeDb[i]);
    std::sort(db.begin(),db.end());assert(std::abs(db[db.size()/2]-20*std::log10(.625))<.3);
    CalibrationCurve cal0,cal90;assert(readCalibration("calibration/7033298.txt",0,cal0,&error));assert(readCalibration("calibration/7033298_90deg.txt",90,cal90,&error));
    assert(std::abs(cal0.sensitivityFactorDb-2.7673)<1e-6&&std::abs(cal90.sensitivityFactorDb-2.7673)<1e-6);
    assert(std::abs(calibrationDb(cal0,5000)-calibrationDb(cal90,5000))>.5);
    auto calibrated0=flatResult;auto calibrated90=flatResult;std::size_t toneBin=0;for(std::size_t i=0;i<flatResult.frequencyHz.size();i++)if(std::abs(flatResult.frequencyHz[i]-1000)<std::abs(flatResult.frequencyHz[toneBin]-1000))toneBin=i;
    const double uncorrected=flatResult.magnitudeDb[toneBin];assert(applyCalibration(calibrated0,cal0,&error));assert(applyCalibration(calibrated90,cal90,&error));
    assert(std::abs(calibrated0.magnitudeDb[toneBin]-(uncorrected-calibrationDb(cal0,flatResult.frequencyHz[toneBin])))<1e-9);
    assert(std::abs(calibrated0.magnitudeDb[toneBin]-calibrated90.magnitudeDb[toneBin])>.02);
    assert(calibrated0.calibrationOrientationDeg==0&&calibrated90.calibrationOrientationDeg==90&&calibrated0.calibrationSensitivityFactorDb==2.7673);
    // Two independent timing markers expose 100 ppm; one point cannot.
    double ppm=0;assert(estimateClockPpm(960000,960096,ppm));assert(std::abs(ppm-100)<.01);assert(!estimateClockPpm(0,10,ppm));
    auto timed=generateMeasurementSignal(so,.2,.2,.05,.08);assert(!timed.samples.empty());
    const double markerRatio=1.0001;const std::size_t markerLead=503;
    std::vector<double> markerCapture(markerLead+static_cast<std::size_t>(timed.samples.size()*markerRatio)+100,0.0);
    for(std::size_t i=0;i+markerLead<markerCapture.size();i++){double at=i/markerRatio;auto j=static_cast<std::size_t>(at);double f=at-j;if(j+1<timed.samples.size())markerCapture[markerLead+i]=timed.samples[j]*(1-f)+timed.samples[j+1]*f;}
    ClockAlignment alignment;assert(locateClockMarkers(timed.marker,markerCapture,timed.markerEndSample-timed.markerStartSample,alignment,&error));
    assert(std::abs(alignment.clockPpm-100)<15);assert(std::abs(static_cast<long>(alignment.firstMarkerSample)-static_cast<long>(markerLead+timed.markerStartSample))<=3);
    // Drift correction operates before spectral recovery; synthetic capture is
    // sampled 100 ppm faster and is restored to the playback sample timeline.
    const double ratio=1.0001;std::vector<double> drifted(static_cast<std::size_t>(y.size()*ratio));
    for(std::size_t i=0;i<drifted.size();i++){double at=i/ratio;auto j=static_cast<std::size_t>(at);double f=at-j;drifted[i]=j+1<y.size()?y[j]*(1-f)+y[j+1]*f:y.back();}
    ao.clockPpm=100;AnalysisResult corrected;assert(analyze(x,drifted,ao,corrected,&error));
    assert(corrected.impulsePeak>=lead+direct-2&&corrected.impulsePeak<=lead+direct+2);
    std::vector<double> truncated(y.begin(),y.begin()+x.size()/3);assert(!analyze(x,truncated,ao,corrected,&error));
    // Clipping is explicitly rejected rather than turned into a plausible curve.
    auto clipped=y;for(std::size_t i=lead+direct;i<lead+direct+8;i++)clipped[i]=1.0;assert(!analyze(x,clipped,ao,corrected,&error));assert(error.find("clipped")!=std::string::npos);
    // White noise is deterministic and finite through the analysis path.
    ao.rejectClipping=false;ao.clockPpm=0;auto noisy=y;uint32_t state=0x12345678;for(std::size_t i=0;i<noisy.size();i++){state=1664525*state+1013904223;noisy[i]+=(static_cast<int32_t>(state)-2147483648.0)/2147483648.0*1e-5;}
    assert(analyze(x,noisy,ao,corrected,&error));for(double v:corrected.impulse)assert(std::isfinite(v));
    const auto dir=std::filesystem::temp_directory_path()/"trollfx-sweep-dsp-test";std::filesystem::remove_all(dir);assert(writeArtifacts(dir.string(),corrected,8000,40,3000,&error));
    assert(std::filesystem::exists(dir/"impulse.wav")&&std::filesystem::exists(dir/"frequency-response.csv")&&std::filesystem::exists(dir/"frequency-response.svg")&&std::filesystem::exists(dir/"measurement.json"));std::filesystem::remove_all(dir);
    // A 10 s 48 kHz sweep/capture pair produces a 1,048,576 point transfer
    // FFT. At this resolution default ostream precision rounds adjacent bins
    // to the same printed frequency (the Force failure near 10 kHz/19.8 kHz).
    constexpr std::size_t longFftSize=1u<<20;constexpr unsigned longSampleRate=48000;constexpr double longStopHz=19845.0;
    AnalysisResult longResponse;longResponse.impulse={0.0};
    for(std::size_t k=0;k<=longFftSize/2;k++){
        const double hz=static_cast<double>(k)*longSampleRate/static_cast<double>(longFftSize);
        if(hz<20.0||hz>longStopHz)continue;
        longResponse.frequencyHz.push_back(hz);longResponse.magnitude.push_back(0.5);longResponse.magnitudeDb.push_back(20.0*std::log10(0.5));longResponse.phaseRadians.push_back(0.0);
    }
    bool defaultPrecisionCollision=false;
    for(std::size_t i=1;i<longResponse.frequencyHz.size();i++){
        std::ostringstream previous,current;previous<<longResponse.frequencyHz[i-1];current<<longResponse.frequencyHz[i];
        if(previous.str()==current.str()){defaultPrecisionCollision=true;break;}
    }
    assert(defaultPrecisionCollision);
    const auto precisionDir=std::filesystem::temp_directory_path()/"trollfx-sweep-csv-precision-test";std::filesystem::remove_all(precisionDir);
    assert(writeArtifacts(precisionDir.string(),longResponse,longSampleRate,20.0,longStopHz,&error,6));
    ResponseCurve longRoundTrip;assert(readResponseCsv((precisionDir/"frequency-response.csv").string(),longRoundTrip,&error));
    assert(longRoundTrip.frequencyHz==longResponse.frequencyHz);
    assert(longRoundTrip.magnitude==longResponse.magnitude&&longRoundTrip.gainDb==longResponse.magnitudeDb&&longRoundTrip.phaseRadians==longResponse.phaseRadians);
    for(std::size_t i=1;i<longRoundTrip.frequencyHz.size();i++)assert(longRoundTrip.frequencyHz[i]>longRoundTrip.frequencyHz[i-1]);
    assert(std::filesystem::exists(precisionDir/"frequency-response-smoothed.csv")&&std::filesystem::exists(precisionDir/"frequency-response.svg"));
    std::filesystem::remove_all(precisionDir);
    // Fractional-octave smoothing is magnitude-domain and log-frequency based.
    std::vector<double> freq,flatMag,peakMag;for(int i=0;i<=1200;i++){double hz=20*std::pow(1000.0,i/1200.0);freq.push_back(hz);flatMag.push_back(0.5);double z=std::log2(hz/1000.0);peakMag.push_back(0.5+2.0*std::exp(-0.5*(z/.018)*(z/.018)));}
    std::vector<double> sm;assert(fractionalOctaveSmooth(freq,flatMag,6,sm,&error));for(double v:sm)assert(std::abs(v-.5)<1e-10);
    assert(fractionalOctaveSmooth(freq,peakMag,0,sm,&error));assert(sm==peakMag);const std::size_t peakIndex=static_cast<std::size_t>(std::round(std::log(1000.0/20.0)/std::log(1000.0)*1200));double lastPeak=peakMag[peakIndex];for(unsigned n:{48u,24u,12u,6u,3u}){assert(fractionalOctaveSmooth(freq,peakMag,n,sm,&error));assert(sm[peakIndex]<lastPeak);lastPeak=sm[peakIndex];assert(std::abs(sm.front()-peakMag.front())<1e-7&&std::abs(sm.back()-peakMag.back())<1e-7);}
    // Reprocessing a legacy session preserves its raw CSV and impulse file.
    auto session=std::filesystem::temp_directory_path()/"trollfx-sweep-smoothing-test";std::filesystem::remove_all(session);std::filesystem::create_directories(session);
    {std::ofstream csv(session/"frequency-response.csv");csv<<"frequency_hz,magnitude,gain_db,phase_rad\n";for(std::size_t i=0;i<freq.size();++i)csv<<freq[i]<<','<<peakMag[i]<<','<<20*std::log10(peakMag[i])<<",0.25\n";}
    {std::ofstream json(session/"measurement.json");json<<"{\"format\":\"TROLLfx Sweep Lab V0.5\",\"clipped_samples\":0}\n";}
    {std::ofstream ir(session/"impulse.wav");ir<<"preserve";}
    const auto rawBefore=std::filesystem::file_size(session/"frequency-response.csv");assert(reprocessSession(session.string(),6,1000,true,&error));assert(std::filesystem::file_size(session/"frequency-response.csv")==rawBefore);
    std::ifstream graph(session/"frequency-response.svg");std::string svg((std::istreambuf_iterator<char>(graph)),{});assert(svg.find("1/6 octave")!=std::string::npos&&svg.find("y=\"548\"")!=std::string::npos&&svg.find("Frequency (Hz)")!=std::string::npos);std::size_t lineCommands=0,pos=0;while((pos=svg.find(" L",pos))!=std::string::npos){++lineCommands;pos+=2;}assert(lineCommands==2*(freq.size()-1));
    assert(reprocessSession(session.string(),3,1000,true,&error));std::ifstream metadata(session/"measurement.json");std::string meta((std::istreambuf_iterator<char>(metadata)),{});assert(meta.find("clipped_samples")!=std::string::npos&&meta.find("smoothing_algorithm_version")!=std::string::npos&&meta.find("response_smoothing_octaves\": 3")!=std::string::npos);assert(meta.find("smoothing_algorithm_version",meta.find("smoothing_algorithm_version")+1)==std::string::npos);
    std::ifstream impulse(session/"impulse.wav");std::string irText((std::istreambuf_iterator<char>(impulse)),{});assert(irText=="preserve");{std::ofstream bad(session/"measurement.json");bad<<"{\"clipped_samples\": 4}";}assert(!reprocessSession(session.string(),6,1000,true,&error)&&error.find("clipped")!=std::string::npos);std::filesystem::remove_all(session);
    std::cout<<"sweep DSP: generator, deconvolution, gain, reflection, two-marker drift estimate/correction, clipping, noise, exports PASS\n";
}
