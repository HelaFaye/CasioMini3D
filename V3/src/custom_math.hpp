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

/* The parabolic approximation is defined over [-pi, pi]. */
inline float wrap_radians(float angle)
{
    angle = fmodf(angle, 2.0f * static_cast<float>(M_PI));
    if (angle > static_cast<float>(M_PI))
        angle -= 2.0f * static_cast<float>(M_PI);
    else if (angle < -static_cast<float>(M_PI))
        angle += 2.0f * static_cast<float>(M_PI);
    return angle;
}

inline fixed_t wrap_radians_fixed(fixed_t angle)
{
    const fixed_t pi = FLOAT_TO_FIXED(M_PI);
    const fixed_t two_pi = FLOAT_TO_FIXED(2.0 * M_PI);
    angle %= two_pi;
    if (angle > pi)
        angle -= two_pi;
    else if (angle < -pi)
        angle += two_pi;
    return angle;
}

inline float sin_approx(float angle)
{
    angle = wrap_radians(angle);
    const float B = 4.0f / static_cast<float>(M_PI);
    const float C = -4.0f / (static_cast<float>(M_PI) * static_cast<float>(M_PI));
    return B * angle + C * angle * fabsf(angle);
}

inline float cos_approx(float angle)
{
    return sin_approx(angle + static_cast<float>(M_PI_2));
}

/* Fixed-point parabolic sine approximation, with radians as input. */
inline fixed_t fsin_approx(fixed_t angle)
{
    angle = wrap_radians_fixed(angle);
    const fixed_t B = FLOAT_TO_FIXED(4.0 / M_PI);
    const fixed_t C = FLOAT_TO_FIXED(-4.0 / (M_PI * M_PI));
    return fmul(B, angle) + fmul(C, fmul(angle, fixed_abs(angle)));
}

/* Cosine is sine shifted by pi/2, not by the coefficient 4/pi. */
inline fixed_t fcos_approx(fixed_t angle)
{
    return fsin_approx(angle + FLOAT_TO_FIXED(M_PI_2));
}

#endif
