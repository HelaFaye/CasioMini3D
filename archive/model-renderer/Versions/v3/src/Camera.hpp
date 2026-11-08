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
    Vector3<fixed_t> fposition;
    static RenderFace faces[MAX_FACES];
    void SortFaces();
public:
    static Camera* current;
    Vector3<int> position;
    Vector2<int> offset;
    Quaternion rotation;
    Camera();
    ~Camera();
    void UpdateCoordinates();
    void CalculateProjection(Vertex* vertice, Vector3<fixed_t> offset);
    void AddFace(RenderFace face);
    void Draw();
    void PrintRenderFaceCount();
};

#endif