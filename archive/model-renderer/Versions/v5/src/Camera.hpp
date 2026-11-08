#ifndef CAMERA_HPP
#define CAMERA_HPP
#include "Mesh.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstdio>

#define half_screen_width INT_TO_FIXED(SCREEN_WIDTH / 2);
#define half_screen_height INT_TO_FIXED(SCREEN_HEIGHT / 2);

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
    fixed_t fov;
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