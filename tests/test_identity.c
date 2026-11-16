#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/identity.h"
static unsigned char disc[256][2048];
static int fail_sector=-1;
static int reader(void *ctx,uint32_t sector,uint32_t count,void *buf) {
 (void)ctx;if(sector+count>256||(int)sector==fail_sector)return -1;
 memcpy(buf,disc[sector],count*2048);return 0;
}
static void put32(unsigned char *p,unsigned x) {for(unsigned i=0;i<4;i++)p[i]=x>>(8*i);}
static void both32(unsigned char *p,unsigned x) {put32(p,x);for(unsigned i=0;i<4;i++)p[i+4]=x>>(24-8*i);}
static void fixture(void) {
 memset(disc,0,sizeof disc);fail_sector=-1;
 unsigned char *b=disc[16];b[0]=1;memcpy(b+1,"CD001",5);b[6]=1;both32(b+80,256);b[129]=8;b[130]=8;
 both32(b+158,20);both32(b+166,4096);
 /* SYSTEM.CNF on second root directory sector exercises directory walking. */
 b=disc[21];b[0]=46;both32(b+2,22);both32(b+10,40);b[32]=12;memcpy(b+33,"SYSTEM.CNF;1",12);
 memcpy(disc[22],"BOOT2 = cdrom0:\\SLES_503.30;1\r\nVER = 1.00\r\n",40);
}
int main(void) {
 disc_id a,b;fixture();assert(!identify(reader,0,&a));assert(!strcmp(a.serial,"SLES_503.30"));
 assert(!identify(reader,0,&b)&&same_disc(&a,&b));
 disc[255][100]=1;assert(!identify(reader,0,&b)&&!same_disc(&a,&b));
 fixture();disc[22][21]='9';assert(!identify(reader,0,&b)&&!same_disc(&a,&b));
 fixture();disc[16][84]^=1;assert(identify(reader,0,&b)==-2);
 fixture();both32(disc[16]+166,0xffffffff);assert(identify(reader,0,&b)==-3);
 fixture();disc[21][0]=1;assert(identify(reader,0,&b)==-5);
 fixture();disc[21][32]=250;assert(identify(reader,0,&b)==-5);
 fixture();both32(disc[21]+10,4096);assert(identify(reader,0,&b)==-6);
 fixture();disc[22][4]='1';assert(identify(reader,0,&b)==-7);
 fixture();fail_sector=255;assert(identify(reader,0,&b)==-8);
 fixture();fail_sector=16;assert(identify(reader,0,&b)==-1);
 puts("PASS: 11 ISO identity/parser cases");return 0;
}
