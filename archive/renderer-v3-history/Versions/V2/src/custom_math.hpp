#ifndef CUSTOM_MATH_H
#define CUSTOM_MATH_H
#include <math.h>
#include "fixed.hpp"

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923

#define DEG_TO_RAD(x) ((x) * M_PI / 180.0)
#define RAD_TO_DEG(x) ((x) * 180.0 / M_PI)
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

inline float sin_approx(float i)
{
    const float B = 4 / M_PI;
    const float C = -4 / (M_PI * M_PI);

    return B * i + C * i * fabs(i);
}

inline float cos_approx(float i)
{
    return sin_approx(i + M_PI_2);
}

/* Approximation de sin en virgule fixe */
inline fixed_t fsin_approx(fixed_t i)
{
    const fixed_t B = FLOAT_TO_FIXED(4 / M_PI);
    const fixed_t C = FLOAT_TO_FIXED(-4 / (M_PI * M_PI));

    return fmul(B, i) + fmul(C, fmul(i, abs(i)));
}

/* Approximation de cos en virgule fixe */
inline fixed_t fcos_approx(fixed_t i)
{
    const fixed_t p = FLOAT_TO_FIXED(4 / M_PI);
    return fsin_approx(i + p);
}

#endif