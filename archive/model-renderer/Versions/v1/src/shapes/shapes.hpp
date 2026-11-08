#ifndef SHAPES_H
#define SHAPES_H
#include "../types.hpp"
#include "../display.hpp"
#include "../fixed.hpp"
#include <algorithm>
#include <tuple>

struct VertexLocal { int x, y; fixed_t u, v; };

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2);
void DrawRectUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawRect(VertexLocal &tl, VertexLocal &br);
void DrawHorzParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawHorzParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawVertParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);
void DrawVertParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl);

#endif