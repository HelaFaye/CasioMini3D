#include "Model.hpp"

Mesh* load_model()
{
    Mesh* mesh = new Mesh(663);
    {
        Vector3<int> vertices[4] = {
            {0.049266f, 1.572816f, 0.080508f},
            {0.055005f, 1.575921f, 0.079139f},
            {0.052587f, 1.58327f, 0.08076f},
            {0.046274f, 1.579852f, 0.082223f},
        };
        Vector2<float> uvs[4] = {
            {0.25f, 0.351423f},
            {0.375f, 0.351423f},
            {0.375f, 0.407292f},
            {0.25f, 0.407292f},
        };
        mesh->faces.push_back(Face(4, vertices, uvs, nullptr));
    }
}