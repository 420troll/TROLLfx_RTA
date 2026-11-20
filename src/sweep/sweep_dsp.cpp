#include "sweep_dsp.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <cctype>
#include <mutex>

namespace trollfx::sweep {
namespace {
using C = std::complex<double>;
constexpr double pi = 3.1415926535897932384626433832795;

void fft(std::vector<C>& a, bool inverse) {
    const std::size_t n = a.size();
    for (std::size_t i=1,j=0;i<n;i++) {
        std::size_t bit=n>>1;
        for (;j&bit;bit>>=1) j^=bit;
        j^=bit;
        if(i<j) std::swap(a[i],a[j]);
    }
    for(std::size_t len=2;len<=n;len<<=1) {
        const double angle=(inverse?2.0:-2.0)*pi/static_cast<double>(len);
        const C root(std::cos(angle),std::sin(angle));
        for(std::size_t i=0;i<n;i+=len) {
            C w(1.0,0.0);
            for(std::size_t j=0;j<len/2;j++) {
                const C u=a[i+j], v=a[i+j+len/2]*w;
                a[i+j]=u+v; a[i+j+len/2]=u-v; w*=root;
            }
        }
        if(len>n/2) break; // avoid unsigned overflow on an impossible next stage
    }
    if(inverse) for(C& v:a) v/=static_cast<double>(n);
}
std::size_t nextPow2(std::size_t n) {
    std::size_t p=1;
    while(p<n) { if(p>std::numeric_limits<std::size_t>::max()/2) return 0; p<<=1; }
    return p;
}
uint16_t u16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1]<<8)); }
uint32_t u32(const unsigned char* p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1])<<8) | (static_cast<uint32_t>(p[2])<<16) | (static_cast<uint32_t>(p[3])<<24); }
void put16(std::ofstream& f,uint16_t n){ char b[2]={static_cast<char>(n),static_cast<char>(n>>8)};f.write(b,2); }
void put32(std::ofstream& f,uint32_t n){ char b[4]={static_cast<char>(n),static_cast<char>(n>>8),static_cast<char>(n>>16),static_cast<char>(n>>24)};f.write(b,4); }
bool fail(std::string* e,const char* m){if(e)*e=m;return false;}
}

bool validate(const SweepOptions& o,std::string* e) {
    if(o.sampleRate<8000 || o.sampleRate>192000) return fail(e,"sample rate must be 8–192 kHz");
    if(!std::isfinite(o.durationSeconds)||o.durationSeconds<0.1||o.durationSeconds>30.0) return fail(e,"duration must be 0.1–30 seconds");
    if(!std::isfinite(o.startHz)||!std::isfinite(o.stopHz)||o.startHz<=0||o.stopHz<=o.startHz||o.stopHz>=0.45*o.sampleRate) return fail(e,"sweep frequencies must satisfy 0 < start < stop < 0.45 * sample rate");
    if(!std::isfinite(o.peakDbfs)||o.peakDbfs < -60||o.peakDbfs > -6) return fail(e,"peak must be between -60 and -6 dBFS");
    if(!std::isfinite(o.fadeSeconds)||o.fadeSeconds<0||o.fadeSeconds>0.1*o.durationSeconds) return fail(e,"fade must be between zero and 10% of duration");
    return true;
}

bool resampleMono(const std::vector<double>& input,unsigned inputRate,unsigned outputRate,std::vector<double>& output,std::string* e) {
    output.clear();
    if(input.empty()||inputRate<8000||inputRate>192000||outputRate<8000||outputRate>192000)return fail(e,"resampler rates or input are invalid");
    const std::size_t count=static_cast<std::size_t>(std::llround(static_cast<double>(input.size())*outputRate/inputRate));
    if(count<2||count>8u*1024u*1024u)return fail(e,"resampled sweep exceeds the 8M sample safety limit");
    output.resize(count);constexpr int radius=32;
    const double ratio=static_cast<double>(inputRate)/outputRate,cutoff=std::min(1.0,static_cast<double>(outputRate)/inputRate);
    for(std::size_t n=0;n<count;n++){
        const double at=n*ratio;const long center=static_cast<long>(std::floor(at));double sum=0,weight=0;
        for(int tap=-radius+1;tap<=radius;tap++){
            const long index=center+tap;if(index<0||static_cast<std::size_t>(index)>=input.size())continue;
            const double x=at-static_cast<double>(index);if(std::abs(x)>=radius)continue;
            const double sx=cutoff*x;const double sinc=std::abs(sx)<1e-12?1.0:std::sin(pi*sx)/(pi*sx);
            const double window=0.5+0.5*std::cos(pi*x/radius);const double w=cutoff*sinc*window;
            sum+=input[static_cast<std::size_t>(index)]*w;weight+=w;
        }
        output[n]=std::abs(weight)>1e-12?sum/weight:0.0;
    }
    return true;
}

std::vector<double> generate(const SweepOptions& o) {
    if(!validate(o)) return {};
    const std::size_t n=static_cast<std::size_t>(std::llround(o.sampleRate*o.durationSeconds));
    const double T=static_cast<double>(n)/o.sampleRate;
    const double L=T/std::log(o.stopHz/o.startHz), gain=std::pow(10.0,o.peakDbfs/20.0);
    const std::size_t fade=static_cast<std::size_t>(std::llround(o.fadeSeconds*o.sampleRate));
    std::vector<double> x(n);
    for(std::size_t i=0;i<n;i++) {
        const double t=static_cast<double>(i)/o.sampleRate;
        const double ramp=fade?std::max(0.0,std::min({1.0,static_cast<double>(i)/fade,static_cast<double>(n-1-i)/fade})):1.0;
        x[i]=gain*ramp*std::sin(2*pi*o.startHz*L*std::expm1(t/L));
    }
    return x;
}

MeasurementSignal generateMeasurementSignal(const SweepOptions& o,double pre,double post,double gap,double markerSeconds) {
    MeasurementSignal out;if(!validate(o)||!std::isfinite(pre)||!std::isfinite(post)||!std::isfinite(gap)||!std::isfinite(markerSeconds)||pre<0||post<0||gap<0||markerSeconds<0.02||markerSeconds>0.5)return out;
    const std::vector<double> sweep=generate(o);if(sweep.empty())return out;
    const std::size_t preN=static_cast<std::size_t>(std::llround(pre*o.sampleRate)),gapN=static_cast<std::size_t>(std::llround(gap*o.sampleRate)),postN=static_cast<std::size_t>(std::llround(post*o.sampleRate));
    const std::size_t markerN=static_cast<std::size_t>(std::llround(markerSeconds*o.sampleRate)),fade=std::max<std::size_t>(1,static_cast<std::size_t>(std::llround(0.005*o.sampleRate)));
    out.marker.resize(markerN);const double duration=static_cast<double>(markerN)/o.sampleRate,f0=300.0,f1=std::min(5000.0,0.2*o.sampleRate),gain=std::pow(10.0,o.peakDbfs/20.0);
    for(std::size_t i=0;i<markerN;i++){double t=static_cast<double>(i)/o.sampleRate;double ramp=std::min({1.0,static_cast<double>(i)/fade,static_cast<double>(markerN-1-i)/fade});out.marker[i]=gain*ramp*std::sin(2*pi*(f0*t+0.5*(f1-f0)*t*t/duration));}
    out.markerStartSample=preN;out.markerEndSample=preN+markerN+gapN+sweep.size()+gapN;
    const std::size_t total=out.markerEndSample+markerN+postN;if(total>8u*1024u*1024u)return MeasurementSignal{};
    out.samples.assign(total,0.0);std::copy(out.marker.begin(),out.marker.end(),out.samples.begin()+out.markerStartSample);
    const std::size_t sweepAt=out.markerStartSample+markerN+gapN;std::copy(sweep.begin(),sweep.end(),out.samples.begin()+sweepAt);
    std::copy(out.marker.begin(),out.marker.end(),out.samples.begin()+out.markerEndSample);return out;
}

bool locateClockMarkers(const std::vector<double>& marker,const std::vector<double>& capture,double expected,ClockAlignment& out,std::string* e) {
    out=ClockAlignment{};if(marker.size()<128||capture.size()<marker.size()*2||!std::isfinite(expected)||expected<=marker.size()||expected>=capture.size())return fail(e,"marker/capture lengths are invalid");
    for(double v:marker)if(!std::isfinite(v))return fail(e,"marker contains non-finite samples");
    for(double v:capture)if(!std::isfinite(v))return fail(e,"capture contains non-finite samples");
    constexpr std::size_t stride=2;std::vector<double> score(capture.size(),0.0);
    for(std::size_t phase=0;phase<stride;phase++) {
        const std::size_t cn=(capture.size()-phase+stride-1)/stride,mn=(marker.size()+stride-1)/stride;
        std::vector<double> c(cn),m(mn),prefix(cn+1,0.0);
        for(std::size_t i=0;i<cn;i++){c[i]=capture[phase+i*stride];prefix[i+1]=prefix[i]+c[i]*c[i];}
        for(std::size_t i=0;i<mn;i++)m[i]=marker[i*stride];
        const std::size_t n=nextPow2(cn+mn);if(!n||n>4u*1024u*1024u)return fail(e,"marker correlation exceeds memory limit");
        std::vector<C>A(n),B(n);double markerEnergy=0;for(std::size_t i=0;i<mn;i++){A[i]=c[i];B[i]=m[i];markerEnergy+=m[i]*m[i];}
        for(std::size_t i=mn;i<cn;i++) A[i]=c[i];
        fft(A,false);fft(B,false);
        for(std::size_t k=0;k<n;k++) A[k]*=std::conj(B[k]);
        fft(A,true);
        const std::size_t last=cn-mn;
        for(std::size_t lag=0;lag<=last;lag++){double energy=prefix[lag+mn]-prefix[lag];double value=energy>1e-20?A[lag].real()/std::sqrt(markerEnergy*energy):0;std::size_t at=phase+lag*stride;if(at<score.size()&&value>score[at])score[at]=value;}
    }
    std::size_t first=0;for(std::size_t i=0;i<score.size();i++)if(score[i]>score[first])first=i;
    const double tolerance=std::max(expected*0.02,static_cast<double>(marker.size()));
    auto search=[&](long lo,long hi)->std::size_t{lo=std::max(0L,lo);hi=std::min(static_cast<long>(score.size()-1),hi);std::size_t best=0;for(long i=lo;i<=hi;i++)if(score[static_cast<std::size_t>(i)]>score[best])best=static_cast<std::size_t>(i);return best;};
    std::size_t second=search(static_cast<long>(first+expected-tolerance),static_cast<long>(first+expected+tolerance));bool reversed=false;
    if(score[second]<0.05){second=search(static_cast<long>(first-expected-tolerance),static_cast<long>(first-expected+tolerance));reversed=true;}
    if(score[first]<0.05||score[second]<0.05||first==second)return fail(e,"alignment markers not found with sufficient correlation");
    if(reversed)std::swap(first,second);
    auto refine=[&](std::size_t coarse){std::size_t lo=coarse>stride?coarse-stride:0,hi=std::min(capture.size()-marker.size(),coarse+stride),best=lo;double bestScore=-1,markerEnergy=0;for(double v:marker)markerEnergy+=v*v;for(std::size_t at=lo;at<=hi;at++){double dot=0,energy=0;for(std::size_t j=0;j<marker.size();j++){dot+=capture[at+j]*marker[j];energy+=capture[at+j]*capture[at+j];}double s=energy>1e-20?dot/std::sqrt(markerEnergy*energy):0;if(s>bestScore){bestScore=s;best=at;}}return std::pair<std::size_t,double>{best,bestScore};};
    auto a=refine(first),b=refine(second);if(b.first<=a.first)return fail(e,"alignment marker order is invalid");double ppm=0;if(!estimateClockPpm(expected,static_cast<double>(b.first-a.first),ppm))return fail(e,"estimated clock drift outside ±10000 ppm");
    out.firstMarkerSample=a.first;out.secondMarkerSample=b.first;out.clockPpm=ppm;out.firstConfidence=a.second;out.secondConfidence=b.second;return true;
}

bool estimateClockPpm(double expected,double observed,double& ppm) {
    if(!std::isfinite(expected)||!std::isfinite(observed)||expected<=0||observed<=0) return false;
    ppm=(observed/expected-1.0)*1e6;
    return std::isfinite(ppm)&&std::abs(ppm)<=10000.0;
}

bool analyze(const std::vector<double>& reference,const std::vector<double>& raw,
             const AnalysisOptions& o,AnalysisResult& r,std::string* e) {
    r=AnalysisResult{};
    if(reference.size()<16||raw.size()<reference.size()/2) return fail(e,"reference or capture is too short");
    if(o.sampleRate<8000||!std::isfinite(o.startHz)||!std::isfinite(o.stopHz)||o.startHz<=0||o.stopHz<=o.startHz||o.stopHz>=0.5*o.sampleRate) return fail(e,"invalid analysis frequency range");
    if(!std::isfinite(o.clockPpm)||std::abs(o.clockPpm)>10000) return fail(e,"clock correction outside ±10000 ppm");
    if(!std::isfinite(o.clipThreshold)||o.clipThreshold<=0||o.clipThreshold>1) return fail(e,"invalid clipping threshold");
    for(double v:reference) if(!std::isfinite(v)) return fail(e,"reference contains non-finite samples");
    const double ratio=1.0+o.clockPpm*1e-6;
    const std::size_t resampledCount=static_cast<std::size_t>(std::floor((raw.size()-1)/ratio))+1;
    std::vector<double> y(resampledCount);
    for(std::size_t i=0;i<resampledCount;i++) {
        const double at=i*ratio; const std::size_t j=static_cast<std::size_t>(at);
        const double f=at-j;
        y[i]=j+1<raw.size()?raw[j]*(1-f)+raw[j+1]*f:raw[j];
        if(!std::isfinite(y[i])) return fail(e,"capture contains non-finite samples");
        r.inputPeak=std::max(r.inputPeak,std::abs(y[i]));
        if(std::abs(y[i])>=o.clipThreshold) r.clippedSamples++;
    }
    if(o.rejectClipping&&r.clippedSamples) return fail(e,"capture is clipped; result rejected");
    if(resampledCount>8u*1024u*1024u||reference.size()>8u*1024u*1024u) return fail(e,"recording exceeds 8M sample safety bound");
    const std::size_t n=nextPow2(reference.size()+y.size());
    if(!n||n>16u*1024u*1024u) return fail(e,"FFT exceeds 16M point safety bound");
    std::vector<C> X(n),Y(n);
    for(std::size_t i=0;i<reference.size();i++) X[i]=reference[i];
    for(std::size_t i=0;i<y.size();i++) Y[i]=y[i];
    fft(X,false); fft(Y,false);
    double maxPower=0;
    for(const C& x:X) maxPower=std::max(maxPower,std::norm(x));
    if(maxPower<=std::numeric_limits<double>::min()) return fail(e,"reference sweep has no measurable energy");
    const double lambda=maxPower*1e-14;
    for(std::size_t k=0;k<n;k++) {
        const double hz=static_cast<double>(std::min(k,n-k))*o.sampleRate/n;
        const double power=std::norm(X[k]);
        if(hz<o.startHz||hz>o.stopHz||power<maxPower*1e-13) { Y[k]=0; continue; }
        Y[k]=Y[k]*std::conj(X[k])/(power+lambda);
    }
    fft(Y,true);
    r.impulse.resize(n);
    r.impulsePeak=0;
    for(std::size_t i=0;i<n;i++) { r.impulse[i]=Y[i].real(); if(std::abs(r.impulse[i])>std::abs(r.impulse[r.impulsePeak]))r.impulsePeak=i; }
    r.frequencyHz.reserve(n/2+1); r.magnitude.reserve(n/2+1); r.magnitudeDb.reserve(n/2+1); r.phaseRadians.reserve(n/2+1);
    // Transfer response uses the full-resolution spectra (not the band-limited IR).
    for(std::size_t k=0;k<=n/2;k++) {
        const double hz=static_cast<double>(k)*o.sampleRate/n;
        r.frequencyHz.push_back(hz); r.magnitude.push_back(0); r.magnitudeDb.push_back(-300); r.phaseRadians.push_back(0);
    }
    // Reuse Y's storage to bound peak memory on the Force.
    std::fill(Y.begin(),Y.end(),C{});
    for(std::size_t i=0;i<y.size();i++)Y[i]=y[i];
    fft(Y,false);
    bool havePhase=false;double previousPhase=0.0,phaseOffset=0.0;
    for(std::size_t k=0;k<=n/2;k++) {
        const double hz=static_cast<double>(k)*o.sampleRate/n, p=std::norm(X[k]);
        if(hz<o.startHz||hz>o.stopHz||p<maxPower*1e-13)continue;
        const C h=Y[k]*std::conj(X[k])/(p+lambda);
        r.magnitude[k]=std::abs(h); r.magnitudeDb[k]=20*std::log10(std::max(r.magnitude[k],1e-15));
        const double wrapped=std::arg(h);
        if(havePhase){const double delta=wrapped-previousPhase;if(delta>pi)phaseOffset-=2*pi;else if(delta< -pi)phaseOffset+=2*pi;}
        r.phaseRadians[k]=wrapped+phaseOffset;previousPhase=wrapped;havePhase=true;
    }
    r.residualClockPpm=o.clockPpm;
    return true;
}

bool readCalibration(const std::string& path,int orientation,CalibrationCurve& curve,std::string* e) {
    curve=CalibrationCurve{};if(orientation!=0&&orientation!=90)return fail(e,"calibration orientation must be 0 or 90 degrees");
    std::ifstream input(path,std::ios::binary);if(!input)return fail(e,"cannot open microphone calibration file");const std::string contents((std::istreambuf_iterator<char>(input)),{});std::istringstream f(contents);std::string line;
    std::uint64_t hash=14695981039346656037ull;for(unsigned char b:contents){hash^=b;hash*=1099511628211ull;}curve.hashFnv1a64=hash;curve.sourcePath=path;
    while(std::getline(f,line)) {
        const auto tag=line.find("Sens Factor");if(tag!=std::string::npos){const auto eq=line.find('=',tag);if(eq!=std::string::npos){char*end=nullptr;double v=std::strtod(line.c_str()+eq+1,&end);if(end!=line.c_str()+eq+1&&std::isfinite(v))curve.sensitivityFactorDb=v;}continue;}
        const char*p=line.c_str();char*end=nullptr;double hz=std::strtod(p,&end);if(end==p||!std::isfinite(hz)||hz<=0)continue;p=end;double db=std::strtod(p,&end);if(end==p||!std::isfinite(db))continue;
        curve.points.push_back({hz,db});
    }
    std::sort(curve.points.begin(),curve.points.end(),[](const auto&a,const auto&b){return a.frequencyHz<b.frequencyHz;});
    if(curve.points.size()<2||!std::isfinite(curve.sensitivityFactorDb))return fail(e,"calibration file is missing response points or sensitivity factor");
    for(std::size_t i=1;i<curve.points.size();i++)if(curve.points[i].frequencyHz<=curve.points[i-1].frequencyHz)return fail(e,"calibration frequencies are not unique");
    curve.orientationDeg=orientation;return true;
}

double calibrationDb(const CalibrationCurve& curve,double hz) {
    if(curve.points.empty()||!std::isfinite(hz)||hz<=0)return 0.0;
    if(hz<=curve.points.front().frequencyHz)return curve.points.front().responseDb;
    if(hz>=curve.points.back().frequencyHz)return curve.points.back().responseDb;
    auto hi=std::lower_bound(curve.points.begin(),curve.points.end(),hz,[](const CalibrationPoint&p,double f){return p.frequencyHz<f;});
    auto lo=hi-1;double t=std::log(hz/lo->frequencyHz)/std::log(hi->frequencyHz/lo->frequencyHz);
    return lo->responseDb+t*(hi->responseDb-lo->responseDb);
}

bool applyCalibration(AnalysisResult& r,const CalibrationCurve& curve,std::string* e) {
    if(curve.orientationDeg!=0&&curve.orientationDeg!=90)return fail(e,"calibration curve has no valid microphone orientation");
    if(curve.points.size()<2||!std::isfinite(curve.sensitivityFactorDb))return fail(e,"invalid microphone calibration curve");
    if(r.frequencyHz.size()!=r.magnitude.size()||r.magnitude.size()!=r.magnitudeDb.size()||r.magnitudeDb.size()!=r.phaseRadians.size()||r.impulse.empty())return fail(e,"analysis result is incomplete");
    const std::size_t n=r.impulse.size();if(n/2+1!=r.frequencyHz.size())return fail(e,"analysis result FFT dimensions do not match");
    std::vector<C> h(n);
    for(std::size_t k=0;k<=n/2;k++) {
        if(r.magnitude[k]>0&&std::isfinite(r.magnitude[k])) {
            const double correction=-calibrationDb(curve,r.frequencyHz[k]);
            r.magnitude[k]*=std::pow(10.0,correction/20.0);r.magnitudeDb[k]+=correction;
            h[k]=std::polar(r.magnitude[k],r.phaseRadians[k]);
            if(k>0&&k<n/2)h[n-k]=std::conj(h[k]);
        }
    }
    fft(h,true);r.impulsePeak=0;for(std::size_t i=0;i<n;i++){r.impulse[i]=h[i].real();if(std::abs(r.impulse[i])>std::abs(r.impulse[r.impulsePeak]))r.impulsePeak=i;}
    r.calibrationOrientationDeg=curve.orientationDeg;r.calibrationSensitivityFactorDb=curve.sensitivityFactorDb;r.calibrationHashFnv1a64=curve.hashFnv1a64;r.calibrationSourcePath=curve.sourcePath;return true;
}

bool readWav(const std::string& path,std::vector<double>& mono,unsigned& fs,std::string* e) {
    std::ifstream f(path,std::ios::binary); if(!f)return fail(e,"cannot open WAV");
    unsigned char head[12]; if(!f.read(reinterpret_cast<char*>(head),12)||std::memcmp(head,"RIFF",4)||std::memcmp(head+8,"WAVE",4))return fail(e,"not a RIFF/WAVE file");
    uint16_t format=0,channels=0,bits=0; uint32_t rate=0; std::vector<unsigned char> data; bool haveFmt=false;
    while(f) {
        unsigned char ch[8]; if(!f.read(reinterpret_cast<char*>(ch),8))break;
        const uint32_t sz=u32(ch+4); if(sz>512u*1024u*1024u)return fail(e,"WAV chunk exceeds safety bound");
        if(!std::memcmp(ch,"fmt ",4)) { std::vector<unsigned char>b(sz);if(sz<16||!f.read(reinterpret_cast<char*>(b.data()),sz))return fail(e,"invalid WAV fmt chunk");format=u16(b.data());channels=u16(b.data()+2);rate=u32(b.data()+4);bits=u16(b.data()+14);haveFmt=true; }
        else if(!std::memcmp(ch,"data",4)){data.resize(sz);if(!f.read(reinterpret_cast<char*>(data.data()),sz))return fail(e,"truncated WAV data");}
        else f.seekg(sz,std::ios::cur);
        if(sz&1)f.seekg(1,std::ios::cur);
    }
    if(!haveFmt||data.empty()||!channels||!rate)return fail(e,"WAV is missing format or samples");
    const unsigned bytes=(bits+7)/8; if(!((format==1&&(bits==16||bits==24||bits==32))||(format==3&&bits==32))||bytes*channels==0)return fail(e,"only PCM16/24/32 and float32 WAV are supported");
    const std::size_t frames=data.size()/(bytes*channels);mono.resize(frames);
    for(std::size_t i=0;i<frames;i++) {
        const unsigned char*p=data.data()+i*bytes*channels; double sum=0;
        for(unsigned c=0;c<channels;c++) {
            const unsigned char*q=p+c*bytes; double v=0;
            if(format==3){float x;std::memcpy(&x,q,4);v=x;}
            else if(bits==16){int16_t x=static_cast<int16_t>(u16(q));v=x/32768.0;}
            else if(bits==24){int32_t x=static_cast<int32_t>(q[0]|(q[1]<<8)|(q[2]<<16));if(x&0x800000)x|=~0xffffff;v=x/8388608.0;}
            else {int32_t x=static_cast<int32_t>(u32(q));v=x/2147483648.0;}
            sum+=v;
        }
        mono[i]=sum/channels;
    }
    fs=rate;return true;
}

bool writeFloatWav(const std::string& path,const std::vector<double>& samples,unsigned fs,std::string* e) {
    if(samples.size()>(std::numeric_limits<uint32_t>::max()-44)/4||!fs)return fail(e,"float WAV is too large or has invalid sample rate");
    std::ofstream f(path,std::ios::binary);if(!f)return fail(e,"cannot write float WAV");
    const uint32_t bytes=static_cast<uint32_t>(samples.size()*4);f.write("RIFF",4);put32(f,36+bytes);f.write("WAVEfmt ",8);put32(f,16);put16(f,3);put16(f,1);put32(f,fs);put32(f,fs*4);put16(f,4);put16(f,32);f.write("data",4);put32(f,bytes);
    for(double d:samples){float x=static_cast<float>(d);f.write(reinterpret_cast<char*>(&x),4);}return static_cast<bool>(f)||fail(e,"failed writing float WAV");
}

bool writeArtifacts(const std::string& dir,const AnalysisResult& r,unsigned fs,double f0,double f1,std::string* e,unsigned smoothing) {
    std::error_code ec;std::filesystem::create_directories(dir,ec);if(ec)return fail(e,"cannot create output directory");
    if(!writeFloatWav(dir+"/impulse.wav",r.impulse,fs,e))return false;
    {std::ofstream f(dir+"/frequency-response.csv");if(!f)return fail(e,"cannot write response CSV");f<<"frequency_hz,magnitude,gain_db,phase_rad\n"<<std::setprecision(std::numeric_limits<double>::max_digits10);for(std::size_t i=0;i<r.frequencyHz.size();i++)if(r.frequencyHz[i]>=f0&&r.frequencyHz[i]<=f1)f<<r.frequencyHz[i]<<','<<r.magnitude[i]<<','<<r.magnitudeDb[i]<<','<<r.phaseRadians[i]<<'\n';}
    {
        std::ofstream f(dir+"/measurement.json");if(!f)return fail(e,"cannot write measurement metadata");
        f<<"{\n  \"format\": \"TROLLfx Sweep Lab V0.5\",\n  \"sample_rate_hz\": "<<fs<<",\n  \"start_hz\": "<<f0<<",\n  \"stop_hz\": "<<f1<<",\n  \"impulse_peak_sample\": "<<r.impulsePeak<<",\n  \"clipped_samples\": "<<r.clippedSamples<<",\n  \"input_peak\": "<<r.inputPeak<<",\n  \"clock_correction_ppm\": "<<r.residualClockPpm<<",\n  \"marker_alignment_valid\": "<<(r.markerAlignmentValid?"true":"false")<<",\n  \"captured_first_marker_sample\": ";
        if(r.markerAlignmentValid)f<<r.capturedFirstMarker;else f<<"null";
        f<<",\n  \"captured_second_marker_sample\": ";if(r.markerAlignmentValid)f<<r.capturedSecondMarker;else f<<"null";
        f<<",\n  \"marker_confidence\": ";if(r.markerAlignmentValid)f<<r.markerConfidence;else f<<"null";
        f<<",\n  \"microphone_frequency_correction_applied\": "<<(r.calibrationOrientationDeg>=0?"true":"false")<<",\n  \"calibration_orientation_deg\": ";
        if(r.calibrationOrientationDeg>=0)f<<r.calibrationOrientationDeg;else f<<"null";
        f<<",\n  \"sensitivity_factor_db_metadata_only\": ";if(std::isfinite(r.calibrationSensitivityFactorDb))f<<r.calibrationSensitivityFactorDb;else f<<"null";
        f<<",\n  \"calibration_hash_fnv1a64\": ";if(r.calibrationOrientationDeg>=0)f<<"\""<<std::hex<<std::setw(16)<<std::setfill('0')<<r.calibrationHashFnv1a64<<std::dec<<"\"";else f<<"null";
        f<<",\n  \"absolute_spl_status\": \"PROVISIONAL\"\n}\n";
    }
    ResponseCurve raw; if(!readResponseCsv(dir+"/frequency-response.csv",raw,e))return false;
    if(!writeSmoothedArtifacts(dir,raw,smoothing,1000.0,false,e))return false;
    return true;
}

namespace {
bool validResolution(unsigned n) { return n==0||n==3||n==6||n==12||n==24||n==48; }
double interpLog(const std::vector<double>& x,const std::vector<double>& y,double q) {
    auto it=std::lower_bound(x.begin(),x.end(),q); if(it==x.begin())return y.front(); if(it==x.end())return y.back();
    const std::size_t b=static_cast<std::size_t>(it-x.begin()),a=b-1;
    const double t=std::log(q/x[a])/std::log(x[b]/x[a]); return y[a]+t*(y[b]-y[a]);
}
std::string smoothingName(unsigned n) { if(!n)return "none"; return "1/"+std::to_string(n)+" octave"; }

bool jsonNumber(const std::string& json,const char* key,double& value) {
    const std::string quoted=std::string("\"")+key+"\"";
    const auto at=json.find(quoted); if(at==std::string::npos)return false;
    const auto colon=json.find(':',at+quoted.size()); if(colon==std::string::npos)return false;
    const char* begin=json.c_str()+colon+1; while(*begin&&std::isspace(static_cast<unsigned char>(*begin)))++begin;
    char* end=nullptr; value=std::strtod(begin,&end);
    return end!=begin&&std::isfinite(value);
}

// Older V0.5.2 exports wrote the frequency column with ostream's default
// precision. For long FFTs this rounded adjacent bins to identical text. Only
// repair that exact legacy serialization, and only when every CSV token
// matches the six-significant-digit rendering of the bin derived from the
// unchanged impulse WAV and measurement metadata.
bool reconstructLegacyAxis(const std::string& csvPath,const std::vector<std::string>& tokens,
                           std::vector<double>& axis,std::string* error) {
    const auto slash=csvPath.find_last_of('/'); const std::string dir=slash==std::string::npos?".":csvPath.substr(0,slash);
    std::ifstream mf(dir+"/measurement.json"); if(!mf)return fail(error,"legacy frequency axis is rounded and measurement metadata is unavailable");
    const std::string metadata((std::istreambuf_iterator<char>(mf)),{}); double fsMeta=0,startHz=0,stopHz=0;
    if(!jsonNumber(metadata,"sample_rate_hz",fsMeta)||!jsonNumber(metadata,"start_hz",startHz)||!jsonNumber(metadata,"stop_hz",stopHz)||fsMeta<=0||startHz<=0||stopHz<=startHz)
        return fail(error,"legacy frequency axis is rounded and sweep metadata is invalid");
    std::vector<double> impulse; unsigned fsWav=0;
    if(!readWav(dir+"/impulse.wav",impulse,fsWav,error))return false;
    if(impulse.size()<2||fsWav!=static_cast<unsigned>(std::llround(fsMeta)))return fail(error,"legacy frequency axis cannot be verified against impulse WAV sample rate");
    const double binHz=static_cast<double>(fsWav)/impulse.size();
    std::size_t token=0;
    for(std::size_t k=0;k<impulse.size();++k) {
        const double hz=k*binHz;
        if(hz<startHz||hz>stopHz)continue;
        if(token>=tokens.size())return fail(error,"legacy response row count does not match impulse FFT bins");
        std::ostringstream legacy; legacy<<std::setprecision(6)<<hz;
        if(legacy.str()!=tokens[token])return fail(error,"legacy frequency tokens do not match the impulse FFT bin grid");
        axis[token++]=hz;
    }
    if(token!=tokens.size()||token<2)return fail(error,"legacy response row count does not match impulse FFT bins");
    return true;
}
}

bool fractionalOctaveSmooth(const std::vector<double>& f,const std::vector<double>& m,unsigned n,std::vector<double>& out,std::string* e) {
    if(!validResolution(n)||f.size()!=m.size()||f.size()<2)return fail(e,"invalid response or smoothing resolution");
    for(std::size_t i=0;i<f.size();++i)if(!std::isfinite(f[i])||f[i]<=0||!std::isfinite(m[i])||m[i]<0||(i&&f[i]<=f[i-1]))return fail(e,"response frequencies must be positive, increasing, with finite nonnegative magnitudes");
    if(!n){out=m;return true;} out.resize(m.size()); const double half=std::log(2.0)/(2.0*n);
    std::vector<double> logf(f.size()),prefix(f.size(),0.0);for(std::size_t i=0;i<f.size();++i){logf[i]=std::log(f[i]);if(i)prefix[i]=prefix[i-1]+(logf[i]-logf[i-1])*(m[i]+m[i-1])*0.5;}
    auto integral=[&](double x){if(x<=logf.front())return 0.0;if(x>=logf.back())return prefix.back();auto it=std::upper_bound(logf.begin(),logf.end(),x);const std::size_t b=static_cast<std::size_t>(it-logf.begin()),a=b-1;const double t=(x-logf[a])/(logf[b]-logf[a]),y=m[a]+t*(m[b]-m[a]);return prefix[a]+(x-logf[a])*(m[a]+y)*0.5;};
    for(std::size_t k=0;k<f.size();++k) {
        const double center=logf[k],lo=std::max(logf.front(),center-half),hi=std::min(logf.back(),center+half);
        if(hi<=lo){out[k]=m[k];continue;}out[k]=(integral(hi)-integral(lo))/(hi-lo);
    }
    return true;
}

double responseDbAt(const std::vector<double>& f,const std::vector<double>& m,double q) {
    if(f.size()!=m.size()||f.empty()||q<f.front()||q>f.back()||q<=0)return std::numeric_limits<double>::quiet_NaN();
    const double v=interpLog(f,m,q);return v>0?20.0*std::log10(v):-std::numeric_limits<double>::infinity();
}

bool readResponseCsv(const std::string& path,ResponseCurve& r,std::string* e) {
    std::ifstream in(path);if(!in)return fail(e,"cannot open raw response CSV");std::string line;std::getline(in,line);
    if(line!="frequency_hz,magnitude,gain_db,phase_rad")return fail(e,"unsupported response CSV header");
    r=ResponseCurve{};std::vector<std::string> frequencyTokens;while(std::getline(in,line)){std::istringstream s(line);std::string a,b,c,d;if(!std::getline(s,a,',')||!std::getline(s,b,',')||!std::getline(s,c,',')||!std::getline(s,d,','))return fail(e,"malformed response CSV row");try{frequencyTokens.push_back(a);r.frequencyHz.push_back(std::stod(a));r.magnitude.push_back(std::stod(b));r.gainDb.push_back(std::stod(c));r.phaseRadians.push_back(std::stod(d));}catch(...){return fail(e,"invalid response CSV number");}}
    if(r.frequencyHz.size()<2)return fail(e,"incomplete response CSV");
    std::vector<double> check;
    if(!fractionalOctaveSmooth(r.frequencyHz,r.magnitude,0,check,nullptr)) {
        std::vector<double> reconstructed(r.frequencyHz.size());
        if(!reconstructLegacyAxis(path,frequencyTokens,reconstructed,e))return false;
        r.frequencyHz=std::move(reconstructed);r.frequencyAxisReconstructed=true;
        if(!fractionalOctaveSmooth(r.frequencyHz,r.magnitude,0,check,e))return false;
    }
    return true;
}

bool writeSmoothedArtifacts(const std::string& dir,const ResponseCurve& raw,unsigned n,double ref,bool rawTrace,std::string* e,bool updateMetadata) {
    static std::mutex outputMutex;
    std::lock_guard<std::mutex> outputLock(outputMutex);
    if(!validResolution(n)||!std::isfinite(ref)||ref<=0)return fail(e,"invalid smoothing or reference frequency");
    std::vector<double> sm;if(!fractionalOctaveSmooth(raw.frequencyHz,raw.magnitude,n,sm,e))return false;
    const double refMag=interpLog(raw.frequencyHz,sm,std::clamp(ref,raw.frequencyHz.front(),raw.frequencyHz.back()));
    {std::ofstream f(dir+"/frequency-response-smoothed.csv");if(!f)return fail(e,"cannot write smoothed CSV");f<<"frequency_hz,magnitude,gain_db,phase_rad\n";for(std::size_t i=0;i<sm.size();++i)f<<std::setprecision(12)<<raw.frequencyHz[i]<<','<<sm[i]<<','<<(sm[i]>0?20*std::log10(sm[i]):-300)<<','<<(i<raw.phaseRadians.size()?raw.phaseRadians[i]:0)<<'\n';}
    const double f0=std::max(20.0,raw.frequencyHz.front()),f1=std::min(20000.0,raw.frequencyHz.back());if(f1<=f0)return fail(e,"measurement has no valid 20 Hz to 20 kHz bandwidth");
    {std::ofstream f(dir+"/frequency-response.svg");if(!f)return fail(e,"cannot write response SVG");constexpr double left=86,right=1170,top=76,bottom=510;f<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"620\" viewBox=\"0 0 1200 620\"><rect width=\"100%\" height=\"100%\" fill=\"#101010\"/><text x=\"600\" y=\"30\" fill=\"white\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"21\">Sweep frequency response — "<<smoothingName(n)<<" smoothing</text>";
        auto X=[&](double hz){return left+(right-left)*std::log(hz/f0)/std::log(f1/f0);};auto Y=[](double db){return top+(30-db)*(bottom-top)/90.0;};
        for(int db=-60;db<=30;db+=10){double y=Y(db);f<<"<path d=\"M"<<left<<' '<<y<<" H"<<right<<"\" stroke=\"#444\"/><text x=\"78\" y=\""<<y+5<<"\" fill=\"#ddd\" text-anchor=\"end\" font-size=\"13\">"<<db<<" dB</text>";}
        f<<"<text x=\"20\" y=\"300\" fill=\"#ddd\" font-size=\"13\" text-anchor=\"middle\" transform=\"rotate(-90 20 300)\">Relative magnitude (dB re reference)</text>";
        const char* labels[]={"20","25","31.5","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1.25k","1.6k","2k","2.5k","3.15k","4k","5k","6.3k","8k","10k","12.5k","16k","20k"};
        for(int i=0;i<31;i++){double hz=20*std::pow(1000.0,i/10.0);if(hz<f0||hz>f1)continue;double x=X(hz);f<<"<path d=\"M"<<x<<' '<<top<<" V"<<bottom<<"\" stroke=\"#292929\"/><text x=\""<<x<<"\" y=\"548\" fill=\"#f2f2f2\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"12\" font-weight=\"600\">"<<labels[i]<<"</text>";}
        f<<"<text x=\"628\" y=\"582\" fill=\"#eee\" text-anchor=\"middle\" font-size=\"14\">Frequency (Hz) — logarithmic scale</text>";
        auto path=[&](const std::vector<double>& v,const char* color,double width,double opacity){f<<"<path d=\"";bool first=true;for(std::size_t i=0;i<v.size();++i){double hz=raw.frequencyHz[i];if(hz<f0||hz>f1||v[i]<=0)continue;double db=20*std::log10(v[i]/refMag);if(db<-60||db>30)db=std::clamp(db,-60.0,30.0);f<<(first?"M":" L")<<X(hz)<<' '<<Y(db);first=false;}f<<"\" fill=\"none\" stroke=\""<<color<<"\" stroke-width=\""<<width<<"\" opacity=\""<<opacity<<"\"/>";};
        if(rawTrace)path(raw.magnitude,"#8b5555",1,0.55);
        path(sm,"#ff3030",2.5,1);
        f<<"<text x=\"880\" y=\"52\" fill=\"#ddd\" font-size=\"12\">Shape reference: "<<ref<<" Hz = "<<(refMag>0?20*std::log10(refMag):0)<<" dB absolute</text></svg>";}
    if(!updateMetadata)return true;
    const std::string meta=dir+"/measurement.json";std::ifstream mf(meta);std::string json;if(mf)json.assign(std::istreambuf_iterator<char>(mf),{});else json="{\n}\n";
    const std::size_t close=json.find_last_of('}');if(close==std::string::npos)return fail(e,"invalid measurement metadata");json.erase(close);
    const char* fields[]={"response_smoothing_octaves","smoothing_algorithm_version","smoothing_method","graph_reference_hz","graph_reference_level_db","graph_raw_trace"};
    for(const char* field:fields){const std::string key=std::string("\"")+field+"\"";const auto at=json.find(key);if(at!=std::string::npos){const auto lineStart=json.rfind('\n',at);const auto lineEnd=json.find('\n',at);json.erase(lineStart==std::string::npos?0:lineStart+1,(lineEnd==std::string::npos?json.size():lineEnd+1)-(lineStart==std::string::npos?0:lineStart+1));}}
    while(!json.empty()&&std::isspace(static_cast<unsigned char>(json.back())))json.pop_back();
    if(!json.empty()&&json.back()==','){json.pop_back();while(!json.empty()&&std::isspace(static_cast<unsigned char>(json.back())))json.pop_back();}
    if(json.size()>1&&json.back()!='{')json+=",\n";
    json+="  \"response_smoothing_octaves\": "+(n?std::to_string(n):std::string("null"))+",\n  \"smoothing_algorithm_version\": 1,\n  \"smoothing_method\": \"log-frequency trapezoidal magnitude average\",\n  \"graph_reference_hz\": "+std::to_string(ref)+",\n  \"graph_reference_level_db\": "+std::to_string(refMag>0?20*std::log10(refMag):0)+",\n  \"graph_raw_trace\": "+(rawTrace?"true":"false")+"\n}\n";
    std::ofstream out(meta);if(!out)return fail(e,"cannot update measurement metadata");out<<json;return static_cast<bool>(out)||fail(e,"cannot write measurement metadata");
}

bool reprocessSession(const std::string& dir,unsigned n,double ref,bool rawTrace,std::string* e) {
    {std::ifstream metadata(dir+"/measurement.json");if(metadata){std::string json((std::istreambuf_iterator<char>(metadata)),{});const auto key=json.find("\"clipped_samples\"");if(key!=std::string::npos){const auto colon=json.find(':',key);if(colon!=std::string::npos){char* end=nullptr;const unsigned long long clipped=std::strtoull(json.c_str()+colon+1,&end,10);if(end!=json.c_str()+colon+1&&clipped>0)return fail(e,"clipped measurement cannot be smoothed or graphed as a valid response");}}}}
    ResponseCurve raw;if(!readResponseCsv(dir+"/frequency-response.csv",raw,e))return false;
    return writeSmoothedArtifacts(dir,raw,n,ref,rawTrace,e,!raw.frequencyAxisReconstructed);
}
} // namespace trollfx::sweep
