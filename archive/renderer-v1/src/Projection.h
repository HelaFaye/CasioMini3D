#ifndef PROJECTION_H
#define PROJECTION_H
#include "fixed.h"

#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/display.h>
#include <gint/rtc.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <gint/dma.h>

void gint_dhline(int x1, int x2, int y, color_t color);

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define clamp(x, a, b) (x < a ? a : (x > b ? b : x))

#define rgb(r, g, b) ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3)



#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923

#define SCREEN_WIDTH 396
#define SCREEN_HEIGHT 224

typedef enum DrawMode
{
    DRAW_WIREFRAME,
    DRAW_COLOR,
    DRAW_TEXTURE
} DrawMode;

typedef enum ModeUV
{
    UV_LINEAR,
    UV_NEAREST
} ModeUV;

typedef struct Vector3
{
    int x;
    int y;
    int z;
} Vector3;

typedef struct Vector2
{
    int x;
    int y;
} Vector2;

typedef struct Vector3f
{
    float x;
    float y;
    float z;
} Vector3f;

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

typedef struct Transform
{
    Vector3 position;
    Vector3f rotation;
    Vector3f scale;
} Transform;

typedef struct Vertex
{
    Vector3 position;
    fVector3 projected;
} Vertex;

typedef struct RenderQuad
{
    fVector2 points[4];
    fixed_t z;
    int color;
    DrawMode mode;
    ModeUV uv_mode;
} RenderQuad;

typedef struct Mesh
{
    Vertex *vertices;
    size_t verticesCount;
} Mesh;

typedef struct Projection
{
    Mesh *mesh;
    Transform transform;
} Projection;

typedef struct Camera
{
    Transform transform;
} Camera;

void CalculateProjection(Camera *camera, Projection* obj);


void init_uv_map();

float sin_approx(float i);

float cos_approx(float i);

fixed_t fsin_approx(fixed_t i);

fixed_t fcos_approx(fixed_t i);


void DrawPixel(int x, int y, int color);

void DrawWireframeQuad(fVector2 points[4], int color);

void DrawFilledQuadColor(fVector2 points[4], int color);

void DrawFilledQuadTexture(fVector2 points[4], ModeUV uv_mode);

#endif // PROJECTION_H