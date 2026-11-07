#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "world.hpp"
#include "bench.hpp"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

clock_t lastTime = 0;

enum class AppMode
{
    Demo = 0,
    Benchmark = 1
};

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

static void print_bench_result(const BenchResult& result)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "bench:%s fps:%.2f ms/frame:%.3f",
             result.name,
             result.fps,
             result.avg_ms_per_frame);

    dtext(0, 0, C_WHITE, buffer);
}

int main(){
    //clock_set_speed(CLOCK_SPEED_F5);
    srand(time(NULL));
    init_uv_map();

    AppMode mode = AppMode::Demo;
    BenchId selected_bench = BenchId::TriangleThroughput;

    BenchConfig cfg;
    cfg.warmup_frames = 5;
    cfg.measure_frames = 30;
    cfg.triangle_count = 64;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;

    Camera *camera = new Camera(Vector3<int>(0, 100, -300), 0);
    World *world = new World(16, 16, 16, new Block*[2]{nullptr, new Cube()});
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

        if(keydown(KEY_F1))
            clock_set_speed(CLOCK_SPEED_F1);
        if(keydown(KEY_F5))
            clock_set_speed(CLOCK_SPEED_F5);
        if(keydown(KEY_F6))
        {
            mode = AppMode::Benchmark;
            selected_bench = BenchId::TriangleThroughput;
        }

        if (mode == AppMode::Benchmark)
        {
            BenchResult result;
            if (run_benchmark(selected_bench, cfg, &result))
            {
                print_bench_result(result);
                dupdate();
                while (!keydown(KEY_MENU))
                    clearevents();
                mode = AppMode::Demo;
            }
        }
        else
        {
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
    }
    return 0;
}
