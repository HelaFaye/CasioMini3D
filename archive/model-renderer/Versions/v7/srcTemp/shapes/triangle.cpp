#include "shapes.hpp"

struct Edge { int x, y; fixed_t u, v; };

inline Edge CalcEdge(VertexLocal a, VertexLocal b, int y)
{
    const int deltaY = b.y - a.y;
    if (deltaY == 0) return {a.x, y, a.u, a.v};
    const fixed_t factor = INT_TO_FIXED(y - a.y) / deltaY;
    const int x = a.x + TO_INT((b.x - a.x) * factor);
    const fixed_t u = a.u + fmul((b.u - a.u), factor);
    const fixed_t v = a.v + fmul((b.v - a.v), factor);
    return {x, y, u, v};
};

inline void ProcessScanline(int y, int Ax, int Bx, fixed_t Au, fixed_t Av, fixed_t Bu, fixed_t Bv) {
    if (Bx - Ax == 0) return;
    if (Ax > Bx) {
        std::swap(Ax, Bx);
        std::swap(Au, Bu);
        std::swap(Av, Bv);
    }

    const int startX = std::max(Ax, 0);
    const int endX = std::min(Bx, SCREEN_WIDTH - 1);
    const int len = Bx - Ax;

    const int du = (Bu - Au) / len;
    const int dv = (Bv - Av) / len;

    fixed_t u = Au + du * (startX - Ax);
    fixed_t v = Av + dv * (startX - Ax);

    for (int x = startX; x <= endX; ++x) {
        DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
        u += du;
        v += dv;
    }
};

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2)
{
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    if (v2.y - v0.y == 0) return;

    int startY = std::max(v0.y, 0);
    int endY = std::min(v2.y, SCREEN_HEIGHT - 1);

    for (int y = startY; y <= endY; ++y) {
        const bool second_half = y > v1.y || v1.y == v0.y;
        Edge A = CalcEdge(v0, v2, y);
        Edge B = second_half ? CalcEdge(v1, v2, y) : CalcEdge(v0, v1, y);
        ProcessScanline(y, A.x, B.x, A.u, A.v, B.u, B.v);
    }
};