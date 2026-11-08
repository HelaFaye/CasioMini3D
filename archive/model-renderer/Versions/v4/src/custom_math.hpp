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

inline float sin_approx(float x)
{
    const float PI     = M_PI;
    const float TWO_PI = 2.0f * M_PI;

    // Ramener x dans [-PI, PI]
    x = fmodf(x + PI, TWO_PI);
    if (x < 0) x += TWO_PI;
    x -= PI;

    // Approximation rapide (avec forme parabole affine)
    const float B = 4 / PI;
    const float C = -4 / (PI * PI);
    const float P = 0.225f; // amélioration

    float y = B * x + C * x * fabsf(x);
    return P * (y * fabsf(y) - y) + y;
}

inline float cos_approx(float x)
{
    return sin_approx(x + M_PI_2);
}

inline fixed_t fsin_approx(fixed_t x)
{
    const fixed_t PI     = FLOAT_TO_FIXED(M_PI);
    const fixed_t TWO_PI = FLOAT_TO_FIXED(2 * M_PI);

    // Ramener x dans [-PI, PI]
    x = x + PI;
    x = x % TWO_PI;
    if (x < 0) x += TWO_PI;
    x = x - PI;

    const fixed_t B = FLOAT_TO_FIXED(4 / M_PI);
    const fixed_t C = FLOAT_TO_FIXED(-4 / (M_PI * M_PI));
    const fixed_t P = FLOAT_TO_FIXED(0.225f); // amélioration

    fixed_t y = fmul(B, x) + fmul(C, fmul(x, abs(x)));
    return fmul(P, (fmul(y, abs(y)) - y)) + y;
}

inline fixed_t fcos_approx(fixed_t x)
{
    return fsin_approx(x + FLOAT_TO_FIXED(M_PI_2));
}

inline fixed_t fatan2_approx(fixed_t y, fixed_t x)
{
    if (x == 0) return (y > 0 ? FLOAT_TO_FIXED(M_PI_2) : (y < 0 ? -FLOAT_TO_FIXED(M_PI_2) : 0));

    fixed_t abs_y = abs(y);
    fixed_t angle;

    if (abs(x) > abs_y) {
        fixed_t r = fdiv(abs_y, abs(x));
        fixed_t r2 = fmul(r, r);
        angle = fmul(r, FLOAT_TO_FIXED(0.97239411f)) - fmul(r, fmul(r2, FLOAT_TO_FIXED(0.19194795f)));
    } else {
        fixed_t r = fdiv(abs(x), abs_y);
        fixed_t r2 = fmul(r, r);
        angle = FLOAT_TO_FIXED(M_PI_2) - (fmul(r, FLOAT_TO_FIXED(0.97239411f)) - fmul(r, fmul(r2, FLOAT_TO_FIXED(0.19194795f))));
    }

    // Quadrants
    if (x < 0) {
        if (y < 0)
            angle -= FLOAT_TO_FIXED(M_PI);
        else
            angle = FLOAT_TO_FIXED(M_PI) - angle;
    } else {
        if (y < 0)
            angle = -angle;
    }

    return angle;
}


inline fixed_t fasin_approx(fixed_t x)
{
    // Clamp
    if (x < -FLOAT_TO_FIXED(1.0f)) x = -FLOAT_TO_FIXED(1.0f);
    if (x >  FLOAT_TO_FIXED(1.0f)) x =  FLOAT_TO_FIXED(1.0f);

    // Approximation : arcsin(x) ≈ x + a·x³ + b·x⁵
    const fixed_t a = FLOAT_TO_FIXED(0.165f);
    const fixed_t b = FLOAT_TO_FIXED(0.007f);

    fixed_t x3 = fmul(x, fmul(x, x));
    fixed_t x5 = fmul(x3, fmul(x, x));

    return x + fmul(a, x3) + fmul(b, x5);
}


inline fixed_t fcopysign(fixed_t a, fixed_t b) {
    return (b < 0) ? -abs(a) : abs(a);
}

#endif