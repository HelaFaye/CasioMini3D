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

typedef struct fVector3
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
} fVector3;

typedef struct fVector2
{
    fixed_t x;
    fixed_t y;
} fVector2;

typedef struct Vertex
{
    Vector3<int> position;
    fVector3 projected;
} Vertex;

typedef struct RenderQuad
{
    fVector2 points[4];
    fixed_t z;
} RenderQuad;

class LineSlope
{
private:
    int x1, y1, x2, y2;
    int dx, dy;
    int stepx, stepy;
    int cumul;
    int i;
    fixed_t dratio;//pas de progression

    Vector2<int> cumulxy;
    Vector2<int> stepxy[2];
public:
    int tt_steps;
    fixed_t ratio;//pourcentage de progression (0-1 en fixe)
    int x, y;
    LineSlope(int x1, int y1, int x2, int y2)
    {
        this->x1 = x1;
        this->y1 = y1;
        this->x2 = x2;
        this->y2 = y2;

        this->x = x1;
        this->y = y1;

        dx = x2 - x1;
        dy = y2 - y1;

        stepx = dx < 0 ? -1 : 1;
        stepy = dy < 0 ? -1 : 1;
        dx = abs(dx);
        dy = abs(dy);
        i = 1;
        ratio = 0;
        dratio = 0;
        if (dx >= dy)
        {
            cumul = dx >> 1;
            if (dx != 0)
                dratio = FIXED_ONE / dx;
            tt_steps = dx;
            stepxy[0] = Vector2<int>(stepx, 0);
            stepxy[1] = Vector2<int>(0, stepy);
            cumulxy = Vector2<int>(dx, dy);
        }
        else
        {
            cumul = dy >> 1;
            if (dy != 0)
                dratio = FIXED_ONE / dy;
            tt_steps = dy;
            stepxy[0] = Vector2<int>(0, stepy);
            stepxy[1] = Vector2<int>(stepx, 0);
            cumulxy = Vector2<int>(dy, dx);
        }
    };

    static int tt_steps_for(int x1, int y1, int x2, int y2)
    {
        int dx = x2 - x1;
        int dy = y2 - y1;
        dx = abs(dx);
        dy = abs(dy);
        return dx >= dy ? dx : dy;
    };

    bool Next()
    {
        if(i <= tt_steps)
        {
            x += stepxy[0].x;
            y += stepxy[0].y;
            cumul += cumulxy.y;
            if(cumul >= cumulxy.x)
            {
                cumul -= cumulxy.x;
                x += stepxy[1].x;
                y += stepxy[1].y;
            }
            ratio += dratio;
            i++;
            return true;
        }
        ratio = FIXED_ONE;
        return false;
    };

    Vector2<int> Get()
    {
        return Vector2<int>(x, y);
    };
};


#endif