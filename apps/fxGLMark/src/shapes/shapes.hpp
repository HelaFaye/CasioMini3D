#ifndef SHAPES_H
#define SHAPES_H
#include "../types.hpp"
#include "../display.hpp"
#include "../fixed.hpp"
#include "gfx/texture.hpp"
#include <algorithm>
#include <tuple>

struct VertexLocal { int x, y; fixed_t u, v; };

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2);
void DrawTriangleSolid(VertexLocal v0, VertexLocal v1, VertexLocal v2, int color);
void DrawTriangleTextured(
    VertexLocal v0,
    VertexLocal v1,
    VertexLocal v2,
    const gfx::Texture2DView& texture,
    const gfx::TextureSampler& sampler);
void DrawRectUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawRect(VertexLocal &tl, VertexLocal &br);
void DrawHorzParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawHorzParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawVertParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawVertParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);

#endif