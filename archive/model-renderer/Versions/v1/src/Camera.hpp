#ifndef CAMERA_HPP
#define CAMERA_HPP
#include "Mesh.hpp"
#include "types.hpp"

#define MAX_QUADS 60

extern RenderFace quads[MAX_QUADS];

class Camera
{
private:
    /* data */
public:
    Vector3<int> position;
    Camera(/* args */);
    ~Camera();
    void CalculateProjection(Vertex* vertice, fVector3 offset);
};

#endif