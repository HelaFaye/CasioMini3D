#include "display.hpp"
#include "fixed.hpp"
#include "types.hpp"
#include "Mesh.hpp"
#include "shapes/shapes.hpp"
#include "Camera.hpp"
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

Mesh* LoadModel(char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
        return NULL;
    unsigned int vertexCount, uvCount, faceCount;
    fread(&vertexCount, sizeof(unsigned int), 1, file);
    fread(&uvCount, sizeof(unsigned int), 1, file);
    fread(&faceCount, sizeof(unsigned int), 1, file);
    Mesh* mesh = new Mesh(vertexCount, uvCount, faceCount);
    for (unsigned int i = 0; i < vertexCount; i++)
    {
        float x, y, z;
        fread(&x, sizeof(float), 1, file);
        fread(&y, sizeof(float), 1, file);
        fread(&z, sizeof(float), 1, file);
        Vector3<float> position = {x, y, z};
        position = position * 80;
        mesh->vertices[i].position = position;
        mesh->vertices[i].projected = Vector3<fixed_t>{0, 0, 0};
    }
    for (unsigned int i = 0; i < uvCount; i++)
    {
        float u, v;
        fread(&u, sizeof(float), 1, file);
        fread(&v, sizeof(float), 1, file);
        mesh->uvs[i] = Vector2<fixed_t>{FLOAT_TO_FIXED(u), FLOAT_TO_FIXED(v)};
    }
    for (unsigned int i = 0; i < faceCount; i++)
    {
        unsigned char length;
        fread(&length, sizeof(unsigned char), 1, file);
        unsigned int vertices[4];
        unsigned int uvs[4];
        for (int j = 0; j < length; j++)
        {
            unsigned int vertex, uv;
            fread(&vertex, sizeof(unsigned int), 1, file);
            fread(&uv, sizeof(unsigned int), 1, file);
            vertices[j] = vertex;
            uvs[j] = uv;
        }
        mesh->faces[i] = Face(length, vertices, uvs, NULL);
    }
    fclose(file);
    return mesh;
}

void debugVector(Vector3<fixed_t> v, int x, int y)
{
    Vector3<float> vf = vector3_cast<fixed_t, float>(v);

    int x_int = (int)vf.x;
    int x_dec = (int)((vf.x - x_int) * 1000);

    int y_int = (int)vf.y;
    int y_dec = (int)((vf.y - y_int) * 1000);

    int z_int = (int)vf.z;
    int z_dec = (int)((vf.z - z_int) * 1000);

    char buffer[100];
    sprintf(buffer, "x: %d.%03d y: %d.%03d z: %d.%03d",
            x_int, abs(x_dec), y_int, abs(y_dec), z_int, abs(z_dec));

    dtext(x, y, C_WHITE, buffer);
}


void CameraMove(Camera* camera)
{
    fixed_t moveSpeed = FLOAT_TO_FIXED(25.0f);
    fixed_t rotSpeed = FLOAT_TO_FIXED(0.2f);

    // Axes (sans roll)
    Vector3<fixed_t> forward = camera->rotation.Rotate(Vector3<fixed_t>(0, 0, FIXED_ONE));
    Vector3<fixed_t> right   = camera->rotation.Rotate(Vector3<fixed_t>(FIXED_ONE, 0, 0));
    Vector3<fixed_t> up      = camera->rotation.Rotate(Vector3<fixed_t>(0, FIXED_ONE, 0));
    Vector3<fixed_t> euler = camera->rotation.GetEuler();

    debugVector(forward, 0, 70);
    debugVector(right, 0, 85);
    debugVector(up, 0, 100);
    debugVector(euler, 0, 115);

    // Déplacement
    if (keydown(KEY_UP))
        camera->position += vector3_cast<fixed_t, int>(fmul(forward, moveSpeed));
    if (keydown(KEY_DOWN))
        camera->position -= vector3_cast<fixed_t, int>(fmul(forward, moveSpeed));
    if (keydown(KEY_RIGHT))
        camera->position += vector3_cast<fixed_t, int>(fmul(right, moveSpeed));
    if (keydown(KEY_LEFT))
        camera->position -= vector3_cast<fixed_t, int>(fmul(right, moveSpeed));
    if (keydown(KEY_8))
        camera->position += vector3_cast<fixed_t, int>(fmul(up, moveSpeed));
    if (keydown(KEY_2))
        camera->position -= vector3_cast<fixed_t, int>(fmul(up, moveSpeed));


    // ROTATIONS – à présent en local space
    if (keydown(KEY_7)) // Yaw + (tourner à gauche)
        camera->rotation = Quaternion::AngleAxis(rotSpeed, up) * camera->rotation;
    if (keydown(KEY_1)) // Yaw -
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, up) * camera->rotation;

    if (keydown(KEY_9)) // Pitch + (lever la tête)
        camera->rotation = Quaternion::AngleAxis(rotSpeed, right) * camera->rotation;
    if (keydown(KEY_3)) // Pitch -
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, right) * camera->rotation;

    if (keydown(KEY_4)) // Roll + (pencher la tête à gauche)
        camera->rotation = Quaternion::AngleAxis(rotSpeed, forward) * camera->rotation;
    if (keydown(KEY_6)) // Roll -
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, forward) * camera->rotation;

}


int main(){
    //init_uv_map();
    char path[] = "Model.bin";
    Mesh* mesh = (Mesh*)gint_world_switch(GINT_CALL(LoadModel,(void *)path));
    Camera* camera = new Camera();
    camera->position = Vector3<int>{0, 200, -1300};
    camera->offset = Vector2<int>{0, 0};
    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();

        if(keydown(KEY_F1))
            clock_set_speed(CLOCK_SPEED_F1);
        if(keydown(KEY_F5))
            clock_set_speed(CLOCK_SPEED_F5);

        CameraMove(camera);


        camera->UpdateCoordinates();
        Vector3<fixed_t> offset = {INT_TO_FIXED(0), INT_TO_FIXED(0), INT_TO_FIXED(0)};
        mesh->CalculateProjection(offset);
        mesh->Render();
        camera->Draw();
        dclock();
        camera->PrintRenderFaceCount();
        dupdate();
    }
    delete mesh;
    return 0;
}