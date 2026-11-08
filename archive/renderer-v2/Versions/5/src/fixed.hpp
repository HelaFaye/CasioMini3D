#ifndef FIXED_H
#define FIXED_H
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

/* The fixed point type (to make the code easier to read). */
typedef intmax_t fixed_t;

/* The precision of the fixed point numbers. */
#define PRECISION 16

#define FIXED_ONE (1 << PRECISION)

/* Convert an integer to a fixed point number. */
#define INT_TO_FIXED(num) (fixed_t)((num)<<PRECISION)
/* Convert a float to a fixed point number. */
#define FLOAT_TO_FIXED(num) (fixed_t)((num)*(1<<PRECISION))
/* Convert a fixed point number to an integer. */
#define TO_INT(num) (int)((num)>>PRECISION)
/* Convert a fixed point number to a float. */
#define TO_FLOAT(num) ((float)(num)/(1<<PRECISION))
/* Multiply two fixed point numbers together. */
#define MUL(a, b) ((a*b)>>PRECISION)

#define DIV(a, b) ((a<<PRECISION)/b)



static inline fixed_t fixed_abs(fixed_t x)
{
    return x < 0 ? -x : x;
}

static inline fixed_t fmul(fixed_t left, fixed_t right)
{
    /* Generally optimized by the compiler to use dmuls.l and xtrct */
    intmax_t p = left * right;
    return (p >> PRECISION);
}

static inline fixed_t fdiv(fixed_t left, fixed_t right)
{
    /* Pretty slow */
    intmax_t d = left << PRECISION;
    return d / right;
}

#define fix(x) ((int)((x) * FIXED_ONE))

static inline fixed_t fixdouble(double constant)
{
    return (fixed_t)(constant * FIXED_ONE);
}

static inline fixed_t fixfloat(float constant)
{
    return (fixed_t)(constant * FIXED_ONE);
}

static inline fixed_t fdec(fixed_t f)
{
    return f & 0xffff;
}

static inline int ffloor(fixed_t f)
{
    return f >> PRECISION;
}

static inline int fceil(fixed_t f)
{
    return (f + 0xffff) >> PRECISION;
}

static inline int fround(fixed_t f)
{
    return (f + 0x8000) >> PRECISION;
}

static inline float f2float(fixed_t f)
{
    return (float)f / FIXED_ONE;
}

static inline double f2double(fixed_t f)
{
    return (double)f / FIXED_ONE;
}

static inline double f2int(fixed_t f)
{
    return (int)f / FIXED_ONE;
}

static inline fixed_t feasein(fixed_t x)
{
    return fmul(x, x);
}

static inline fixed_t fease(fixed_t x)
{
    if(x <= fix(0.5)) {
        return 2 * fmul(x, x);
    }
    else {
        x = fix(1) - x;
        return fix(1) - 2 * fmul(x, x);
    }
}

static inline void fswap(fixed_t *x, fixed_t *y)
{
    fixed_t temp = *x;
    *x = *y;
    *y = temp;
}

static inline fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t)
{
    return a + fmul(t, b - a);
}

#endif
