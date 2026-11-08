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

void Face::CalculateNormal(Vertex* vertices)
{
    Vector3<fixed_t> v1 = vertices[this->vertices[1]].position - vertices[this->vertices[0]].position;
    Vector3<fixed_t> v2 = vertices[this->vertices[2]].position - vertices[this->vertices[0]].position;
    this->normal = v1.cross(v2);
    this->normal = this->normal.normalize();
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

void Mesh::CalculateProjection(Vector3<fixed_t> position)
{
    for (unsigned int i = 0; i < this->vertex_count; i++)
    {
        Camera::current->CalculateProjection(&this->vertices[i], position);
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
    this->depth = fixed_t(0);
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