#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
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
    Mesh* mesh = load_model();
    delete mesh;
    return 0;
}