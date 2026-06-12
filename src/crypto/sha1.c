#include "../../globals.h"

#define SHA1_ROL(v,b) (((v)<<(b))|((v)>>(32-(b))))

typedef struct { uint32_t H[5]; uint8_t buf[64]; uint32_t lo,hi; } SHA1_CTX;

static void sha1_init(SHA1_CTX *c){
    c->H[0]=0x67452301;c->H[1]=0xEFCDAB89;c->H[2]=0x98BADCFE;
    c->H[3]=0x10325476;c->H[4]=0xC3D2E1F0;c->lo=c->hi=0;memset(c->buf,0,64);}

static void sha1_block(SHA1_CTX *c,const uint8_t *blk){
    uint32_t W[80],a,b,cc,d,e,f=0,k=0,t;int i;
    for(i=0;i<16;i++)W[i]=((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
                           ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
    for(i=16;i<80;i++)W[i]=SHA1_ROL(W[i-3]^W[i-8]^W[i-14]^W[i-16],1);
    a=c->H[0];b=c->H[1];cc=c->H[2];d=c->H[3];e=c->H[4];
    for(i=0;i<80;i++){
        if(i<20){f=(b&cc)|(~b&d);k=0x5A827999;}
        else if(i<40){f=b^cc^d;k=0x6ED9EBA1;}
        else if(i<60){f=(b&cc)|(b&d)|(cc&d);k=0x8F1BBCDC;}
        else{f=b^cc^d;k=0xCA62C1D6;}
        t=SHA1_ROL(a,5)+f+e+k+W[i];e=d;d=cc;cc=SHA1_ROL(b,30);b=a;a=t;}
    c->H[0]+=a;c->H[1]+=b;c->H[2]+=cc;c->H[3]+=d;c->H[4]+=e;}

static void sha1_update(SHA1_CTX *c,const uint8_t *data,size_t len){
    uint32_t idx=c->lo&63;c->lo+=(uint32_t)len;
    if(c->lo<(uint32_t)len) c->hi++;
    c->hi+=(uint32_t)(len>>29);
    while(len--){c->buf[idx++]=*data++;if(idx==64){sha1_block(c,c->buf);idx=0;}}}

static void sha1_final(SHA1_CTX *c,uint8_t out[20]){
    uint8_t pad[64];uint32_t i,bhi,blo,idx;
    bhi=(c->hi<<3)|(c->lo>>29);blo=c->lo<<3;idx=c->lo&63;
    memset(pad,0,64);pad[0]=0x80;
    sha1_update(c,pad,(idx<56)?(56-idx):(120-idx));
    pad[0]=(uint8_t)(bhi>>24);pad[1]=(uint8_t)(bhi>>16);
    pad[2]=(uint8_t)(bhi>>8);pad[3]=(uint8_t)bhi;
    pad[4]=(uint8_t)(blo>>24);pad[5]=(uint8_t)(blo>>16);
    pad[6]=(uint8_t)(blo>>8);pad[7]=(uint8_t)blo;
    sha1_update(c,pad,8);
    for(i=0;i<5;i++){out[i*4]=(uint8_t)(c->H[i]>>24);out[i*4+1]=(uint8_t)(c->H[i]>>16);
                      out[i*4+2]=(uint8_t)(c->H[i]>>8);out[i*4+3]=(uint8_t)c->H[i];}}

void sha1_hash(const uint8_t *data,size_t len,uint8_t out[20]){
    SHA1_CTX c;sha1_init(&c);sha1_update(&c,data,len);sha1_final(&c,out);}
