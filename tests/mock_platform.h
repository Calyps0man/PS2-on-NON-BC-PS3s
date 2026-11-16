#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;typedef int32_t s32;typedef int64_t s64;
typedef struct {void *func,*rtoc;u64 zero;} opd64;typedef struct {u32 func,rtoc;} opd32;
typedef struct stat sysFSStat;
typedef struct {u8 d_type,d_namlen;char d_name[1025];} sysFSDirent;
#define SYS_O_RDONLY 0
#define SYS_O_WRONLY 1
#define SYS_O_CREAT 0100
#define SYS_O_EXCL 0200
#define SYS_O_APPEND 02000
s64 mock_call(u64,u64,u64,u64,u64,u64,u64,u64);
#define lv2syscall1(n,a) s64 p1 __attribute__((unused))=mock_call(n,(u64)(a),0,0,0,0,0,0)
#define lv2syscall2(n,a,b) s64 p1 __attribute__((unused))=mock_call(n,(u64)(a),(u64)(b),0,0,0,0,0)
#define lv2syscall3(n,a,b,c) s64 p1 __attribute__((unused))=mock_call(n,(u64)(a),(u64)(b),(u64)(c),0,0,0,0)
#define lv2syscall4(n,a,b,c,d) s64 p1 __attribute__((unused))=mock_call(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d),0,0,0)
#define lv2syscall7(n,a,b,c,d,e,f,g) s64 p1=mock_call(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d),(u64)(e),(u64)(f),(u64)(g))
int sysLv2FsOpen(const char*,int,s32*,u32,const void*,u64);
int sysLv2FsRead(s32,void*,u64,u64*);
int sysLv2FsWrite(s32,const void*,u64,u64*);
int sysLv2FsClose(s32);
int sysLv2FsStat(const char*,sysFSStat*);
int sysLv2FsOpenDir(const char*,s32*);
int sysLv2FsReadDir(s32,sysFSDirent*,u64*);
int sysLv2FsCloseDir(s32);
int sysLv2FsLSeek64(s32,u64,s32,u64*);
int sysLv2FsMkdir(const char*,s32);
int sysLv2FsUnlink(const char*);
int sysLv2FsFsync(s32);
int sysLv2FsRename(const char*,const char*);
int sysThreadJoin(u64,u64*);
static inline int sys_thread_create_ex(u64*a,opd32*b,void*c,s32 d,u64 e,u64 f,const char*g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;return 0;}
static inline int imp_vshtask_notify(int a,const char*b){(void)a;(void)b;return 0;}
static inline int imp_vshmain_mode(void){return 0;}
static inline int imp_fs_free_size(const char*a,u32*b,u64*c){(void)a;(void)b;(void)c;return 0;}
static inline int imp_socket(int a,int b,int c){(void)a;(void)b;(void)c;return -1;}
static inline int imp_connect(int a,const void*b,u32 c){(void)a;(void)b;(void)c;return -1;}
static inline int imp_send(int a,const void*b,u32 c,int d){(void)a;(void)b;(void)c;(void)d;return -1;}
static inline int imp_socketclose(int a){(void)a;return 0;}
int imp_pad_get_data(u32 port,void *data);
#endif
