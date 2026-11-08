#ifndef FIXED_H
#define FIXED_H
#include "DoutStream.hpp"
#include <cstdint>
#include <cmath>
#include <type_traits>

constexpr int PRECISION = 16;
constexpr int64_t FIXED_ONE = 1LL << PRECISION;
constexpr double M_PI = 3.14159265358979323846;
constexpr double M_PI_2 = 1.57079632679489661923;

class fixed_t {
    int64_t value;

public:
    // Constructors
    constexpr fixed_t() : value(0) {}
    constexpr fixed_t(const fixed_t& other) = default;
    constexpr fixed_t(int v) : value(static_cast<int64_t>(v) << PRECISION) {}
    constexpr fixed_t(float v) : value(static_cast<int64_t>(v * FIXED_ONE)) {}
    constexpr fixed_t(double v) : value(static_cast<int64_t>(v * FIXED_ONE)) {}
    constexpr fixed_t(int64_t raw, bool) : value(raw) {} // raw constructor

    // Implicit conversions from int, float, double
    constexpr fixed_t& operator=(int v) { value = static_cast<int64_t>(v) << PRECISION; return *this; }
    constexpr fixed_t& operator=(float v) { value = static_cast<int64_t>(v * FIXED_ONE); return *this; }
    constexpr fixed_t& operator=(double v) { value = static_cast<int64_t>(v * FIXED_ONE); return *this; }

    // Conversion operators
    constexpr operator int() const { return static_cast<int>(value >> PRECISION); }
    constexpr operator float() const { return static_cast<float>(value) / FIXED_ONE; }
    constexpr operator double() const { return static_cast<double>(value) / FIXED_ONE; }

    // Access to raw value
    constexpr int64_t raw() const { return value; }

    // Arithmetic operators with fixed_t
    constexpr fixed_t operator+(const fixed_t& other) const { return fixed_t(value + other.value, true); }
    constexpr fixed_t operator-(const fixed_t& other) const { return fixed_t(value - other.value, true); }
    constexpr fixed_t operator*(const fixed_t& other) const { return fixed_t((value * other.value) >> PRECISION, true); }
    constexpr fixed_t operator/(const fixed_t& other) const { return fixed_t((value << PRECISION) / other.value, true); }
    constexpr fixed_t operator-() const { return fixed_t(-value, true); }

    // Compound assignment
    fixed_t& operator+=(const fixed_t& other) { value += other.value; return *this; }
    fixed_t& operator-=(const fixed_t& other) { value -= other.value; return *this; }
    fixed_t& operator*=(const fixed_t& other) { value = (value * other.value) >> PRECISION; return *this; }
    fixed_t& operator/=(const fixed_t& other) { value = (value << PRECISION) / other.value; return *this; }

    // Arithmetic operators with int
    constexpr fixed_t operator+(int other) const { return *this + fixed_t(other); }
    constexpr fixed_t operator-(int other) const { return *this - fixed_t(other); }
    constexpr fixed_t operator*(int other) const { return *this * fixed_t(other); }
    constexpr fixed_t operator/(int other) const { return *this / fixed_t(other); }

    fixed_t& operator+=(int other) { *this = *this + fixed_t(other); return *this; }
    fixed_t& operator-=(int other) { *this = *this - fixed_t(other); return *this; }
    fixed_t& operator*=(int other) { *this = *this * fixed_t(other); return *this; }
    fixed_t& operator/=(int other) { *this = *this / fixed_t(other); return *this; }

    // Arithmetic operators with float
    constexpr fixed_t operator+(float other) const { return *this + fixed_t(other); }
    constexpr fixed_t operator-(float other) const { return *this - fixed_t(other); }
    constexpr fixed_t operator*(float other) const { return *this * fixed_t(other); }
    constexpr fixed_t operator/(float other) const { return *this / fixed_t(other); }

    fixed_t& operator+=(float other) { *this = *this + fixed_t(other); return *this; }
    fixed_t& operator-=(float other) { *this = *this - fixed_t(other); return *this; }
    fixed_t& operator*=(float other) { *this = *this * fixed_t(other); return *this; }
    fixed_t& operator/=(float other) { *this = *this / fixed_t(other); return *this; }

    // Comparison operators with fixed_t
    constexpr bool operator==(const fixed_t& other) const { return value == other.value; }
    constexpr bool operator!=(const fixed_t& other) const { return value != other.value; }
    constexpr bool operator<(const fixed_t& other) const { return value < other.value; }
    constexpr bool operator>(const fixed_t& other) const { return value > other.value; }
    constexpr bool operator<=(const fixed_t& other) const { return value <= other.value; }
    constexpr bool operator>=(const fixed_t& other) const { return value >= other.value; }

    // Comparison operators with int
    constexpr bool operator==(int other) const { return *this == fixed_t(other); }
    constexpr bool operator!=(int other) const { return *this != fixed_t(other); }
    constexpr bool operator<(int other) const { return *this < fixed_t(other); }
    constexpr bool operator>(int other) const { return *this > fixed_t(other); }
    constexpr bool operator<=(int other) const { return *this <= fixed_t(other); }
    constexpr bool operator>=(int other) const { return *this >= fixed_t(other); }

    // Comparison operators with float
    constexpr bool operator==(float other) const { return *this == fixed_t(other); }
    constexpr bool operator!=(float other) const { return *this != fixed_t(other); }
    constexpr bool operator<(float other) const { return *this < fixed_t(other); }
    constexpr bool operator>(float other) const { return *this > fixed_t(other); }
    constexpr bool operator<=(float other) const { return *this <= fixed_t(other); }
    constexpr bool operator>=(float other) const { return *this >= fixed_t(other); }

    // Utilities
    static constexpr fixed_t from_raw(int64_t raw_val) { return fixed_t(raw_val, true); }
    static constexpr fixed_t one() { return fixed_t(FIXED_ONE, true); }
    static constexpr fixed_t zero() { return fixed_t(0, true); }

    constexpr int floor() const { return static_cast<int>(value >> PRECISION); }
    constexpr int ceil() const { return static_cast<int>((value + (FIXED_ONE - 1)) >> PRECISION); }
    constexpr int round() const { return static_cast<int>((value + (FIXED_ONE >> 1)) >> PRECISION); }

    constexpr fixed_t abs() const { return value < 0 ? fixed_t(-value, true) : *this; }

    static constexpr fixed_t lerp(fixed_t a, fixed_t b, fixed_t t) {
        return a + t * (b - a);
    }

    static inline fixed_t sqrt(fixed_t x) {
        return fixed_t(std::sqrt(static_cast<float>(x)), false);
    }

    static inline fixed_t inv(fixed_t x) {
        return fixed_t((FIXED_ONE << PRECISION) / x.value, true);
    }

    static constexpr fixed_t ease_in(fixed_t x) {
        return x * x;
    }

    static constexpr fixed_t ease(fixed_t x) {
        return (x <= fixed_t(0.5f)) ? fixed_t(2) * x * x : fixed_t(1) - fixed_t(2) * (fixed_t(1) - x) * (fixed_t(1) - x);
    }

    static constexpr void swap(fixed_t& a, fixed_t& b) {
        fixed_t temp = a;
        a = b;
        b = temp;
    }

    static inline fixed_t sin(fixed_t x) {
        const fixed_t PI     = fixed_t(M_PI);
        const fixed_t TWO_PI = fixed_t(2 * M_PI);
        x = x + PI;
        x = fixed_t(x.raw() % TWO_PI.raw(), true);
        if (x < fixed_t::zero()) x = x + TWO_PI;
        x = x - PI;

        const fixed_t B = fixed_t(4.0 / M_PI);
        const fixed_t C = fixed_t(-4.0 / (M_PI * M_PI));
        const fixed_t P = fixed_t(0.225f);

        fixed_t y = B * x + C * x * x.abs();
        return P * (y * y.abs() - y) + y;
    }

    static inline fixed_t cos(fixed_t x) {
        return sin(x + fixed_t(M_PI_2));
    }

    static inline fixed_t atan2(fixed_t y, fixed_t x) {
        if (x == fixed_t::zero()) return (y > fixed_t::zero() ? fixed_t(M_PI_2) : (y < fixed_t::zero() ? fixed_t(-M_PI_2) : fixed_t::zero()));

        fixed_t abs_y = y.abs();
        fixed_t angle;

        if (x.abs() > abs_y) {
            fixed_t r = abs_y / x.abs();
            fixed_t r2 = r * r;
            angle = r * fixed_t(0.97239411f) - r * r2 * fixed_t(0.19194795f);
        } else {
            fixed_t r = x.abs() / abs_y;
            fixed_t r2 = r * r;
            angle = fixed_t(M_PI_2) - (r * fixed_t(0.97239411f) - r * r2 * fixed_t(0.19194795f));
        }

        if (x < fixed_t::zero()) {
            angle = (y < fixed_t::zero()) ? angle - fixed_t(M_PI) : fixed_t(M_PI) - angle;
        } else {
            if (y < fixed_t::zero()) angle = -angle;
        }

        return angle;
    }

    static inline fixed_t asin(fixed_t x) {
        if (x < fixed_t(-1.0f)) x = fixed_t(-1.0f);
        if (x > fixed_t(1.0f)) x = fixed_t(1.0f);

        const fixed_t a = fixed_t(0.165f);
        const fixed_t b = fixed_t(0.007f);
        fixed_t x3 = x * x * x;
        fixed_t x5 = x3 * x * x;

        return x + a * x3 + b * x5;
    }

    static constexpr fixed_t copysign(fixed_t a, fixed_t b) {
        return (b < fixed_t::zero()) ? -a.abs() : a.abs();
    }
};

inline casio::DoutStream& operator<<(casio::DoutStream& out, const fixed_t& f) {
    out << static_cast<float>(f);
    return out;
}

//surcharge d'opérateurs sqrt, sin, cos, atan2, asin
inline fixed_t sqrt(fixed_t x) { return fixed_t::sqrt(x); }
inline fixed_t sin(fixed_t x) { return fixed_t::sin(x); }
inline fixed_t cos(fixed_t x) { return fixed_t::cos(x); }
inline fixed_t atan2(fixed_t y, fixed_t x) { return fixed_t::atan2(y, x); }
inline fixed_t asin(fixed_t x) { return fixed_t::asin(x); }

#endif