#ifndef CAMERA_HPP
#define CAMERA_HPP
#include "Mesh.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstdio>

#define MAX_FACES 4000

class Camera
{
private:
    size_t last_face_count = 0;
    size_t face_count = 0;
    static RenderFace faces[MAX_FACES];
    void SortFaces();
public:
    static Camera* current;
    Vector3<fixed_t> position;
    Vector2<fixed_t> offset;
    Quaternion rotation;
    Camera();
    ~Camera();
    void CalculateProjection(Vertex* vertice, Vector3<fixed_t> position);
    void AddFace(RenderFace face);
    void Draw();
    void PrintRenderFaceCount();
};

#endif