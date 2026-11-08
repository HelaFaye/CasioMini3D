#ifndef MESH_H
#define MESH_H
#include <vector>
#include "types.hpp"
#include "fixed.hpp"
#include "display.hpp"

typedef struct Vertex
{
    Vector3<fixed_t> position;
    Vector3<fixed_t> projected;
} Vertex;

class Face
{
public:
    int length;
    unsigned int vertices[4];
    unsigned int uvs[4];
    Vector3<fixed_t> normal;
    bopti_image_t* texture;
    Face();
    Face(int length, unsigned int V[4], unsigned int UV[4], bopti_image_t* texture);
    ~Face();
    void CalculateNormal(Vertex* vertices);
};

class Mesh
{
public:
    Face* faces;
    Vertex* vertices;
    Vector2<fixed_t>* uvs;
    unsigned int vertex_count;
    unsigned int uv_count;
    unsigned int face_count;
    Mesh(unsigned int vertex_count, unsigned int uv_count, unsigned int face_count);
    ~Mesh();
    void CalculateProjection(Vector3<fixed_t> position);
    void CalculateNormals();
    void Render();
};

class RenderFace
{
public:
    Mesh* mesh;
    Face* face;
    fixed_t depth;
    RenderFace(Mesh* mesh, Face* face);
    RenderFace();
    ~RenderFace();
    void CalculateDepth();
};

#endif