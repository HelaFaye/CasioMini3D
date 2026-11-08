#include "Mesh.hpp"
#include "Camera.hpp"
#include <algorithm>

Face::Face()
{
}

Face::Face(int length, unsigned int V[4], unsigned int UV[4], bopti_image_t* texture)
{
    this->length = length;
    for (int i = 0; i < 4; i++)
    {
        vertices[i] = V[i];
        uvs[i] = UV[i];
    }
    this->texture = texture;
}

Face::~Face()
{
}

Mesh::Mesh(unsigned int vertex_count, unsigned int uv_count, unsigned int face_count)
{
    faces = new Face[face_count];
    vertices = new Vertex[vertex_count];
    uvs = new Vector2<fixed_t>[uv_count];
    this->vertex_count = vertex_count;
    this->uv_count = uv_count;
    this->face_count = face_count;
}

Mesh::~Mesh()
{
    delete[] faces;
    delete[] vertices;
    delete[] uvs;
}

void Mesh::CalculateProjection(Vector3<fixed_t> offset)
{
    for (unsigned int i = 0; i < this->vertex_count; i++)
    {
        Camera::current->CalculateProjection(&this->vertices[i], offset);
    }
}

void Mesh::Render()
{
    for(unsigned int i = 0; i < this->face_count; i++)
    {
        RenderFace face = RenderFace(this, &this->faces[i]);
        Camera::current->AddFace(face);
    }
}

RenderFace::RenderFace(Mesh* mesh, Face* face)
{
    this->mesh = mesh;
    this->face = face;
    this->CalculateDepth();
}

RenderFace::RenderFace()
{
    this->mesh = nullptr;
    this->face = nullptr;
    this->depth = 0;
}

RenderFace::~RenderFace()
{
}

void RenderFace::CalculateDepth()
{
    this->depth = 0;
    for (int i = 0; i < this->face->length; i++)
    {
        this->depth += this->mesh->vertices[this->face->vertices[i]].projected.z;
    }
    this->depth /= this->face->length;
}