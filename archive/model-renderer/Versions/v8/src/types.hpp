#ifndef TYPES_H
#define TYPES_H
#include "fixed.hpp"
#include "dout.hpp"
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

    T length() const
    {
        return sqrt(x * x + y * y);
    }
    Vector2 normalize()
    {
        T len = length();
        if (len == 0)
            return Vector2(0, 0);
        return Vector2(x / len, y / len);
    }

    template<typename U>
    Vector2(const Vector2<U>& other)
        : x(static_cast<T>(other.x)), 
          y(static_cast<T>(other.y)) {}

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
    Vector2 operator-() const
    {
        return Vector2(-x, -y);
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

    T length() const
    {
        return sqrt(x * x + y * y + z * z);
    }
    Vector3 normalize()
    {
        T len = length();
        if (len == 0)
            return Vector3(0, 0, 0);
        return Vector3(x / len, y / len, z / len);
    }
    Vector3 cross(const Vector3 &v) const
    {
        return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
    T dot(const Vector3 &v) const
    {
        return x * v.x + y * v.y + z * v.z;
    }

    template<typename U>
    Vector3(const Vector3<U>& other)
        : x(static_cast<T>(other.x)), 
          y(static_cast<T>(other.y)), 
          z(static_cast<T>(other.z)) {}

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
    Vector3 operator-() const
    {
        return Vector3(-x, -y, -z);
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

class Quaternion
{
private:
    fixed_t x, y, z, w;
    Vector3<fixed_t> euler;
    Vector3<fixed_t> cos;
    Vector3<fixed_t> sin;

    void UpdateTrigCache();
    Vector3<fixed_t> ToEulerAngles() const;
public:
    Quaternion();
    ~Quaternion();
    Quaternion(fixed_t x, fixed_t y, fixed_t z, fixed_t w);
    Quaternion(Vector3<fixed_t> eulerAngles);

    void Normalize();

    void Set(fixed_t x, fixed_t y, fixed_t z, fixed_t w);
    void Set(Vector3<fixed_t> eulerAngles);

    static Quaternion Identity();
    static Quaternion AngleAxis(fixed_t angleRad, Vector3<fixed_t> axis);

    Quaternion operator*(const Quaternion& q) const;
    Quaternion& operator*=(const Quaternion& q);

    bool operator==(const Quaternion& q) const;
    bool operator!=(const Quaternion& q) const;

    Vector3<fixed_t> Rotate(const Vector3<fixed_t>& vec) const;

    // Getters
    fixed_t X() const { return x; }
    fixed_t Y() const { return y; }
    fixed_t Z() const { return z; }
    fixed_t W() const { return w; }

    Vector3<fixed_t> GetCos() const { return cos; }
    Vector3<fixed_t> GetSin() const { return sin; }
    Vector3<fixed_t> GetEuler() const { return euler; }
};
    
inline casio::DoutStream& operator<<(casio::DoutStream& out, const Quaternion& q) {
    out << "(" << q.X() << ", " << q.Y() << ", " << q.Z() << ", " << q.W() << ")";
    return out;
}


#endif