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
    bool operator==(const Vector2 &v) const
    {
        return x == v.x && y == v.y;
    }
    bool operator!=(const Vector2 &v) const
    {
        return x != v.x || y != v.y;
    }
};

inline Vector2<fixed_t> fmul(const Vector2<fixed_t>& v, fixed_t s)
{
    return Vector2<fixed_t>{
        fmul(v.x, s),
        fmul(v.y, s)
    };
}

inline Vector2<fixed_t> fmul(const Vector2<fixed_t>& v, float f)
{
    return fmul(v, FLOAT_TO_FIXED(f));
}

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
    bool operator==(const Vector3 &v) const
    {
        return x == v.x && y == v.y && z == v.z;
    }
    bool operator!=(const Vector3 &v) const
    {
        return x != v.x || y != v.y || z != v.z;
    }
};

template<typename T, typename U>
Vector3<U> vector3_cast(const Vector3<T>& v);

// Implémentations
template<>
inline Vector3<fixed_t> vector3_cast<int, fixed_t>(const Vector3<int>& v) {
    return Vector3<fixed_t>{INT_TO_FIXED(v.x), INT_TO_FIXED(v.y), INT_TO_FIXED(v.z)};
}

template<>
inline Vector3<int> vector3_cast<fixed_t, int>(const Vector3<fixed_t>& v) {
    return Vector3<int>{TO_INT(v.x), TO_INT(v.y), TO_INT(v.z)};
}

template<>
inline Vector3<fixed_t> vector3_cast<float, fixed_t>(const Vector3<float>& v) {
    return Vector3<fixed_t>{FLOAT_TO_FIXED(v.x), FLOAT_TO_FIXED(v.y), FLOAT_TO_FIXED(v.z)};
}

template<>
inline Vector3<float> vector3_cast<fixed_t, float>(const Vector3<fixed_t>& v) {
    return Vector3<float>{TO_FLOAT(v.x), TO_FLOAT(v.y), TO_FLOAT(v.z)};
}

template<>
inline Vector3<int> vector3_cast<float, int>(const Vector3<float>& v) {
    return Vector3<int>{(int)v.x, (int)v.y, (int)v.z};
}

template<>
inline Vector3<float> vector3_cast<int, float>(const Vector3<int>& v) {
    return Vector3<float>{(float)v.x, (float)v.y, (float)v.z};
}



inline Vector3<fixed_t> fmul(const Vector3<fixed_t>& v, fixed_t s)
{
    return Vector3<fixed_t>{
        fmul(v.x, s),
        fmul(v.y, s),
        fmul(v.z, s)
    };
}

inline Vector3<fixed_t> fmul(const Vector3<fixed_t>& v, float f)
{
    return fmul(v, FLOAT_TO_FIXED(f));
}

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
    Quaternion(float x, float y, float z, float w);
    Quaternion(Vector3<fixed_t> eulerAngles);
    Quaternion(Vector3<float> eulerAngles);

    void Normalize();

    void Set(fixed_t x, fixed_t y, fixed_t z, fixed_t w);
    void Set(float x, float y, float z, float w);
    void Set(Vector3<fixed_t> eulerAngles);
    void Set(Vector3<float> eulerAngles);

    static Quaternion Identity();
    static Quaternion AngleAxis(float angleRad, Vector3<float> axis);
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
    
    


#endif