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
    char fps_buffer[32];
    char frame_buffer[32];
    char elapsed_buffer[32];
    snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %.2f", result.fps);
    snprintf(frame_buffer, sizeof(frame_buffer), "ms/frame: %.3f", result.avg_ms_per_frame);
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "Elapsed: %lu ms", (unsigned long)result.elapsed_ms);

    dtext(8, 12, C_WHITE, "Benchmark complete");
    dtext(8, 32, C_WHITE, result.name);
    dtext(8, 52, C_WHITE, fps_buffer);
    dtext(8, 72, C_WHITE, frame_buffer);
    dtext(8, 92, C_WHITE, elapsed_buffer);
    dtext(8, 116, C_WHITE, "MENU: return");
}

static void print_benchmark_legend()
{
    dtext(8, 156, C_WHITE, "F6: clear-only");
    dtext(8, 170, C_WHITE, "EXE: moving triangles");
    dtext(8, 184, C_WHITE, "MENU: quit");
}

int main(){
    //clock_set_speed(CLOCK_SPEED_F5);
    srand(time(NULL));
    init_uv_map();

    AppMode mode = AppMode::Demo;
    BenchId selected_bench = BenchId::ClearOnly;

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
            selected_bench = BenchId::ClearOnly;
            mode = AppMode::Benchmark;
        }
        if(keydown(KEY_EXE))
        {
            selected_bench = BenchId::TriangleThroughput;
            mode = AppMode::Benchmark;
        }

        if (mode == AppMode::Benchmark)
        {
            BenchResult result;
            if (run_benchmark(selected_bench, cfg, &result))
            {
                dclear(C_BLACK);
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
            print_benchmark_legend();
            dupdate();
        }
    }
    return 0;
}
