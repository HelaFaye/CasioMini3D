#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*typedef struct Quad
{
    Vector2<int> points[4];
    Vector2<float> uv[4];
} Quad;

inline int Clamp(int x, int min, int max) {
    return (x < min) ? min : ((x > max) ? max : x);
}

void DebugText(const char *text)
{
    drect(0, 30, 80, 50, C_BLACK);
    dtext(0, 30, C_WHITE, text);
    dupdate();
    while (!keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_MENU))
        clearevents();
}*/

/*void DrawQuad(Quad quad)
{
    int texW = 40;
    int texH = 40;
    //GetTextureSize(quad.texture, &texW, &texH);

    struct Vertex { int x, y, u, v; } verts[4];
    for (int i = 0; i < 4; ++i) {
        verts[i].x = quad.points[i].x;
        verts[i].y = quad.points[i].y;
        verts[i].u = FLOAT_TO_FIXED(quad.uv[i].x * (texW - 1));
        verts[i].v = FLOAT_TO_FIXED(quad.uv[i].y * (texH - 1));
    }

    auto DrawTriangle = [&](Vertex v0, Vertex v1, Vertex v2) {
        if (v1.y < v0.y) std::swap(v0, v1);
        if (v2.y < v0.y) std::swap(v0, v2);
        if (v2.y < v1.y) std::swap(v1, v2);

        int totalHeight = v2.y - v0.y;
        if (totalHeight == 0) return;

        for (int y = Clamp(v0.y, 0, SCREEN_HEIGHT - 1); y <= Clamp(v2.y, 0, SCREEN_HEIGHT - 1); ++y) {
            bool second_half = y > v1.y || v1.y == v0.y;
            int segmentHeight = second_half ? v2.y - v1.y : v1.y - v0.y;
            if (segmentHeight == 0) continue;

            int alpha = ((y - v0.y) << PRECISION) / totalHeight;
            int beta = ((y - (second_half ? v1.y : v0.y)) << PRECISION) / segmentHeight;

            Vertex A = { 
                v0.x + TO_INT((v2.x - v0.x) * alpha),
                y,
                v0.u + ((v2.u - v0.u) * alpha >> PRECISION),
                v0.v + ((v2.v - v0.v) * alpha >> PRECISION)
            };

            Vertex B = second_half ?
                Vertex{
                    v1.x + TO_INT((v2.x - v1.x) * beta),
                    y,
                    v1.u + ((v2.u - v1.u) * beta >> PRECISION),
                    v1.v + ((v2.v - v1.v) * beta >> PRECISION)
                } :
                Vertex{
                    v0.x + TO_INT((v1.x - v0.x) * beta),
                    y,
                    v0.u + ((v1.u - v0.u) * beta >> PRECISION),
                    v0.v + ((v1.v - v0.v) * beta >> PRECISION)
                };

            if (A.x > B.x) std::swap(A, B);
            int len = B.x - A.x;
            if (len == 0) continue;

            int du = (B.u - A.u) / len;
            int dv = (B.v - A.v) / len;
            int u = A.u;
            int v = A.v;

            int lastU = -1, lastV = -1, color = 123456;
            for (int x = Clamp(A.x, 0, SCREEN_WIDTH - 1); x <= Clamp(B.x, 0, SCREEN_WIDTH - 1); ++x) {
                int currU = TO_INT(u);
                int currV = TO_INT(v);

                if (currU != lastU || currV != lastV) {
                    //GetPixel(quad.texture, currU, currV, &r, &g, &b);
                    / *char coords[50];
                    sprintf(coords, "U: %d, V: %d", currU, currV);
                    DebugText(coords);
                    if (currU < 0 || currU >= 40 || currV < 0 || currV >= 40)
                    {
                        currU = Clamp(currU, 0, 39);
                        currV = Clamp(currV, 0, 39);
                    }* /
                    color = get_uv_map(currU, currV);
                    lastU = currU;
                    lastV = currV;
                }
                DrawPixel(x, y, color);

                u += du;
                v += dv;
            }
        }
    };

    DrawTriangle(verts[0], verts[1], verts[2]);
    DrawTriangle(verts[0], verts[2], verts[3]);
}*/

/*void DrawQuad2(Quad quad)
{
    int texW = 40;
    int texH = 40;
    //GetTextureSize(quad.texture, &texW, &texH);
    int scrW = SCREEN_WIDTH;
    int scrH = SCREEN_HEIGHT;

    struct Vertex { int x, y, u, v; } verts[4];
    for (int i = 0; i < 4; ++i) {
        verts[i].x = quad.points[i].x;
        verts[i].y = quad.points[i].y;
        verts[i].u = FLOAT_TO_FIXED(quad.uv[i].x * (texW - 1));
        verts[i].v = FLOAT_TO_FIXED(quad.uv[i].y * (texH - 1));
    }

    // Split quad into two triangles (0,1,2) and (0,2,3)
    auto DrawTriangle = [&](Vertex v0, Vertex v1, Vertex v2) {
        int minX = Clamp(std::min({v0.x, v1.x, v2.x}), 0, scrW - 1);
        int minY = Clamp(std::min({v0.y, v1.y, v2.y}), 0, scrH - 1);
        int maxX = Clamp(std::max({v0.x, v1.x, v2.x}), 0, scrW - 1);
        int maxY = Clamp(std::max({v0.y, v1.y, v2.y}), 0, scrH - 1);

        int A01 = v0.y - v1.y, B01 = v1.x - v0.x;
        int A12 = v1.y - v2.y, B12 = v2.x - v1.x;
        int A20 = v2.y - v0.y, B20 = v0.x - v2.x;

        int area = (B12 * A20 - B20 * A12);
        if (area == 0) return;

        int inv_area = (1 << 24) / area;

        int w0_row = (A12 * (minX - v1.x) + B12 * (minY - v1.y));
        int w1_row = (A20 * (minX - v2.x) + B20 * (minY - v2.y));
        int w2_row = (A01 * (minX - v0.x) + B01 * (minY - v0.y));

        for (int y = minY; y <= maxY; ++y)
        {
            int w0 = w0_row, w1 = w1_row, w2 = w2_row;
            for (int x = minX; x <= maxX; ++x)
            {
                if ((w0 | w1 | w2) >= 0)
                {
                    int u = (w0 * v0.u + w1 * v1.u + w2 * v2.u) * inv_area >> 8;
                    int v = (w0 * v0.v + w1 * v1.v + w2 * v2.v) * inv_area >> 8;
                    DrawPixel(x, y, get_uv_map(TO_INT(u), TO_INT(v)));
                }
                w0 += A12; w1 += A20; w2 += A01;
            }
            w0_row += B12; w1_row += B20; w2_row += B01;
        }
    };

    // Triangle 1
    DrawTriangle(verts[0], verts[1], verts[2]);
    // Triangle 2
    DrawTriangle(verts[0], verts[2], verts[3]);
}*/


clock_t lastTime = 0;

void dclock()
{
    clock_t deltaTime = clock() - lastTime;
    int fps = 1000;
    if (deltaTime != 0)
        fps = CLOCKS_PER_SEC / deltaTime;

    char buffer[20];
    sprintf(buffer, "FPS: %d", fps);
    dtext(0, 0, C_WHITE, buffer);
    lastTime = clock();
}
int main(){
    //clock_set_speed(CLOCK_SPEED_F5);
    srand(time(NULL));
    init_uv_map();
    /*Quad quad;
    quad.points[0] = Vector2<int>(100, 100);
    quad.points[1] = Vector2<int>(200, 100);
    quad.points[2] = Vector2<int>(200, 200);
    quad.points[3] = Vector2<int>(100, 200);
    quad.uv[0] = Vector2<float>(0, 0);
    quad.uv[1] = Vector2<float>(1, 0);
    quad.uv[2] = Vector2<float>(1, 1);
    quad.uv[3] = Vector2<float>(0, 1);
    while (true)
    {
        dclear(C_BLACK);
        clearevents();
        for (size_t i = 0; i < 100; i++)
            DrawQuad(quad);
        dclock();
        dupdate();
    }*/
    Camera *camera = new Camera(Vector3<int>(0, 100, -300), 0);
    World *world = new World(16, 16, 16, new Block*[2]{nullptr,new Cube()});
    world->set_id_unsafe(0, 0, 0, 1);
    world->set_id_unsafe(1, 0, 0, 1);
    for (size_t i = 0; i < 200; i++)
    {
        world->set_id_unsafe(rand() % 16, rand() % 16, rand() % 16, 1);
    }
    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();

        if(keydown(KEY_LEFT))
            camera->position.x -= 10;
        if(keydown(KEY_RIGHT))
            camera->position.x += 10;
        if(keydown(KEY_UP))
            camera->position.z += 10;
        if(keydown(KEY_DOWN))
            camera->position.z -= 10;


        if(keydown(KEY_8))
            camera->position.y += 10;
        if(keydown(KEY_2))
            camera->position.y -= 10;

        if(keydown(KEY_7))
            camera->angle += 1;
        if(keydown(KEY_1))
            camera->angle -= 1;

        if(keydown(KEY_9))
            camera->offset.y -= 10;
        if(keydown(KEY_3))
            camera->offset.y += 10;

        world->Render();
        camera->Render();
        dclock();
        dupdate();
    }
    return 0;
}