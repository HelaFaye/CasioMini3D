#include "Mesh.hpp"

Face::Face(int length, Vector3<float> V[4], Vector2<float> UV[4], bopti_image_t* texture)
{
    this->length = length;
    for (int i = 0; i < 4; i++)
    {
        vertices[i] = Vector3<fixed_t>(FLOAT_TO_FIXED(V[i].x), FLOAT_TO_FIXED(V[i].y), FLOAT_TO_FIXED(V[i].z));
        uvs[i] = Vector2<fixed_t>(FLOAT_TO_FIXED(UV[i].x), FLOAT_TO_FIXED(UV[i].y));
    }
    this->texture = texture;
}

Face::~Face()
{
}

Mesh::Mesh(int face_count)
{
    faces.reserve(face_count);
}

Mesh::~Mesh()
{
    faces.clear();
    faces.shrink_to_fit();
}
