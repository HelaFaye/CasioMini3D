#include "shapes.hpp"

struct Edge { int x, y; fixed_t u, v; };

inline Edge CalcEdge(VertexLocal a, VertexLocal b, int y)
{
    const fixed_t deltaY = static_cast<fixed_t>(b.y) - a.y;
    if (deltaY == 0)
        return {a.x, y, a.u, a.v};

    // Promote before subtraction/multiplication: screen coordinates are ints,
    // while the interpolation factor is 16.16 fixed point.
    const fixed_t factor =
        ((static_cast<fixed_t>(y) - a.y) * FIXED_ONE) / deltaY;
    const int x = a.x + static_cast<int>(
        ((static_cast<fixed_t>(b.x) - a.x) * factor) >> PRECISION);
    const fixed_t u = a.u + (((b.u - a.u) * factor) >> PRECISION);
    const fixed_t v = a.v + (((b.v - a.v) * factor) >> PRECISION);
    return {x, y, u, v};
}

inline void ProcessScanline(
    int y, int Ax, int Bx,
    fixed_t Au, fixed_t Av, fixed_t Bu, fixed_t Bv)
{
    if (Bx == Ax)
        return;

    if (Ax > Bx)
    {
        std::swap(Ax, Bx);
        std::swap(Au, Bu);
        std::swap(Av, Bv);
    }

    const int startX = std::max(Ax, 0);
    const int endX = std::min(Bx, SCREEN_WIDTH - 1);
    const fixed_t len = static_cast<fixed_t>(Bx) - Ax;

    const fixed_t du = (Bu - Au) / len;
    const fixed_t dv = (Bv - Av) / len;

    fixed_t u = Au + du * (static_cast<fixed_t>(startX) - Ax);
    fixed_t v = Av + dv * (static_cast<fixed_t>(startX) - Ax);

    for (int x = startX; x <= endX; ++x)
    {
        const int pixelColor = get_uv_map(TO_INT(u), TO_INT(v));
        DrawPixel(x, y, pixelColor);
        u += du;
        v += dv;
    }
}

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2)
{
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    if (v2.y == v0.y)
        return;

    const int startY = std::max(v0.y, 0);
    const int endY = std::min(v2.y, SCREEN_HEIGHT - 1);

    for (int y = startY; y <= endY; ++y)
    {
        const bool second_half = y > v1.y || v1.y == v0.y;
        const Edge A = CalcEdge(v0, v2, y);
        const Edge B = second_half ? CalcEdge(v1, v2, y) : CalcEdge(v0, v1, y);
        ProcessScanline(y, A.x, B.x, A.u, A.v, B.u, B.v);
    }
}
