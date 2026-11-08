#ifndef MESH_H
#define MESH_H
#include <vector>
#include "types.hpp"
#include "fixed.hpp"
#include "display.hpp"

typedef struct Vertex
{
    Vector3<int> position;
    Vector3<fixed_t> projected;
} Vertex;

class Face
{
public:
    int length;
    Vector3<size_t> vertices[4];
    Vector2<fixed_t> uvs[4];
    bopti_image_t* texture;
    Face(int length, Vector3<size_t> V[4], Vector2<float> UV[4], bopti_image_t* texture);
    ~Face();
};

typedef struct RenderFace
{
    int length;
    Vertex& vertices[4];
    Face& face;
} RenderFace;

class Mesh
{
public:
    std::vector<Face> faces;
    std::vector<Vertex> vertices;
    Mesh(int face_count);
    ~Mesh();
};

#endif