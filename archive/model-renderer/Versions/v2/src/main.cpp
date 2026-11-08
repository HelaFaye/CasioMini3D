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

void CameraMove(Camera* camera)
{
    float moveSpeed = 10.0f;
    float rotationSpeed = 0.1f; // en radian, plus fluide

    // Vecteurs de base
    Vector3<fixed_t> baseForward = Vector3<fixed_t>(0, 0, FIXED_ONE);
    Vector3<fixed_t> baseRight   = Vector3<fixed_t>(FIXED_ONE, 0, 0);
    Vector3<fixed_t> baseUp      = Vector3<fixed_t>(0, FIXED_ONE, 0);

    // Appliquer la rotation de la caméra aux vecteurs de base
    Vector3<fixed_t> forward = camera->rotation.Rotate(baseForward);
    Vector3<fixed_t> right   = camera->rotation.Rotate(baseRight);
    Vector3<fixed_t> up      = camera->rotation.Rotate(baseUp);

    if (keydown(KEY_UP))
        camera->position += vector3_cast<fixed_t, int>(fmul(forward, FLOAT_TO_FIXED(moveSpeed)));
    if (keydown(KEY_DOWN))
        camera->position -= vector3_cast<fixed_t, int>(fmul(forward, FLOAT_TO_FIXED(moveSpeed)));
    if (keydown(KEY_RIGHT))
        camera->position += vector3_cast<fixed_t, int>(fmul(right, FLOAT_TO_FIXED(moveSpeed)));
    if (keydown(KEY_LEFT))
        camera->position -= vector3_cast<fixed_t, int>(fmul(right, FLOAT_TO_FIXED(moveSpeed)));
    if (keydown(KEY_8))
        camera->position += vector3_cast<fixed_t, int>(fmul(up, FLOAT_TO_FIXED(moveSpeed)));
    if (keydown(KEY_2))
        camera->position -= vector3_cast<fixed_t, int>(fmul(up, FLOAT_TO_FIXED(moveSpeed)));

    // Rotations (pitch, yaw, roll)
    fixed_t fixedRotationSpeed = FLOAT_TO_FIXED(rotationSpeed);
    if (keydown(KEY_7)) // Pitch +
        camera->rotation *= Quaternion::AngleAxis(fixedRotationSpeed, Vector3<fixed_t>(0, FIXED_ONE, 0));
    if (keydown(KEY_1)) // Pitch -
        camera->rotation *= Quaternion::AngleAxis(-fixedRotationSpeed, Vector3<fixed_t>(0, FIXED_ONE, 0));

    if (keydown(KEY_9)) // Roll +
        camera->rotation *= Quaternion::AngleAxis(fixedRotationSpeed, Vector3<fixed_t>(FIXED_ONE, 0, 0));
    if (keydown(KEY_3)) // Roll -
        camera->rotation *= Quaternion::AngleAxis(-fixedRotationSpeed, Vector3<fixed_t>(FIXED_ONE, 0, 0));

    if (keydown(KEY_4)) // Yaw +
        camera->rotation *= Quaternion::AngleAxis(fixedRotationSpeed, Vector3<fixed_t>(0, 0, FIXED_ONE));
    if (keydown(KEY_6)) // Yaw -
        camera->rotation *= Quaternion::AngleAxis(-fixedRotationSpeed, Vector3<fixed_t>(0, 0, FIXED_ONE));
}


int main(){
    //init_uv_map();
    char path[] = "Model.bin";
    Mesh* mesh = (Mesh*)gint_world_switch(GINT_CALL(LoadModel,(void *)path));
    Camera* camera = new Camera();
    camera->position = Vector3<int>{0, 150, -350};
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