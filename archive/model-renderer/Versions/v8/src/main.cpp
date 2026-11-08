#include "dout.hpp"
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
clock_t deltaTime = 0;

void dclock()
{
    deltaTime = clock() - lastTime;
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
        Vector3<float> position = {x*40, y*40, z*40};
        mesh->vertices[i].position = position;
        mesh->vertices[i].projected = Vector3<fixed_t>();
    }
    for (unsigned int i = 0; i < uvCount; i++)
    {
        float u, v;
        fread(&u, sizeof(float), 1, file);
        fread(&v, sizeof(float), 1, file);
        mesh->uvs[i] = Vector2<fixed_t>{fixed_t(u), fixed_t(v)};
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
    mesh->CalculateNormals();
    fclose(file);
    return mesh;
}

void debugVector(Vector3<fixed_t> v, int x, int y)
{
    Vector3<float> vf = v;
    auto format_float = [](float val, char* out) {
        int sign = val < 0 ? -1 : 1;
        val = fabsf(val);

        int int_part = (int)val;
        int dec_part = (int)((val - int_part) * 1000);
        sprintf(out, "%s%d.%03d", (sign < 0 ? "-" : ""), int_part, dec_part);
    };
    char sx[16], sy[16], sz[16];
    format_float(vf.x, sx);
    format_float(vf.y, sy);
    format_float(vf.z, sz);
    char buffer[100];
    sprintf(buffer, "x: %s y: %s z: %s", sx, sy, sz);
    dtext(x, y, C_WHITE, buffer);
}


void CameraMove(Camera* camera)
{
    /*const fixed_t moveSpeed = fixed_t(25.0f);
    const fixed_t rotSpeed = fixed_t(0.2f);*/
    //adaptation des vitesses en fonction de deltaTime
    float dt = (float)deltaTime / CLOCKS_PER_SEC;
    const fixed_t moveSpeed = fixed_t(160.0f * dt);
    const fixed_t rotSpeed = fixed_t(0.8f * dt);


    // Axes de la caméra (toujours à jour avec la rotation)
    Vector3<fixed_t> forward = camera->rotation.Rotate(Vector3<fixed_t>(0, 0, 1));
    Vector3<fixed_t> right   = camera->rotation.Rotate(Vector3<fixed_t>(1, 0, 0));
    Vector3<fixed_t> up      = camera->rotation.Rotate(Vector3<fixed_t>(0, 1, 0));

    // Déplacement dans l'espace local
    if (keydown(KEY_7))
        camera->position += (forward * moveSpeed);
    if (keydown(KEY_1))
        camera->position -= (forward * moveSpeed);
    if (keydown(KEY_6))
        camera->position += (right * moveSpeed);
    if (keydown(KEY_4))
        camera->position -= (right * moveSpeed);
    if (keydown(KEY_8))
        camera->position += (up * moveSpeed);
    if (keydown(KEY_2))
        camera->position -= (up * moveSpeed);

    if (keydown(KEY_F3))//Test
    {
        Quaternion q = Quaternion(Vector3<fixed_t>(0, 1, 0));
        //Quaternion::AngleAxis(fixed_t(0.1f), Vector3<fixed_t>(0, 1, 0));
        casio::dout(0, 70, C_WHITE) << "rot: " << q;
    }


    // Rotation dans l'espace **local**
    // Pitch (lever / baisser la tête) autour de right (X local)
    if (keydown(KEY_UP))//vers le haut
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, right) * camera->rotation;
    if (keydown(KEY_DOWN))//vers le bas
        camera->rotation = Quaternion::AngleAxis(rotSpeed, right) * camera->rotation;

    // Yaw (tourner gauche/droite) autour de up (Y local)
    if (keydown(KEY_LEFT))//vers la gauche
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, up) * camera->rotation;
    if (keydown(KEY_RIGHT))//vers la droite
        camera->rotation = Quaternion::AngleAxis(rotSpeed, up) * camera->rotation;

    // Roll (pencher la tête) autour de forward (Z local)
    if (keydown(KEY_F5))//pencher à gauche
        camera->rotation = Quaternion::AngleAxis(rotSpeed, forward) * camera->rotation;
    if (keydown(KEY_F6))//pencher à droite
        camera->rotation = Quaternion::AngleAxis(-rotSpeed, forward) * camera->rotation;
}

int main2(){
    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();
        VertexLocal tl = {(10), (10), fixed_t(0), fixed_t(0)};
        VertexLocal tr = {(200), (10), fixed_t(255), fixed_t(0)};
        VertexLocal br = {(200), (200), fixed_t(255), fixed_t(255)};
        VertexLocal bl = {(10), (200), fixed_t(0), fixed_t(255)};
        DrawTriangle(tl, tr, br);
        DrawTriangle(tl, br, bl);

        /*casio::dout(0, 70, C_WHITE) << ""<< (fixed_t(1.5)*2.0f);
        casio::dout(0, 80, C_WHITE) << ""<< (fixed_t(1.5)/2.0f);
        casio::dout(0, 90, C_WHITE) << ""<< (fixed_t(1.5)+2.0f);
        casio::dout(0, 100, C_WHITE) << ""<< (fixed_t(1.5)-2.0f);*/
        dclock();
        dupdate();
    }
    return 0;
}



int main(){
    //init_uv_map();
    char path[] = "Model.bin";
    Mesh* mesh = (Mesh*)gint_world_switch(GINT_CALL(LoadModel,(void *)path));
    Camera* camera = new Camera();
    camera->position = Vector3<int>{0, 235, -475};
    while (!keydown(KEY_MENU))
    {
        dclear(C_BLACK);
        clearevents();

        if(keydown(KEY_F1))
            clock_set_speed(CLOCK_SPEED_F1);
        if(keydown(KEY_F2))
            clock_set_speed(CLOCK_SPEED_F5);

        CameraMove(camera);
        mesh->CalculateProjection(Vector3<fixed_t>());
        mesh->Render();
        camera->Draw();
        dclock();
        camera->PrintRenderFaceCount();
        debugVector(camera->position, 0, 40);
        dupdate();
    }
    delete mesh;
    return 0;
}