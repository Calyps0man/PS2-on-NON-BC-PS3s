#define main identity_test_main
#include "test_identity.c"
#undef main
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#define HOST_TEST
#include "../src/plugin.c"
static char sandbox[128];static int part_fd=-1,short_write,corrupt,fail_sync,cancel_reads,no_space,read_calls;
static int scripted_answer=-1;
static int big_alloc,allocs,frees;static u64 max_read;
static DIR *dirs[16];
static const char *map_path(const char *p,char *out) {snprintf(out,1200,"%s%s",sandbox,p+9);return out;}
int sysLv2FsOpen(const char *p,int flags,s32 *fd,u32 mode,const void *arg,u64 args) {
 (void)arg;(void)args;char b[1200];*fd=open(map_path(p,b),flags,mode);
 if(*fd>=0&&strstr(p,".part"))part_fd=*fd;return *fd<0?-errno:0;
}
int sysLv2FsClose(s32 fd){if(fd==part_fd)part_fd=-1;return close(fd);}
int sysLv2FsRead(s32 fd,void *p,u64 n,u64 *got){ssize_t r=read(fd,p,n);if(r<0)return -errno;*got=r;if(corrupt&&fd==part_fd&&r)((unsigned char*)p)[0]^=1;return 0;}
int sysLv2FsWrite(s32 fd,const void *p,u64 n,u64 *got){if(short_write&&fd==part_fd)n/=2;ssize_t r=write(fd,p,n);if(r<0)return -errno;*got=r;return 0;}
int sysLv2FsStat(const char *p,sysFSStat *s){char b[1200];return stat(map_path(p,b),s);}
int sysLv2FsOpenDir(const char *p,s32 *fd){char b[1200];for(int i=0;i<16;i++)if(!dirs[i]){dirs[i]=opendir(map_path(p,b));*fd=i;return dirs[i]?0:-1;}return -1;}
int sysLv2FsReadDir(s32 fd,sysFSDirent *ent,u64 *n){struct dirent *e=readdir(dirs[fd]);*n=!!e;if(e){strcpy(ent->d_name,e->d_name);ent->d_type=e->d_type;}return 0;}
int sysLv2FsCloseDir(s32 fd){int r=closedir(dirs[fd]);dirs[fd]=0;return r;}
int sysLv2FsLSeek64(s32 fd,u64 off,s32 dir,u64 *pos){off_t r=lseek(fd,off,dir);*pos=r;return r<0?-errno:0;}
int sysLv2FsMkdir(const char *p,s32 mode){char b[1200];return mkdir(map_path(p,b),mode);}
int sysLv2FsUnlink(const char *p){char b[1200];return unlink(map_path(p,b));}
int sysLv2FsFsync(s32 fd){return fail_sync&&fd==part_fd?-1:fsync(fd);}
int sysLv2FsRename(const char *p,const char *q){char b[1200],c[1200];return rename(map_path(p,b),map_path(q,c));}
int sysThreadJoin(u64 t,u64 *r){(void)t;(void)r;return 0;}
s64 mock_call(u64 sc,u64 a,u64 b,u64 c,u64 d,u64 e,u64 f,u64 g) {
 (void)a;(void)b;(void)g;
 if(sc==8){assert(a==0x7020);*(u32*)b=0xff61;*(u32*)c=0xff61;*(u32*)d=0;return 0;}
 if(sc==348){if(!big_alloc)return -1;void *m=mmap(0,a,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);if(m==MAP_FAILED)return -1;*(u32*)c=(u32)(u64)m;allocs++;return 0;}
 if(sc==349){frees++;return 0;}
 if(sc==602&&max_read&&d>(u64)max_read){read_calls++;return -5;}
 if(sc==602){read_calls++;if(cancel_reads&&read_calls>=cancel_reads)running=0;int rc=reader(0,c,d,(void*)e);*(u32*)f=rc?0:d;return rc;}
 return 0;
}
static int test_mode(void){return 0;}
static int r3_after=-1,pad_calls;
int imp_pad_get_data(u32 port,void *data){s32 *len=data;u16 *b=(u16*)((char*)data+4);(void)port;pad_calls++;
 if(r3_after>=0&&pad_calls>=r3_after){*len=8;b[2]=0x0004;}return 0;}
static int test_space(const char *p,u32 *bs,u64 *free){(void)p;*bs=4096;*free=no_space?0:1000000;return 0;}
static void reset(void){fixture();running=1;part_fd=-1;short_write=corrupt=fail_sync=cancel_reads=no_space=read_calls=0;mode_fn=test_mode;pad_fn=imp_pad_get_data;pad_calls=0;r3_after=-1;space_fn=test_space;notify_fn=0;}
static int images(void) {char p[1200];map_path(ROOT,p);DIR *d=opendir(p);if(!d)return 0;int n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')n++;closedir(d);return n;}
int main(void) {
 strcpy(sandbox,"/tmp/ps2cache-test-XXXXXX");assert(mkdtemp(sandbox));char p[1200];snprintf(p,sizeof p,"%s/tmp",sandbox);assert(!mkdir(p,0700));
 disc_id id;u32 h=1;reset();assert(!identify(reader,0,&id));
 r3_after=-1;assert(!confirm_copy(id.serial));                 /* no R3 within 30 s: skipped */
 pad_calls=0;r3_after=50;assert(confirm_copy(id.serial));     /* R3 pressed: copy */
 assert(!rip(&h,&id));assert(images()==1);assert(scan(ROOT,&id,0)==1);sysFSStat st;assert(!sysLv2FsStat(selected,&st)&&st.st_size==sizeof disc);
 {char first[1024];memcpy(first,selected,sizeof first);
  assert(!rip(&h,&id));assert(images()==2);assert(strcmp(first,selected));   /* name taken: alternate name, nothing overwritten */
  assert(!sysLv2FsStat(first,&st)&&st.st_size==sizeof disc);
  char second[1024];memcpy(second,selected,sizeof second);
  assert(rip(&h,&id)==-2);assert(images()==2);                              /* both names taken: refused */
  assert(!sysLv2FsUnlink(first));assert(!sysLv2FsUnlink(second));}
 /* readable names */
 {char o[40];
  nice_label("JAK_AND_DAXTER                  ","SLES_526.70",o,sizeof o);assert(!strcmp(o,"JAK AND DAXTER"));
  nice_label("SLES_52670                      ","SLES_526.70",o,sizeof o);assert(!o[0]);
  nice_label("SLES_526.70","SLES_526.70",o,sizeof o);assert(!o[0]);
  nice_label("CDROM                           ","SLES_526.70",o,sizeof o);assert(!o[0]);
  nice_label("                                ","SLES_526.70",o,sizeof o);assert(!o[0]);
  nice_label("GT4/ \\:*?\"<>|                    ","SCES_510.53",o,sizeof o);assert(!strcmp(o,"GT4"));
  nice_label("Final Fantasy X","SLUS_203.12",o,sizeof o);assert(!strcmp(o,"Final Fantasy X"));}
 /* identity reads the PVD label */
 {disc_id lab;memcpy(disc[16]+40,"MY_GAME_TITLE                   ",32);
  assert(!identify(reader,0,&lab));assert(!memcmp(lab.label,"MY_GAME_TITLE",13));
  memset(disc[16]+40,0,32);}
 reset();short_write=1;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();fail_sector=0;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();fail_sync=1;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();corrupt=1;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();cancel_reads=1;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();no_space=1;assert(rip(&h,&id)!=0);assert(images()==0);
 reset();assert(!rip(&h,&id));disc[255][0]=9;disc_id other;assert(!identify(reader,0,&other));assert(scan(ROOT,&other,0)==0);
 /* 512 KB runtime buffer; drive rejects reads above 64 sectors -> chunk halves, copy still exact */
 sysLv2FsUnlink(selected);reset();big_alloc=1;max_read=64;allocs=frees=0;{int rr=rip(&h,&id);if(rr)printf("rip rc=%d\n",rr);assert(!rr);}assert(images()==1);
 assert(allocs==1&&frees==1&&xfer==transfer&&xfer_max==READ_CHUNK_SECTORS);
 {disc_id again;s32 f2;assert(!sysLv2FsOpen(selected,SYS_O_RDONLY,&f2,0,0,0));assert(!identify(file_read,&f2,&again));sysLv2FsClose(f2);assert(same_disc(&id,&again));}
 assert(!sysLv2FsUnlink(selected));big_alloc=0;max_read=0;
 puts("PASS: actual copy/scan code: success, no overwrite, short write, drive failure, fsync failure, corrupt readback, cancellation, disk full, mismatch, 512 KB buffer with adaptive chunk size");
 return 0;
}
