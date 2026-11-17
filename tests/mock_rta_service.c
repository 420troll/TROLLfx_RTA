#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>

static volatile sig_atomic_t running=1;
static void stop(int signal_number){(void)signal_number;running=0;}
int main(int argc,char**argv){
 int port=8096;for(int i=1;i+1<argc;i++)if(strcmp(argv[i],"--port")==0)port=atoi(argv[i+1]);
 const char*pid_path=getenv("TROLLFX_RTA_TEST_PIDFILE");int starts=0;if(pid_path){FILE*f=fopen(pid_path,"a+");if(f){rewind(f);char line[64];while(fgets(line,sizeof line,f))starts++;fprintf(f,"%ld\n",(long)getpid());fclose(f);}}
 if(starts==0&&getenv("TROLLFX_RTA_TEST_CRASH_FIRST")){struct timespec delay={0,500000000};nanosleep(&delay,NULL);return 0;}
 if(getenv("TROLLFX_RTA_TEST_NO_API")){for(;;)pause();}
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 2;int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
 struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons((unsigned short)port);
 if(bind(fd,(struct sockaddr*)&addr,sizeof addr)||listen(fd,8)){close(fd);return 3;}
 struct sigaction action={0};action.sa_handler=stop;sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);
 char body[2048],levels[512]="",peaks[512]="";for(int i=0;i<31;i++){char v[24];snprintf(v,sizeof v,"%s-30",i?",":"");strcat(levels,v);snprintf(v,sizeof v,"%s-25",i?",":"");strcat(peaks,v);}
 snprintf(body,sizeof body,"{\"ok\":%s,\"rms_dbfs\":-20,\"z_corrected_dbfs\":-20,\"a_corrected_dbfs\":-20,\"orientation\":0,\"calibrated\":false,\"sensitivity_factor_db\":2.7673,\"spl_z\":null,\"spl_a\":null,\"spl_reference_type\":\"none\",\"level\":[%s],\"peak\":[%s]}",getenv("TROLLFX_RTA_TEST_MIC_OFFLINE")?"false":"true",levels,peaks);
 while(running){int client=accept(fd,NULL,NULL);if(client<0)continue;char req[1024];(void)recv(client,req,sizeof req,0);char response[4096];int n=snprintf(response,sizeof response,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",strlen(body),body);(void)send(client,response,(size_t)n,0);close(client);}
 close(fd);return 0;
}
