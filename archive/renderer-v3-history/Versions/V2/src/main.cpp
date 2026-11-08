#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
            camera->angle += 3;
        if(keydown(KEY_1))
            camera->angle -= 3;

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