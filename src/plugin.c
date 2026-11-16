/* PS2 Disc Cache 02. GPL-3.0-or-later. VSH user process only. */
#ifdef HOST_TEST
#include "mock_platform.h"
#else
#include <sys/file.h>
#include <sys/thread.h>
#endif
#include "identity.h"
#define ROOT "/dev_hdd0/PS2ISO"
#define LOG "/dev_hdd0/tmp/ps2disc_cache.log"
#define DRIVE UINT64_C(0x101000000000006)
#define READ_CHUNK_SECTORS 16
static volatile int running;
static u64 worker_id;
static unsigned char transfer[READ_CHUNK_SECTORS*2048] __attribute__((aligned(128)));
/* Copy buffer: up to 512 KB allocated for the copy (sys_memory_allocate, 64 KB
 * pages), falling back to the 32 KB static buffer. */
static unsigned char *xfer=transfer;
static u32 xfer_max=READ_CHUNK_SECTORS;
static u32 xfer_alloc;
static char selected[1024];
static int (*notify_fn)(int,const char*);
static int (*mode_fn)(void);
static opd32 worker_entry_opd;
static int (*space_fn)(const char*,u32*,u64*);
static int (*socket_fn)(int,int,int);
static int (*connect_fn)(int,const void*,u32);
static int (*send_fn)(int,const void*,u32,int);
static int (*close_fn)(int);
static int (*pad_fn)(u32,void*);
void *memcpy(void *d,const void *s,size_t n) {unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n) {unsigned char *a=d;while(n--)*a++=v;return d;}
static unsigned slen(const char *s) {unsigned n=0;while(s[n])n++;return n;}
static int eq(const char *a,const char *b) {while(*a&&*a==*b)a++,b++;return *a==*b;}
static int cat(char *d,unsigned cap,const char *s) {unsigned n=slen(d),m=slen(s);if(n+m>=cap)return -1;memcpy(d+n,s,m+1);return 0;}
static void hex(char *p,u64 x,unsigned digits) {for(unsigned i=0;i<digits;i++)p[digits-1-i]="0123456789abcdef"[(x>>(i*4))&15];p[digits]=0;}
static void sleep_ms(unsigned ms) {lv2syscall1(141,(u64)ms*1000);}
/* sys_time_get_current_time (145): seconds, for copy speed in the log. */
static u64 now_s(void) {u64 sec=0,nsec=0;lv2syscall2(145,(u64)&sec,(u64)&nsec);(void)p1;return sec;}
static u64 copy_start;
static int types(u32 *r,u32 *e,u32 *f) {lv2syscall4(8,0x7020,(u64)r,(u64)e,(u64)f);return (int)p1;}
static int emu_state(void *s) {lv2syscall2(8,0x7023,(u64)s);return (int)p1;}
static int drive_open(u32 *h) {lv2syscall4(600,DRIVE,0,(u64)h,0);return (int)p1;}
static void drive_close(u32 h) {lv2syscall1(601,h);}
static void logmsg(const char *msg) {s32 fd;u64 w;if(!sysLv2FsOpen(LOG,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_APPEND,&fd,0666,0,0)){sysLv2FsWrite(fd,msg,slen(msg),&w);sysLv2FsWrite(fd,"\n",1,&w);sysLv2FsClose(fd);}}
static int notify_traced;
static void report(const char *msg) {logmsg(msg);if(notify_fn){if(!notify_traced)logmsg("trace: notify call");notify_fn(0,msg);if(!notify_traced){notify_traced=1;logmsg("trace: notify returned");}}}
static void error(const char *msg,int rc) {char b[200]="PS2 cache: ",h[9];cat(b,sizeof b,msg);cat(b,sizeof b," rc=");hex(h,(u32)rc,8);cat(b,sizeof b,h);report(b);}
#ifndef HOST_TEST
extern int sys_thread_create_ex(u64*,opd32*,void*,s32,u64,u64,const char*);
extern int imp_vshtask_notify(int,const char*);
extern int imp_vshmain_mode(void);
extern int imp_fs_free_size(const char*,u32*,u64*);
extern int imp_socket(int,int,int);
extern int imp_connect(int,const void*,u32);
extern int imp_send(int,const void*,u32,int);
extern int imp_socketclose(int);
extern int imp_pad_get_data(u32,void*);
#endif
static int cancelled(void) {return !running || mode_fn()!=0;}
/* The cellSysutil message dialog hangs the XMB when opened from a plugin
 * thread (08c/09 traces, live and boot load). Ask with a notification and
 * accept R3 on any controller within 30 s. The XMB reads the same pad
 * buffer, so a press can be missed; the message says to press again. */
static void dec(char *o,u64 v){char r[24];unsigned n=0,i=0;do{r[n++]='0'+v%10;v/=10;}while(v);while(n){o[i++]=r[--n];}o[i]=0;}
typedef struct {s32 len;u16 button[64];} pad_data;
#define CONFIRM_MS 30000
#define PAD_POLL_MS 20
static int r3_pressed(void) {
 pad_data d;
 for(u32 port=0;port<4;port++){memset(&d,0,sizeof d);if(!pad_fn(port,&d)&&d.len>2&&(d.button[2]&0x0004))return 1;}
 return 0;
}
static int confirm_copy(const char *serial) {
 char message[200]="PS2 cache: ";cat(message,sizeof message,serial);
 cat(message,sizeof message," not on HDD. Asking to copy (R3, 30 s).");
 logmsg(message);
 /* Re-send the prompt every 4 s with a countdown so it stays on screen. */
 for(unsigned t=0;t<CONFIRM_MS&&running;t+=PAD_POLL_MS) {
  if(t%4000==0&&notify_fn){
   char m[200]="",s[8];unsigned left=(CONFIRM_MS-t)/1000;
   cat(m,sizeof m,serial);cat(m,sizeof m," not on HDD.\nPress R3 to copy it to the HDD (");
   dec(s,left);cat(m,sizeof m,s);cat(m,sizeof m," s left).");
   notify_fn(0,m);
  }
  if(r3_pressed()){logmsg("PS2 cache: R3 pressed, copy confirmed");return 1;}
  if(mode_fn()!=0)return 0;
  if(t%500==0){u32 r,e,f;if(types(&r,&e,&f)||r!=0xff61||f){logmsg("PS2 cache: disc removed while asking");return 0;}}
  sleep_ms(PAD_POLL_MS);
 }
 report("PS2 cache: copy skipped. Reinsert the disc to be asked again.");
 return 0;
}
static int real_read(void *ctx,uint32_t lba,uint32_t count,void *buf) {
 u32 got=0;int rc=-1;
 if(!count||count>xfer_max)return -96;
 for(unsigned retry=0;retry<3;retry++) {
  if(!running)return -90;
  {lv2syscall7(602,*(u32*)ctx,0,lba,count,(u64)xfer,(u64)&got,2);rc=(int)p1;}
  if(!rc&&got==count){if(buf!=xfer)memcpy(buf,xfer,(size_t)count*2048);return 0;}
  if(!rc)rc=-91;
  sleep_ms(100);
 }
 char line[96]="DVD read failure: lba=",v[9];hex(v,lba,8);cat(line,sizeof line,v);cat(line,sizeof line," rc=");hex(v,(u32)rc,8);cat(line,sizeof line,v);logmsg(line);
 return rc;
}
static int file_read(void *ctx,uint32_t lba,uint32_t count,void *buf) {
 u64 pos,got;int rc=sysLv2FsLSeek64(*(s32*)ctx,(u64)lba*2048,0,&pos);
 if(rc)return rc;
 rc=sysLv2FsRead(*(s32*)ctx,buf,(u64)count*2048,&got);return rc?rc:(got==(u64)count*2048?0:-92);
}
static int iso_suffix(const char *s) {unsigned n=slen(s);return n>=4&&s[n-4]=='.'&&(s[n-3]=='i'||s[n-3]=='I')&&(s[n-2]=='s'||s[n-2]=='S')&&(s[n-1]=='o'||s[n-1]=='O');}
static int scan(const char *dir,const disc_id *disc,unsigned depth) {
 s32 dd,fd;u64 n;sysFSDirent ent;sysFSStat st;char path[1024];disc_id found;
 if(depth>4||sysLv2FsOpenDir(dir,&dd))return 0;
 int match=0;
 while(running&&!sysLv2FsReadDir(dd,&ent,&n)&&n) {
  if(cancelled()){match=-1;break;} if(ent.d_name[0]=='.')continue;
  path[0]=0;if(cat(path,sizeof path,dir)||cat(path,sizeof path,"/")||cat(path,sizeof path,ent.d_name))continue;
  if(sysLv2FsStat(path,&st))continue;
  if((st.st_mode&0170000)==0040000) {match=scan(path,disc,depth+1);if(match)break;continue;}
  if(!iso_suffix(path)||st.st_size<(u64)disc->blocks*2048)continue;
  {char t[1100]="trace: checking ";cat(t,sizeof t,path);logmsg(t);}
  if(sysLv2FsOpen(path,SYS_O_RDONLY,&fd,0,0,0))continue;
  int rc=identify(file_read,&fd,&found);sysLv2FsClose(fd);
  if(!rc&&same_disc(disc,&found)){memcpy(selected,path,slen(path)+1);match=1;break;}
 }
 sysLv2FsCloseDir(dd);return match;
}
static int still_disc(u32 *h,const disc_id *id) {disc_id check;u32 r,e,f;return !types(&r,&e,&f)&&r==0xff61&&!f&&!identify(real_read,h,&check)&&same_disc(id,&check);}
static void gb(char *out,u64 sectors) {
 u64 tenths=sectors*2048*10/1073741824;char t[24];unsigned i=0;
 u64 whole=tenths/10;char rev[20];unsigned n=0;do{rev[n++]='0'+whole%10;whole/=10;}while(whole);
 while(n){t[i++]=rev[--n];}
 t[i++]='.';t[i++]='0'+tenths%10;t[i]=0;cat(out,200,t);
}
/* Notifications fade after a few seconds, so show one at every 5% step,
 * with the amount done, so progress stays visible. */
static void progress(const char *phase,u32 done,u32 total) {
 char b[128]="PS2 cache: ",n[4];unsigned p=(u64)done*100/total;
 n[0]='0'+p/100;n[1]='0'+p/10%10;n[2]='0'+p%10;n[3]=0;
 cat(b,sizeof b,phase);cat(b,sizeof b," ");cat(b,sizeof b,n+(p>=100?0:p>=10?1:2));cat(b,sizeof b,"% (");
 gb(b,done);cat(b,sizeof b," / ");gb(b,total);cat(b,sizeof b," GB)");
 logmsg(b);
 {u64 e=now_s()-copy_start,r=e?(u64)done*2048*10/1048576/e:0;char t[96]="PS2 cache:   elapsed ",h[24];
  dec(h,e);cat(t,sizeof t,h);cat(t,sizeof t," s, ");dec(h,r/10);cat(t,sizeof t,h);cat(t,sizeof t,".");
  dec(h,r%10);cat(t,sizeof t,h);cat(t,sizeof t," MB/s");logmsg(t);}
}
#define HEARTBEAT_S 4
static u64 last_beat;
static void heartbeat(const char *phase,u32 done,u32 total) {
 u64 t=now_s();if(t-last_beat<HEARTBEAT_S)return;last_beat=t;
 char b[200]="",n[4];unsigned p=(u64)done*100/total;
 n[0]='0'+p/100;n[1]='0'+p/10%10;n[2]='0'+p%10;n[3]=0;
 cat(b,sizeof b,phase);cat(b,sizeof b," ");cat(b,sizeof b,n+(p>=100?0:p>=10?1:2));cat(b,sizeof b,"% (");
 gb(b,done);cat(b,sizeof b," / ");gb(b,total);cat(b,sizeof b," GB)");
 cat(b,sizeof b,"\nDo not launch the game until the ISO is copied and verified.\nEject the disc to cancel.");
 if(notify_fn)notify_fn(0,b);
}
static void big_buffer(void) {
 static const u32 sizes[3]={512*1024,256*1024,128*1024};
 for(unsigned i=0;i<3;i++){u32 a=0;
  {lv2syscall3(348,sizes[i],0x200,(u64)&a);if(!p1&&a){xfer=(unsigned char*)(u64)a;xfer_max=sizes[i]/2048;xfer_alloc=a;break;}}}
 char t[64]="PS2 cache: read chunk ",h[24];dec(h,(u64)xfer_max*2);cat(t,sizeof t,h);cat(t,sizeof t," KB");logmsg(t);
}
static void small_buffer(void) {
 if(xfer_alloc){lv2syscall1(349,xfer_alloc);(void)p1;}
 xfer=transfer;xfer_max=READ_CHUNK_SECTORS;xfer_alloc=0;
}
/* Readable file name from the PVD volume label: "<label> [SERIAL].iso".
 * Matching is by content, so the name is cosmetic; labels that are empty,
 * generic or just the serial fall back to "SERIAL.iso". */
static int alnum(char c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static void nice_label(const char *raw,const char *serial,char *out,unsigned cap) {
 unsigned n=0;int space=1;
 for(unsigned i=0;raw[i]&&n+1<cap;i++){char c=raw[i];
  if(c=='_'||c==' '){if(!space){out[n++]=' ';space=1;}continue;}
  if(alnum(c)||c=='-'||c=='.'||c=='('||c==')'||c=='!'||c=='&'||c=='+'||c==','||c=='\''){out[n++]=c;space=0;}
 }
 while(n&&out[n-1]==' '){n--;}
 out[n]=0;
 char a[40],b[40];unsigned x=0,y=0;
 for(unsigned i=0;out[i]&&x<39;i++){if(alnum(out[i]))a[x++]=(out[i]>='a'&&out[i]<='z')?out[i]-32:out[i];}
 a[x]=0;
 for(unsigned i=0;serial[i]&&y<39;i++){if(alnum(serial[i]))b[y++]=serial[i];}
 b[y]=0;
 static const char *generic[]={"CDROM","DVDROM","DVD","CD","PS2","PLAYSTATION","PLAYSTATION2","DISC","GAME",0};
 int bad=x<3||(x==y&&equal_bytes(a,b,y));
 if(x>=y&&y&&equal_bytes(a,b,y))bad=1;
 for(unsigned i=0;generic[i];i++){if(equal_bytes(a,generic[i],slen(generic[i])+1))bad=1;}
 if(bad)out[0]=0;
}
static int rip(u32 *handle,const disc_id *id) {
 u32 blocksize;u64 freeblocks;int rc;char tmp[1100],h[17];s32 fd=-1;u64 wrote,hash=UINT64_C(14695981039346656037);
 if(!space_fn||space_fn("/dev_hdd0",&blocksize,&freeblocks)||!blocksize||freeblocks<((u64)id->blocks*2048+64*1024*1024)/blocksize+1){report("PS2 cache: insufficient HDD space (or space query failed).");return -1;}
 sysFSStat st;char label[40];nice_label(id->label,id->serial,label,sizeof label);hex(h,id->fingerprint,16);
 /* Partial file keeps the identity name, so a stale one can only be this disc's. */
 tmp[0]=0;cat(tmp,sizeof tmp,ROOT "/");cat(tmp,sizeof tmp,id->serial);cat(tmp,sizeof tmp,"-");cat(tmp,sizeof tmp,h);cat(tmp,sizeof tmp,".iso.part");
 for(int attempt=0;;attempt++){
  selected[0]=0;cat(selected,sizeof selected,ROOT "/");
  if(label[0]){cat(selected,sizeof selected,label);cat(selected,sizeof selected," [");cat(selected,sizeof selected,id->serial);cat(selected,sizeof selected,"]");}
  else cat(selected,sizeof selected,id->serial);
  if(attempt){char s8[9];memcpy(s8,h,8);s8[8]=0;cat(selected,sizeof selected," ");cat(selected,sizeof selected,s8);}
  cat(selected,sizeof selected,".iso");
  if(sysLv2FsStat(selected,&st))break;
  if(attempt){report("PS2 cache: target filename already exists but did not match. It was not overwritten.");return -2;}
 }
 sysLv2FsMkdir(ROOT,0777);
 /* A stale partial belongs only to this exact generated identity filename. */
 sysLv2FsUnlink(tmp);
 rc=sysLv2FsOpen(tmp,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_EXCL,&fd,0666,0,0);if(rc){error("create partial",rc);return rc;}
 report("PS2 cache: copying to HDD. Do not launch the game until the ISO is copied and verified. Eject the disc to cancel.");
 unsigned next=0;copy_start=now_s();big_buffer();u32 chunk=xfer_max,chunks=0;
 for(u32 lba=0;lba<id->blocks;) {
  if(cancelled()){rc=-90;break;}
  u32 count=id->blocks-lba;if(count>chunk)count=chunk;
  rc=real_read(handle,lba,count,xfer);
  if(rc&&chunk>READ_CHUNK_SECTORS){chunk/=2;char t[64]="PS2 cache: read failed, chunk now ",h[24];dec(h,(u64)chunk*2);cat(t,sizeof t,h);cat(t,sizeof t," KB");logmsg(t);continue;}
  if(rc)break;
  rc=sysLv2FsWrite(fd,xfer,(u64)count*2048,&wrote);if(rc||wrote!=(u64)count*2048){if(!rc)rc=-93;break;}
  hash=hash_bytes(hash,xfer,count*2048);lba+=count;
  if(++chunks*(u64)chunk*2048>=8u*1024*1024){chunks=0;sleep_ms(1);}
  heartbeat("Copying",lba,id->blocks);
  unsigned pct=(u64)lba*100/id->blocks;
  if(pct>=next){progress("Copying",lba,id->blocks);next=pct+5;if(!still_disc(handle,id)){rc=-94;break;}}

 }
 if(!rc)rc=sysLv2FsFsync(fd);
 sysLv2FsClose(fd);fd=-1;
 if(!rc)report("PS2 cache: copy done, verifying ISO...");
 if(!rc)rc=sysLv2FsOpen(tmp,SYS_O_RDONLY,&fd,0,0,0);
 if(!rc) {
  u64 check=UINT64_C(14695981039346656037);next=0;
  for(u32 lba=0;lba<id->blocks;) {
   if(cancelled()){rc=-90;break;}u32 count=id->blocks-lba;if(count>xfer_max)count=xfer_max;
   rc=file_read(&fd,lba,count,xfer);if(rc)break;check=hash_bytes(check,xfer,count*2048);lba+=count;
   heartbeat("Verifying",lba,id->blocks);
   unsigned pct=(u64)lba*100/id->blocks;if(pct>=next){progress("Verifying",lba,id->blocks);next=pct+5;}
  }
  if(!rc&&check!=hash)rc=-95;
  sysLv2FsClose(fd);
 }
 if(!rc&&!still_disc(handle,id))rc=-94;
 if(!rc&&!sysLv2FsStat(selected,&st))rc=-97;
 if(!rc)rc=sysLv2FsRename(tmp,selected);
 small_buffer();
 if(rc){sysLv2FsUnlink(tmp);error("copy cancelled/failed; incomplete image removed",rc);}
 return rc;
}
static int mounted_path(const char *path) {
 struct {s32 size,mode;char path[0x420];} state;memset(&state,0,sizeof state);state.size=sizeof state;
 return !emu_state(&state)&&state.mode&&eq(state.path,path);
}
static int mount_request(const char *path) {
 /* Use webMAN's normal PS2 mounting path, retaining its CONFIG/fan handling. */
 char req[3400]="GET /mount.ps3",h[4];
 for(const unsigned char *p=(const unsigned char*)path;*p;p++) {
  if((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='/'||*p=='_'||*p=='.'||*p=='-'){h[0]=*p;h[1]=0;}else{h[0]='%';hex(h+1,*p,2);}if(cat(req,sizeof req,h))return -1;
 }
 cat(req,sizeof req," HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
 unsigned char addr[16]={16,2,0,80,127,0,0,1};int s=socket_fn(2,1,0);if(s<0)return -2;
 int rc=connect_fn(s,addr,16);unsigned off=0;
 while(!rc&&off<slen(req)){int n=send_fn(s,req+off,slen(req)-off,0);if(n<=0){rc=-3;break;}off+=n;}
 close_fn(s);return rc;
}
static int mount_image(void) {
 int rc=mount_request(selected);if(rc)return rc;
 for(unsigned i=0;i<60&&running;i++){if(mounted_path(selected))return 0;sleep_ms(250);}
 return -4;
}
static void worker(void *unused) {
 (void)unused;int handled=0,owned=0;u32 r,e,f;char msg[160];
 sleep_ms(3000);
 /* Proper PRX imports (module.S); the manual memory search crashed the XMB. */
 notify_fn=imp_vshtask_notify;mode_fn=imp_vshmain_mode;
 space_fn=imp_fs_free_size;socket_fn=imp_socket;connect_fn=imp_connect;send_fn=imp_send;close_fn=imp_socketclose;pad_fn=imp_pad_get_data;
 logmsg("imports bound");
 if(!notify_fn||!mode_fn||!pad_fn||!space_fn||!socket_fn||!connect_fn||!send_fn||!close_fn){report("PS2 cache: required XMB dialog export missing; worker stopped.");goto done;}
 report("PS2 Disc Cache 21 ready. Insert a PS2 DVD.");
 unsigned loops=0;
 while(running) {
  sleep_ms(500);loops++;
  if(loops==1)logmsg("trace: mode call");
  int m=mode_fn();
  if(loops==1){char t[48]="trace: mode returned ",h[9];hex(h,(u32)m,8);cat(t,sizeof t,h);logmsg(t);}
  if(m!=0)continue;
  if(loops<=2)logmsg("trace: types call");
  int tr=types(&r,&e,&f);
  if(loops<=2){char t[64]="trace: types rc ",h[9];hex(h,(u32)tr,8);cat(t,sizeof t,h);cat(t,sizeof t," real ");hex(h,r,8);cat(t,sizeof t,h);logmsg(t);}
  if(loops==20)logmsg("trace: 20 loops ok");
  if(tr){handled=0;continue;}
  if(r!=0xff61){
   handled=0;
   if(owned&&mounted_path(selected)){
    int urc=mount_request("/unmount");
    if(urc)error("unmount after ejection",urc);
    else {sleep_ms(1000);owned=mounted_path(selected);}
   }else owned=0;
   continue;
  }
  if(handled||f)continue;
  handled=1;sleep_ms(2000);
  u32 handle;int rc=drive_open(&handle);if(rc){error("open optical drive",rc);continue;}
  disc_id id;rc=identify(real_read,&handle,&id);
  if(rc){drive_close(handle);error("identify/read physical DVD",rc);continue;}
  msg[0]=0;cat(msg,sizeof msg,"PS2 cache: ");cat(msg,sizeof msg,id.serial);cat(msg,sizeof msg," - checking HDD images...");logmsg(msg);
  logmsg("trace: scan begin");
  rc=scan(ROOT,&id,0);int found_before=(rc>0);
  {char t[48]="trace: scan result ",h[9];hex(h,(u32)rc,8);cat(t,sizeof t,h);logmsg(t);}
  if(rc<0){drive_close(handle);report("PS2 cache: scan cancelled. Reinsert disc to retry.");continue;}
  if(!rc) {
   if(!confirm_copy(id.serial)){drive_close(handle);continue;}
   rc=still_disc(&handle,&id)?rip(&handle,&id):-94;
  }else{rc=0;logmsg("PS2 cache: matching ISO found on HDD, mounting quietly");}
  int copied=!found_before;
  if(!rc&&!still_disc(&handle,&id))rc=-94;
  drive_close(handle);
  if(rc)continue;
  if(cancelled())continue;
  rc=mount_image();
  if(rc){error("webMAN mount failed; ISO kept on HDD",rc);continue;}
  owned=1;
  if(copied)report("PS2 cache: matching ISO ready. Launch the disc icon in Game.");
  else logmsg("PS2 cache: matching ISO mounted (quiet).");
 }
done: running=0;{lv2syscall1(41,0);}
}
/* Imported from sysPrxForUser through module.S (proper PRX import, not a lookup). */

int plugin_start(size_t argc,void *argv) {
 (void)argc;(void)argv;logmsg("PS2 Disc Cache 21 start (GameOS VSH, no LV1 writes)");
 opd64 *o=(opd64*)worker;
 worker_entry_opd.func=(u32)(u64)o->func;
 worker_entry_opd.rtoc=(u32)(u64)o->rtoc;
 running=1;
 int rc=sys_thread_create_ex(&worker_id,&worker_entry_opd,0,1500,0x10000,1,"ps2_disc_cache");
 if(rc){running=0;error("worker create",rc);return 1;}
 logmsg("worker thread started");return 0;
}
int plugin_stop(size_t argc,void *argv) {
 (void)argc;(void)argv;running=0;if(worker_id){u64 result;sysThreadJoin(worker_id,&result);worker_id=0;}logmsg("PS2 Disc Cache stopped");return 0;
}
