#include "shapes.hpp"
#include <algorithm>
#include <vector>

struct Edge { int x, y; fixed_t u, v; };

static inline Edge CalcEdge(const VertexLocal& a, const VertexLocal& b, int y)
{
    const int deltaY = b.y - a.y;
    if (deltaY == 0) return {a.x, y, a.u, a.v};
    const fixed_t factor = fixed_t(y - a.y) / deltaY;
    const int x = a.x + static_cast<int>(factor * (b.x - a.x));
    const fixed_t u = a.u + ((b.u - a.u) * factor);
    const fixed_t v = a.v + ((b.v - a.v) * factor);
    return {x, y, u, v};
}

static inline void ProcessScanline(int y, int Ax, int Bx, fixed_t Au, fixed_t Av, fixed_t Bu, fixed_t Bv) {
    if (Bx == Ax) return;
    if (Ax > Bx) {
        std::swap(Ax, Bx);
        std::swap(Au, Bu);
        std::swap(Av, Bv);
    }

    const int startX = std::max(Ax, 0);
    const int endX = std::min(Bx, SCREEN_WIDTH - 1);

    const fixed_t invLen = fixed_t::one() / (Bx - Ax);
    const fixed_t du = (Bu - Au) * invLen;
    const fixed_t dv = (Bv - Av) * invLen;

    const int f = startX - Ax;
    fixed_t u = Au + du * f;
    fixed_t v = Av + dv * f;

    for (int x = startX; x <= endX; ++x) {
        DrawPixelTexture(static_cast<int>(u), static_cast<int>(v), x, y);
        u += du;
        v += dv;
    }
};

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2)
{
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    if (v2.y == v0.y) return;

    int startY = std::max(v0.y, 0);
    int endY = std::min(v2.y, SCREEN_HEIGHT - 1);

    for (int y = startY; y <= endY; ++y) {
        const bool second_half = y > v1.y || v1.y == v0.y;
        Edge A = CalcEdge(v0, v2, y);
        Edge B = second_half ? CalcEdge(v1, v2, y) : CalcEdge(v0, v1, y);
        ProcessScanline(y, A.x, B.x, A.u, A.v, B.u, B.v);
    }
};

/*struct Edge { int x, y; fixed_t u, v; };

static inline Edge CalcEdge(const VertexLocal& a, const VertexLocal& b, int y) {
    const int deltaY = b.y - a.y;
    if (deltaY == 0) return {a.x, y, a.u, a.v};
    const fixed_t factor = fixed_t(y - a.y) / deltaY;
    return {
        a.x + static_cast<int>(factor * (b.x - a.x)),
        y,
        a.u + (b.u - a.u) * factor,
        a.v + (b.v - a.v) * factor
    };
}

static inline void ProcessScanline(int y, int Ax, int Bx, fixed_t Au, fixed_t Av, fixed_t Bu, fixed_t Bv) {
    if (Bx == Ax) return;

    if (Ax > Bx) {
        std::swap(Ax, Bx);
        std::swap(Au, Bu);
        std::swap(Av, Bv);
    }

    int startX = std::max(Ax, 0);
    int endX = std::min(Bx, SCREEN_WIDTH - 1);
    int len = Bx - Ax;

    if (len == 0) return;

    fixed_t invLen = fixed_t::one() / len;
    fixed_t du = (Bu - Au) * invLen;
    fixed_t dv = (Bv - Av) * invLen;

    int dx = startX - Ax;
    fixed_t u = Au + du * dx;
    fixed_t v = Av + dv * dx;

    int count = endX - startX + 1;
    int i = 0;

    // --- Loop Unrolling ---
    for (; i + 1 < count; i += 2) {
        int x = startX + i;
        DrawPixelTexture(int(u), int(v), x, y);
        u += du;
        v += dv;
        DrawPixelTexture(int(u), int(v), x + 1, y);
        u += du;
        v += dv;
    }
    if (i < count) {
        int x = startX + i;
        DrawPixelTexture(int(u), int(v), x, y);
    }
}

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2) {
    // Tri par y
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    int totalHeight = v2.y - v0.y;
    if (totalHeight == 0) return;

    int startY = std::max(v0.y, 0);
    int endY = std::min(v2.y, SCREEN_HEIGHT - 1);

    if (endY - startY + 1 <= 0) return;

    // --- Pipeline de lignes ---
    std::vector<Edge> edgeA(endY - startY + 1);
    std::vector<Edge> edgeB(endY - startY + 1);

    for (int y = startY; y <= endY; ++y) {
        bool second_half = y > v1.y || v1.y == v0.y;
        edgeA[y - startY] = CalcEdge(v0, v2, y);
        edgeB[y - startY] = second_half ? CalcEdge(v1, v2, y) : CalcEdge(v0, v1, y);
    }

    for (int y = startY; y <= endY; ++y) {
        const Edge& A = edgeA[y - startY];
        const Edge& B = edgeB[y - startY];
        ProcessScanline(y, A.x, B.x, A.u, A.v, B.u, B.v);
    }
}*/
