/* Host profiling shim: the soft-float ABI entry points that
   `-m32 -msoft-float -mno-80387` code calls, implemented with hardware
   floats so results are bit-identical to IEEE single/double arithmetic.

   The fx-CG50's SH-4A has no FPU, so on the device every float operation is
   a libgcc call. Each shim call here also burns a fixed number of
   instructions approximating that cost, which makes callgrind's instruction
   counts track device cost. The weights are estimates; calibration against
   physical fx-CG50 runs is described in README.md.

   Compiled WITHOUT -msoft-float. Values cross the soft-float ABI as integer
   bit patterns, which is why every function takes and returns uint32_t or
   uint64_t. libm and libc functions that return floats are re-exported here
   because their glibc versions return in x87 registers. */
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>

unsigned long long softfloat_calls[8];
static void *libm(void){ static void *h; if(!h) h=dlopen("libm.so.6",RTLD_NOW|RTLD_LOCAL); return h; }
static inline void burn(unsigned n, int kind)
{
    softfloat_calls[kind]++;
    for (volatile unsigned i = 0; i < n / 4; ++i) {}
}
enum { K_ADD, K_MUL, K_DIV, K_CMP, K_CONV, K_LIBM, K_DBL };
static inline float F(uint32_t b){float f; memcpy(&f,&b,4); return f;}
static inline uint32_t B(float f){uint32_t b; memcpy(&b,&f,4); return b;}
static inline double D(uint64_t b){double f; memcpy(&f,&b,8); return f;}
static inline uint64_t BD(double f){uint64_t b; memcpy(&b,&f,8); return b;}

uint32_t __addsf3(uint32_t a, uint32_t b){burn(45,K_ADD); return B(F(a)+F(b));}
uint32_t __subsf3(uint32_t a, uint32_t b){burn(45,K_ADD); return B(F(a)-F(b));}
uint32_t __mulsf3(uint32_t a, uint32_t b){burn(40,K_MUL); return B(F(a)*F(b));}
uint32_t __divsf3(uint32_t a, uint32_t b){burn(90,K_DIV); return B(F(a)/F(b));}
uint32_t __negsf2(uint32_t a){return a ^ 0x80000000u;}
int __eqsf2(uint32_t a, uint32_t b){burn(15,K_CMP); return !(F(a)==F(b));}
int __nesf2(uint32_t a, uint32_t b){burn(15,K_CMP); return F(a)!=F(b);}
int __ltsf2(uint32_t a, uint32_t b){burn(15,K_CMP); float x=F(a),y=F(b); return x<y?-1:(x==y?0:1);}
int __lesf2(uint32_t a, uint32_t b){burn(15,K_CMP); float x=F(a),y=F(b); return x<y?-1:(x==y?0:1);}
int __gtsf2(uint32_t a, uint32_t b){burn(15,K_CMP); float x=F(a),y=F(b); return x>y?1:(x==y?0:-1);}
int __gesf2(uint32_t a, uint32_t b){burn(15,K_CMP); float x=F(a),y=F(b); return x>y?1:(x==y?0:-1);}
int __unordsf2(uint32_t a, uint32_t b){burn(15,K_CMP); return F(a)!=F(a)||F(b)!=F(b);}
int32_t __fixsfsi(uint32_t a){burn(20,K_CONV); return (int32_t)F(a);}
uint32_t __fixunssfsi(uint32_t a){burn(20,K_CONV); return (uint32_t)F(a);}
int64_t __fixsfdi(uint32_t a){burn(30,K_CONV); return (int64_t)F(a);}
uint32_t __floatsisf(int32_t a){burn(20,K_CONV); return B((float)a);}
uint32_t __floatunsisf(uint32_t a){burn(20,K_CONV); return B((float)a);}
uint32_t __floatdisf(int64_t a){burn(30,K_CONV); return B((float)a);}
uint32_t __floatundisf(uint64_t a){burn(30,K_CONV); return B((float)a);}
uint64_t __extendsfdf2(uint32_t a){burn(20,K_CONV); return BD((double)F(a));}
uint32_t __truncdfsf2(uint64_t a){burn(25,K_CONV); return B((float)D(a));}
uint64_t __adddf3(uint64_t a, uint64_t b){burn(110,K_DBL); return BD(D(a)+D(b));}
uint64_t __subdf3(uint64_t a, uint64_t b){burn(110,K_DBL); return BD(D(a)-D(b));}
uint64_t __muldf3(uint64_t a, uint64_t b){burn(120,K_DBL); return BD(D(a)*D(b));}
uint64_t __divdf3(uint64_t a, uint64_t b){burn(250,K_DBL); return BD(D(a)/D(b));}
uint64_t __negdf2(uint64_t a){return a ^ 0x8000000000000000ull;}
int __eqdf2(uint64_t a, uint64_t b){burn(25,K_CMP); return !(D(a)==D(b));}
int __nedf2(uint64_t a, uint64_t b){burn(25,K_CMP); return D(a)!=D(b);}
int __ltdf2(uint64_t a, uint64_t b){burn(25,K_CMP); double x=D(a),y=D(b); return x<y?-1:(x==y?0:1);}
int __ledf2(uint64_t a, uint64_t b){burn(25,K_CMP); double x=D(a),y=D(b); return x<y?-1:(x==y?0:1);}
int __gtdf2(uint64_t a, uint64_t b){burn(25,K_CMP); double x=D(a),y=D(b); return x>y?1:(x==y?0:-1);}
int __gedf2(uint64_t a, uint64_t b){burn(25,K_CMP); double x=D(a),y=D(b); return x>y?1:(x==y?0:-1);}
int __unorddf2(uint64_t a, uint64_t b){burn(25,K_CMP); return D(a)!=D(a)||D(b)!=D(b);}
int32_t __fixdfsi(uint64_t a){burn(30,K_CONV); return (int32_t)D(a);}
uint32_t __fixunsdfsi(uint64_t a){burn(30,K_CONV); return (uint32_t)D(a);}
uint64_t __floatsidf(int32_t a){burn(30,K_CONV); return BD((double)a);}
uint64_t __floatunsidf(uint32_t a){burn(30,K_CONV); return BD((double)a);}
uint64_t __floatundidf(uint64_t a){burn(40,K_CONV); return BD((double)a);}
uint64_t __floatdidf(int64_t a){burn(40,K_CONV); return BD((double)a);}

/* libm entry points used by fxOGL/fxGLMark, forwarded to the real libm. */
#define LIBM1F(name, cost) uint32_t name(uint32_t a){ static float (*fn)(float); \
    if(!fn) fn=(float(*)(float))dlsym(libm(),#name); burn(cost,K_LIBM); return B(fn(F(a))); }
#define LIBM2F(name, cost) uint32_t name(uint32_t a, uint32_t b){ static float (*fn)(float,float); \
    if(!fn) fn=(float(*)(float,float))dlsym(libm(),#name); burn(cost,K_LIBM); return B(fn(F(a),F(b))); }
#define LIBM1D(name, cost) uint64_t name(uint64_t a){ static double (*fn)(double); \
    if(!fn) fn=(double(*)(double))dlsym(libm(),#name); burn(cost,K_LIBM); return BD(fn(D(a))); }
LIBM1F(sqrtf,250) LIBM1F(floorf,40) LIBM1F(ceilf,40) LIBM1F(fabsf,5)
LIBM1F(tanf,1200) LIBM1F(log2f,800) LIBM1F(sinf,900) LIBM1F(cosf,900)
LIBM1F(logf,800) LIBM1F(expf,800) LIBM1F(exp2f,800) LIBM1F(atanf,1000)
LIBM1F(asinf,1200) LIBM1F(acosf,1200) LIBM2F(powf,1800) LIBM2F(atan2f,1200)
LIBM1D(floor,80) LIBM1D(sin,2000) LIBM1D(cos,2000) LIBM1D(sqrt,500)

static void *libc_handle(void){ static void *h; if(!h) h=dlopen("libc.so.6",RTLD_NOW); return h; }
/* libc functions returning float/double use the x87 return convention;
   re-export them with the soft-float (integer register) convention. */
uint32_t strtof(const char *s, char **end){ static float (*fn)(const char*,char**);
    if(!fn) fn=(float(*)(const char*,char**))dlsym(libc_handle(),"strtof"); return B(fn(s,end)); }
