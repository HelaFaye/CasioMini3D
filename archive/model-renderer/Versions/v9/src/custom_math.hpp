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
#endif