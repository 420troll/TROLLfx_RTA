#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
typedef struct _snd_pcm snd_pcm_t;
#define N 8192
#define NB 31
#define PI 3.141592653589793
static volatile sig_atomic_t run=1;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static double bars[NB],peaks[NB],rms=-120;
static int mic=0;
static char status[180]="Starting";
static const double hz[NB]={20,25,31.5,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000};
static float ring[N],re[N],im[N];
static int pos=0,filled=0,hop=0;
static void fail(const char*s){pthread_mutex_lock(&mutex);mic=0;snprintf(status,sizeof(status),"%s",s);pthread_mutex_unlock(&mutex);}
static void fft(void){
 unsigned j=0;
 for(unsigned i=1;i<N;i++){unsigned b=N>>1;while(j&b){j^=b;b>>=1;}j^=b;if(j>i){float t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
 for(unsigned len=2;len<=N;len*=2){double c=cos(-2*PI/len),s=sin(-2*PI/len);for(unsigned i=0;i<N;i+=len){double wr=1,wi=0;for(unsigned k=0;k<len/2;k++){unsigned a=i+k,b=a+len/2;double tr=wr*re[b]-wi*im[b],ti=wr*im[b]+wi*re[b];re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;double w=wr*c-wi*s;wi=wr*s+wi*c;wr=w;}}}
}
static void analyze(void){
 double e=0,v[NB];
 for(int i=0;i<N;i++){float s=ring[(pos+i)%N];e+=s*s;re[i]=s*(0.5-0.5*cos(2*PI*i/(N-1)));im[i]=0;}
 fft();
 for(int b=0;b<NB;b++){int lo=(int)ceil(hz[b]/pow(2,1./6.)*N/48000.),hi=(int)floor(hz[b]*pow(2,1./6.)*N/48000.);if(lo<1)lo=1;if(hi<lo)hi=lo;if(hi>N/2)hi=N/2;double sum=0;int n=0;for(int k=lo;k<=hi;k++){sum+=re[k]*re[k]+im[k]*im[k];n++;}v[b]=20*log10(fmax(sqrt(sum/n)/(N*0.5),1e-9));if(v[b]<-100)v[b]=-100;if(v[b]>0)v[b]=0;}
 pthread_mutex_lock(&mutex);
 for(int b=0;b<NB;b++){double a=v[b]>bars[b] ? 0.7 : 0.32;bars[b]+=a*(v[b]-bars[b]);if(bars[b]>peaks[b])peaks[b]=bars[b];else peaks[b]=fmax(bars[b],peaks[b]-0.55);}
 rms=10*log10(fmax(e/N,1e-18));mic=1;strcpy(status,"Capture running");pthread_mutex_unlock(&mutex);
}
static void sample(float s){ring[pos]=s;pos=(pos+1)%N;if(filled<N)filled++;if(++hop>=1024&&filled==N){hop=0;analyze();}}
static void*capture(void*arg){
 const char*dev=arg;void*lib=dlopen("libasound.so.2",RTLD_NOW);
 if(!lib){fail("Missing libasound.so.2");return NULL;}
 int (*op)(snd_pcm_t**,const char*,int,int)=dlsym(lib,"snd_pcm_open");
 int (*fv)(const char*)=dlsym(lib,"snd_pcm_format_value");
 int (*sp)(snd_pcm_t*,int,int,unsigned,unsigned,int,unsigned)=dlsym(lib,"snd_pcm_set_params");
 int (*prep)(snd_pcm_t*)=dlsym(lib,"snd_pcm_prepare");
 int (*start)(snd_pcm_t*)=dlsym(lib,"snd_pcm_start");
 long (*rd)(snd_pcm_t*,void*,unsigned long)=dlsym(lib,"snd_pcm_readi");
 int (*drop)(snd_pcm_t*)=dlsym(lib,"snd_pcm_drop");
 int (*closepcm)(snd_pcm_t*)=dlsym(lib,"snd_pcm_close");
 const char*(*strerr)(int)=dlsym(lib,"snd_strerror");
 if(!op||!fv||!sp||!prep||!start||!rd||!drop||!closepcm||!strerr){fail("ALSA symbol missing");return NULL;}
 snd_pcm_t*p=NULL;int r=op(&p,dev,1,0);
 if(r>=0)r=sp(p,fv("S24_3LE"),3,2,48000,0,250000);
 if(r>=0)r=prep(p);
 if(r>=0)r=start(p);
 if(r<0){char msg[180];snprintf(msg,sizeof(msg),"ALSA setup: %s",strerr(r));fail(msg);if(p)closepcm(p);return NULL;}
 uint8_t buf[1024*6];
 while(run){long n=rd(p,buf,1024);if(n<0){char msg[180];snprintf(msg,sizeof(msg),"ALSA read: %s",strerr((int)n));fail(msg);break;}for(long i=0;i<n;i++){uint8_t*q=buf+i*6;int32_t s=q[0]|(q[1]<<8)|(q[2]<<16);if(s&0x800000)s|=(int32_t)0xff000000;sample(s/8388608.f);}}
 drop(p);closepcm(p);dlclose(lib);return NULL;
}
static const char html[]=
"<!doctype html><meta name='viewport' content='width=device-width,initial-scale=1'><title>TROLLfx RTA</title><style>*{box-sizing:border-box}body{margin:0;background:#080506;color:#ff6661;font-family:Arial,sans-serif}header{padding:10px 15px;border-bottom:1px solid #522;display:flex;justify-content:space-between;align-items:center}h1{margin:0;font-size:24px;letter-spacing:2px}main{padding:8px}.chart{display:flex;gap:2px;height:67vh;min-height:260px;border:1px solid #522;padding:8px;background:#090404}.bar{flex:1;min-width:0;display:flex;flex-direction:column}.stack{flex:1;display:flex;flex-direction:column-reverse;gap:2px;position:relative}.seg{flex:1;background:#2b090b}.seg.on{background:#ed2229;box-shadow:0 0 4px #8c1118}.pk{position:absolute;left:0;right:0;height:3px;background:#ffad68}.f{text-align:center;color:#aa7777;font-size:clamp(6px,1vw,10px);padding-top:6px;white-space:nowrap}footer{display:flex;align-items:center;gap:15px;flex-wrap:wrap;padding:12px}button{padding:12px;background:#321012;color:#ff9990;border:1px solid #922;border-radius:4px}input{accent-color:red}#error{padding:10px;color:#c99}</style>"
"<header><h1>TROLLfx RTA</h1><span id='state'>CONNECTING</span><span>UMIK-1 · 48k · 1/3 OCT</span></header><main><div class='chart' id='chart'></div><footer><strong id='level'>LEVEL -- dBFS</strong><button id='hold'>PEAK HOLD ON</button><button id='reset'>RESET PEAK</button><label>DISPLAY GAIN <input type='range' min='0' max='60' value='25' id='gain'><output id='gv'>+25 dB</output></label><label>LED ATTACK <input type='range' min='0' max='400' step='10' value='50' id='attack'><output id='av'>50 ms</output></label><label>LED FALL <input type='range' min='2' max='60' value='18' id='fall'><output id='fv'>18 dB/s</output></label><label>PEAK HOLD <input type='range' min='0' max='3000' step='100' value='750' id='holdms'><output id='hv'>750 ms</output></label><label>PEAK FALL <input type='range' min='2' max='60' value='12' id='peakfall'><output id='pv'>12 dB/s</output></label><small>31 BANDS · RELATIVE dBFS · NOT CALIBRATED SPL</small></footer></main><div id='error'></div>"
"<script>const freq=['20','25','31','40','50','63','80','100','125','160','200','250','315','400','500','630','800','1k','1.25k','1.6k','2k','2.5k','3.15k','4k','5k','6.3k','8k','10k','12.5k','16k','20k'];let gain=25,hold=true,B=[],shown=Array(31).fill(-120),pkval=Array(31).fill(-120),pktime=Array(31).fill(0),last=performance.now();let attackMs=50,fallRate=18,holdMs=750,peakFall=12;for(let i=0;i<31;i++){let a=document.createElement('div');a.className='bar';let s=document.createElement('div');s.className='stack';let c=[];for(let k=0;k<24;k++){let t=document.createElement('div');t.className='seg';s.append(t);c.push(t)}let pk=document.createElement('div');pk.className='pk';pk.hidden=true;s.append(pk);let f=document.createElement('div');f.className='f';f.textContent=i%2===0||innerWidth>900?freq[i]:'';a.append(s,f);chart.append(a);B.push({c,pk});}document.getElementById('gain').oninput=e=>{gain=+e.target.value;gv.textContent='+'+gain+' dB';};document.getElementById('attack').oninput=e=>{attackMs=+e.target.value;av.textContent=attackMs+' ms';};document.getElementById('fall').oninput=e=>{fallRate=+e.target.value;fv.textContent=fallRate+' dB/s';};document.getElementById('holdms').oninput=e=>{holdMs=+e.target.value;hv.textContent=holdMs+' ms';};document.getElementById('peakfall').oninput=e=>{peakFall=+e.target.value;pv.textContent=peakFall+' dB/s';};document.getElementById('hold').onclick=()=>{hold=!hold;document.getElementById('hold').textContent='PEAK HOLD '+(hold?'ON':'OFF');};reset.onclick=()=>{pkval=shown.slice();pktime.fill(performance.now());fetch('/api/reset',{method:'POST'});};function h(x){return Math.max(0,Math.min(24,Math.round((x+gain+90)*24/80)))}async function tick(){try{let d=await(await fetch('/api/spectrum',{cache:'no-store'})).json();state.textContent=d.ok?'MIC LIVE':'MIC OFFLINE';error.textContent=d.error;level.textContent='LEVEL '+d.rms.toFixed(1)+' dBFS';let now=performance.now(),dt=Math.min((now-last)/1000,0.5);last=now;for(let i=0;i<31;i++){let target=d.level[i];if(target>shown[i]){let alpha=attackMs===0?1:1-Math.exp(-dt*1000/attackMs);shown[i]+=alpha*(target-shown[i]);}else shown[i]=Math.max(target,shown[i]-fallRate*dt);if(shown[i]>pkval[i]){pkval[i]=shown[i];pktime[i]=now;}else if(now-pktime[i]>holdMs)pkval[i]=Math.max(shown[i],pkval[i]-peakFall*dt);let n=h(shown[i]),p=h(pkval[i]);for(let j=0;j<24;j++)B[i].c[j].className='seg'+(j<n?' on':'');B[i].pk.hidden=!hold||p===0;B[i].pk.style.bottom=(p*100/24)+'%';}}catch(e){state.textContent='DISCONNECTED'}setTimeout(tick,80)}tick();</script>";
static void sendall(int fd,const char*str){size_t len=strlen(str);while(len){ssize_t n=send(fd,str,len,0);if(n<=0)return;str+=n;len-=n;}}
static void reply(int fd,const char*type,const char*data){char h[256];snprintf(h,sizeof h,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",type,strlen(data));sendall(fd,h);sendall(fd,data);}
static void handle(int fd){char req[1024];int n=recv(fd,req,sizeof(req)-1,0);if(n<=0)return;req[n]=0;if(!strncmp(req,"GET /api/spectrum ",18)){char b[2500];int off;pthread_mutex_lock(&mutex);off=snprintf(b,sizeof(b),"{\"ok\":%s,\"rms\":%.2f,\"level\":[",mic?"true":"false",rms);for(int i=0;i<NB;i++)off+=snprintf(b+off,sizeof(b)-off,"%s%.1f",i?",":"",bars[i]);off+=snprintf(b+off,sizeof(b)-off,"],\"peak\":[");for(int i=0;i<NB;i++)off+=snprintf(b+off,sizeof(b)-off,"%s%.1f",i?",":"",peaks[i]);snprintf(b+off,sizeof(b)-off,"],\"error\":\"%s\"}",status);pthread_mutex_unlock(&mutex);reply(fd,"application/json",b);}else if(!strncmp(req,"POST /api/reset ",16)){pthread_mutex_lock(&mutex);for(int i=0;i<NB;i++)peaks[i]=bars[i];pthread_mutex_unlock(&mutex);reply(fd,"application/json","{\"ok\":true}");}else reply(fd,"text/html; charset=utf-8",html);}
static void term(int sig){(void)sig;run=0;}
int main(int argc,char**argv){
 const char*dev="hw:U18dB,0";int port=8096;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--device")&&i+1<argc)dev=argv[++i];else if(!strcmp(argv[i],"--port")&&i+1<argc)port=atoi(argv[++i]);}
 for(int i=0;i<NB;i++)bars[i]=peaks[i]=-90;
 signal(SIGINT,term);signal(SIGTERM,term);signal(SIGPIPE,SIG_IGN);
 pthread_t t;if(pthread_create(&t,NULL,capture,(void*)dev)){perror("pthread_create");return 1;}
 int srv=socket(AF_INET,SOCK_STREAM,0);if(srv<0){perror("socket");return 1;}int yes=1;setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(port);if(bind(srv,(void*)&addr,sizeof addr)||listen(srv,8)){perror("bind/listen");return 1;}fprintf(stderr,"TROLLfx RTA listening on 127.0.0.1:%d\n",port);
 while(run){fd_set f;FD_ZERO(&f);FD_SET(srv,&f);struct timeval timeout={1,0};if(select(srv+1,&f,NULL,NULL,&timeout)>0){int fd=accept(srv,NULL,NULL);if(fd>=0){handle(fd);close(fd);}}}
 close(srv);pthread_cancel(t);pthread_join(t,NULL);return 0;
}