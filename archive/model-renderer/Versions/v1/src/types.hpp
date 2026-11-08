#ifndef TYPES_H
#define TYPES_H
#include "fixed.hpp"
#include "custom_math.hpp"
#include <stdint.h>
#include <limits.h>

template <typename T>
class Vector2
{
public:
    T x, y;
    Vector2(T x, T y) : x(x), y(y) {}
    Vector2() : x(0), y(0) {}
    Vector2 operator+(const Vector2 &v) const
    {
        return Vector2(x + v.x, y + v.y);
    }
    Vector2 operator-(const Vector2 &v) const
    {
        return Vector2(x - v.x, y - v.y);
    }
    Vector2 operator*(const T &s) const
    {
        return Vector2(x * s, y * s);
    }
    Vector2 operator/(const T &s) const
    {
        return Vector2(x / s, y / s);
    }
    Vector2 &operator+=(const Vector2 &v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vector2 &operator-=(const Vector2 &v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    Vector2 &operator*=(const T &s)
    {
        x *= s;
        y *= s;
        return *this;
    }
    Vector2 &operator/=(const T &s)
    {
        x /= s;
        y /= s;
        return *this;
    }
    bool operator==(const Vector2 &v) const
    {
        return x == v.x && y == v.y;
    }
    bool operator!=(const Vector2 &v) const
    {
        return x != v.x || y != v.y;
    }
};

template <typename T>
class Vector3
{
public:
    T x, y, z;
    Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
    Vector3() : x(0), y(0), z(0) {}
    Vector3 operator+(const Vector3 &v) const
    {
        return Vector3(x + v.x, y + v.y, z + v.z);
    }
    Vector3 operator-(const Vector3 &v) const
    {
        return Vector3(x - v.x, y - v.y, z - v.z);
    }
    Vector3 operator*(const T &s) const
    {
        return Vector3(x * s, y * s, z * s);
    }
    Vector3 operator/(const T &s) const
    {
        return Vector3(x / s, y / s, z / s);
    }
    Vector3 &operator+=(const Vector3 &v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    Vector3 &operator-=(const Vector3 &v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    Vector3 &operator*=(const T &s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vector3 &operator/=(const T &s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
    bool operator==(const Vector3 &v) const
    {
        return x == v.x && y == v.y && z == v.z;
    }
    bool operator!=(const Vector3 &v) const
    {
        return x != v.x || y != v.y || z != v.z;
    }
};

#endif