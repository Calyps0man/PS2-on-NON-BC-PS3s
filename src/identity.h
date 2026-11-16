#ifndef DISCISO_IDENTITY_H
#define DISCISO_IDENTITY_H
#include <stdint.h>
#include <stddef.h>
typedef int (*sector_reader)(void *, uint32_t, uint32_t, void *);
typedef struct { uint32_t blocks; uint64_t fingerprint; char serial[16]; char label[33]; } disc_id;
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t be32(const unsigned char *p) { return (uint32_t)p[3]|(uint32_t)p[2]<<8|(uint32_t)p[1]<<16|(uint32_t)p[0]<<24; }
static uint64_t hash_bytes(uint64_t h,const unsigned char *p,size_t n) { while(n--) {h^=*p++; h*=UINT64_C(1099511628211);} return h; }
static int equal_bytes(const void *a,const void *b,size_t n) { const unsigned char *x=a,*y=b; while(n--) if(*x++!=*y++)return 0; return 1; }
/* Bounded ISO9660 parsing. Identification includes PVD, BOOT2, and distributed
 * sectors. This is an identity fingerprint, not a cryptographic proof of every byte. */
static int identify(sector_reader read,void *ctx,disc_id *id) {
 unsigned char b[2048],cnf[2049]; uint32_t root,bytes,extent=0,len=0;
 if(read(ctx,16,1,b)||b[0]!=1||!equal_bytes(b+1,"CD001",5)||b[6]!=1)return -1;
 id->blocks=le32(b+80);
 for(unsigned k=0;k<32;k++){id->label[k]=(char)b[40+k];}
 id->label[32]=0; /* PVD volume identifier */
 if(id->blocks<32||id->blocks>5000000||id->blocks!=be32(b+84)||b[128]!=0||b[129]!=8)return -2;
 id->fingerprint=hash_bytes(UINT64_C(14695981039346656037),b,2048);
 root=le32(b+158); bytes=le32(b+166);
 if(!root||bytes>1024*1024||root>=id->blocks||((uint64_t)root+(bytes+2047)/2048)>id->blocks)return -3;
 for(uint32_t s=0;s<(bytes+2047)/2048 && !extent;s++) {
  if(read(ctx,root+s,1,b))return -4;
  for(unsigned o=0;o<2048;) {
   unsigned n=b[o]; if(!n)break;
   if(n<34||o+n>2048||b[o+32]>n-33)return -5;
   if(b[o+32]==12 && equal_bytes(b+o+33,"SYSTEM.CNF;1",12)) {extent=le32(b+o+2);len=le32(b+o+10);break;}
   o+=n;
  }
 }
 if(!extent||extent>=id->blocks||!len||len>2048||read(ctx,extent,1,cnf))return -6;
 cnf[len]=0; id->serial[0]=0;
 for(unsigned i=0;i+5<len;i++) if(equal_bytes(cnf+i,"BOOT2",5)) {
  unsigned e=i;while(e<len&&cnf[e]!='\n'&&cnf[e]!='\r')e++;
  for(unsigned j=i+5;j+11<=e;j++) {
   int ok=1; for(unsigned k=0;k<4;k++)if(cnf[j+k]<'A'||cnf[j+k]>'Z')ok=0;
   if(cnf[j+4]!='_'||cnf[j+8]!='.')ok=0;
   for(unsigned k=5;k<11;k++)if(k!=8&&(cnf[j+k]<'0'||cnf[j+k]>'9'))ok=0;
   if(ok) {for(unsigned k=0;k<11;k++)id->serial[k]=(char)cnf[j+k];id->serial[11]=0;break;}
  } break;
 }
 if(!id->serial[0])return -7;
 id->fingerprint=hash_bytes(id->fingerprint,cnf,len);
 for(unsigned i=1;i<=8;i++) {uint32_t lba=(uint64_t)(id->blocks-1)*i/8; if(read(ctx,lba,1,b))return -8; id->fingerprint=hash_bytes(id->fingerprint,b,2048);}
 return 0;
}
static int same_disc(const disc_id *a,const disc_id *b) {return a->blocks==b->blocks && a->fingerprint==b->fingerprint && equal_bytes(a->serial,b->serial,12);}
#endif
