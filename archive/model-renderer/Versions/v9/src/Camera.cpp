#include "Camera.hpp"
#include "shapes/shapes.hpp"

RenderFace Camera::faces[MAX_FACES];
Camera* Camera::current = nullptr;

Camera::Camera()
{
    if (Camera::current == nullptr)
        Camera::current = this;
    this->fov = fixed_t(SCREEN_WIDTH);
    this->position = Vector3<fixed_t>{0, 0, 0};
    this->offset = Vector2<fixed_t>{0, 0};
    this->rotation = Quaternion();
}

Camera::~Camera()
{
    if (Camera::current == this)
        Camera::current = nullptr;
}


/*inline static void ApplyRotationX(fixed_t *y, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_y = (*y * cos_theta) - (*z * sin_theta);
    const fixed_t new_z = (*y * sin_theta) + (*z * cos_theta);
    *y = new_y;
    *z = new_z;
}

inline static void ApplyRotationY(fixed_t *x, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = (*x * cos_theta) + (*z * sin_theta);
    const fixed_t new_z = (-*x * sin_theta) + (*z * cos_theta);
    *x = new_x;
    *z = new_z;
}

inline static void ApplyRotationZ(fixed_t *x, fixed_t *y, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = (*x * cos_theta) - (*y * sin_theta);
    const fixed_t new_y = (*x * sin_theta) + (*y * cos_theta);
    *x = new_x;
    *y = new_y;
}*/

/*void Camera::CalculateProjection(Vertex* vertice, Vector3<fixed_t> position)
{
    const fixed_t half_screen_width = fixed_t(SCREEN_WIDTH / 2);
    const fixed_t half_screen_height = fixed_t(SCREEN_HEIGHT / 2);
    Vector3<fixed_t> m = vertice->position;
    / * Application de la translation (incluant l'inverse de la translation de la caméra) * /
    m.x += position.x - this->position.x;
    m.y += position.y - this->position.y;
    m.z += position.z - this->position.z;
    / * Application de la rotation de la caméra * /
    const Vector3<fixed_t> cos_camera = this->rotation.GetCos();
    const Vector3<fixed_t> sin_camera = this->rotation.GetSin();
    ApplyRotationX(&m.y, &m.z, cos_camera.x, sin_camera.x);
    ApplyRotationY(&m.x, &m.z, cos_camera.y, sin_camera.y);
    ApplyRotationZ(&m.x, &m.y, cos_camera.z, sin_camera.z);
    / * Projection * /
    const fixed_t m_z = (m.z < fixed_t(1)) ? fixed_t(1) : m.z;
    const fixed_t f = (fov / m_z);
    / * Calcul final de la position projetée * /
    vertice->projected.x = (m.x * f) + half_screen_width;
    vertice->projected.y = (-m.y * f) + half_screen_height;
    vertice->projected.z = m.z;

    vertice->projected.x += this->offset.x;
    vertice->projected.y += this->offset.y;
}*/

void Camera::CalculateProjection(Vertex* vertice, Vector3<fixed_t> position)
{
    // Translation relative caméra
    fixed_t x = vertice->position.x + position.x - this->position.x;
    fixed_t y = vertice->position.y + position.y - this->position.y;
    fixed_t z = vertice->position.z + position.z - this->position.z;

    const Vector3<fixed_t> cos_camera = this->rotation.GetCos();
    const Vector3<fixed_t> sin_camera = this->rotation.GetSin();

    const fixed_t cx = cos_camera.x;
    const fixed_t sx = sin_camera.x;
    const fixed_t cy = cos_camera.y;
    const fixed_t sy = sin_camera.y;
    const fixed_t cz = cos_camera.z;
    const fixed_t sz = sin_camera.z;

    // Rotation X
    fixed_t tmp_y = (y * cx - z * sx);
    z = (y * sx + z * cx);
    y = tmp_y;

    // Rotation Y
    fixed_t tmp_x = (x * cy + z * sy);
    z = (-x * sy + z * cy);
    x = tmp_x;

    // Rotation Z
    tmp_x = (x * cz - y * sz);
    y = (x * sz + y * cz);
    x = tmp_x;

    // Projection
    if (z < fixed_t(1)) z = fixed_t(1); // évite division par 0 / clipping

    fixed_t inv_z = fov / z;

    vertice->projected.x = (x * inv_z) + offset.x + fixed_t(SCREEN_WIDTH / 2);
    vertice->projected.y = (-y * inv_z) + offset.y + fixed_t(SCREEN_HEIGHT / 2);
    vertice->projected.z = z;
}


static inline bool IsBackface(Vertex* vertices, Face* face)
{
    const Vector3<fixed_t>& n = face->normal;
    Vector3<fixed_t> sum = vertices[face->vertices[0]].position +
                           vertices[face->vertices[1]].position +
                           vertices[face->vertices[2]].position;
    if (face->length == 4)
        sum += vertices[face->vertices[3]].position;
    // Produit scalaire avec vecteur vue = (somme - camPos * n) · normal
    sum -= Camera::current->position * face->length;
    return n.dot(sum) > 0;
}

static inline bool IsVisible(RenderFace face)
{
    if (face.depth <= 0)
        return false;

    const fixed_t w = fixed_t(SCREEN_WIDTH);
    const fixed_t h = fixed_t(SCREEN_HEIGHT);

    Vertex* vertices = face.mesh->vertices;
    unsigned int* v = face.face->vertices;

    if (face.face->length == 4)
    {
        if ((vertices[v[0]].projected.x >= 0 && vertices[v[0]].projected.x < w &&
             vertices[v[0]].projected.y >= 0 && vertices[v[0]].projected.y < h) ||
            (vertices[v[1]].projected.x >= 0 && vertices[v[1]].projected.x < w &&
             vertices[v[1]].projected.y >= 0 && vertices[v[1]].projected.y < h) ||
            (vertices[v[2]].projected.x >= 0 && vertices[v[2]].projected.x < w &&
             vertices[v[2]].projected.y >= 0 && vertices[v[2]].projected.y < h) ||
            (vertices[v[3]].projected.x >= 0 && vertices[v[3]].projected.x < w &&
             vertices[v[3]].projected.y >= 0 && vertices[v[3]].projected.y < h))
            return !IsBackface(vertices, face.face);
    }
    else
    {
        if ((vertices[v[0]].projected.x >= 0 && vertices[v[0]].projected.x < w &&
             vertices[v[0]].projected.y >= 0 && vertices[v[0]].projected.y < h) ||
            (vertices[v[1]].projected.x >= 0 && vertices[v[1]].projected.x < w &&
             vertices[v[1]].projected.y >= 0 && vertices[v[1]].projected.y < h) ||
            (vertices[v[2]].projected.x >= 0 && vertices[v[2]].projected.x < w &&
             vertices[v[2]].projected.y >= 0 && vertices[v[2]].projected.y < h))
            return !IsBackface(vertices, face.face);
    }

    return !IsBackface(vertices, face.face);
}


void Camera::AddFace(RenderFace face)
{
    if (!IsVisible(face))
        return;
    if (Camera::current->face_count < MAX_FACES)
    {
        Camera::faces[Camera::current->face_count] = face;
        Camera::current->face_count++;
    }
    /*else
    {
        size_t max_depth = 0;
        for (size_t i = 0; i < MAX_FACES; i++)
        {
            if (Camera::faces[i].depth > Camera::faces[max_depth].depth)
                max_depth = i;
        }
        if (face.depth < Camera::faces[max_depth].depth)
            Camera::faces[max_depth] = face;
    }*/
}

void DrawWireframeQuad(RenderFace rface, int color)
{
    for (int i = 0; i < rface.face->length; i++)
    {
        DrawLine(
            (int)(rface.mesh->vertices[rface.face->vertices[i]].projected.x),
            (int)(rface.mesh->vertices[rface.face->vertices[i]].projected.y),
            (int)(rface.mesh->vertices[rface.face->vertices[(i + 1) % rface.face->length]].projected.x),
            (int)(rface.mesh->vertices[rface.face->vertices[(i + 1) % rface.face->length]].projected.y),
            color);
    }
}

void DrawQuad(RenderFace face)
{
    VertexLocal verts[4];
    const int texW_minus1 = 512-1;
    const int texH_minus1 = 512-1;

    for (int i = 0; i < 4; ++i) {
        verts[i].x = (int)(face.mesh->vertices[face.face->vertices[i]].projected.x);
        verts[i].y = (int)(face.mesh->vertices[face.face->vertices[i]].projected.y);
        verts[i].u = face.mesh->uvs[face.face->uvs[i]].x * texW_minus1;
        verts[i].v = face.mesh->uvs[face.face->uvs[i]].y * texH_minus1;
    }
    if (verts[0].y == verts[1].y && verts[2].y == verts[3].y)
    {
        if (verts[0].x == verts[3].x && verts[1].x == verts[2].x)
            DrawRectUV(verts[0], verts[1], verts[2], verts[3]);
            //DrawRect(verts[0], verts[1], verts[2], verts[3]);
        else
            DrawHorzParallelogramUV(verts[0], verts[1], verts[2], verts[3]);
            //DrawHorzParallelogram(verts[0], verts[1], verts[2], verts[3]);
    }
    else
    {
        if (verts[0].x == verts[3].x && verts[1].x == verts[2].x)
            DrawVertParallelogramUV(verts[0], verts[1], verts[2], verts[3]);
            //DrawVertParallelogram(verts[0], verts[1], verts[2], verts[3]);
        else
        {
            DrawTriangle(verts[0], verts[1], verts[2]);
            DrawTriangle(verts[0], verts[2], verts[3]);
        }
    }
}

void DrawTris(RenderFace face)
{
    VertexLocal verts[3];
    const int texW_minus1 = 512-1;
    const int texH_minus1 = 512-1;
    for (int i = 0; i < 3; ++i) {
        verts[i].x = (int)(face.mesh->vertices[face.face->vertices[i]].projected.x);
        verts[i].y = (int)(face.mesh->vertices[face.face->vertices[i]].projected.y);
        verts[i].u = face.mesh->uvs[face.face->uvs[i]].x * texW_minus1;
        verts[i].v = face.mesh->uvs[face.face->uvs[i]].y * texH_minus1;
    }
    DrawTriangle(verts[0], verts[1], verts[2]);
}

inline bool compare_faces(const RenderFace& a, const RenderFace& b)
{
    return a.depth > b.depth;
}

void Camera::SortFaces()
{
    std::sort(faces, faces + face_count, compare_faces);// tri des faces par profondeur du plus loin au plus proche
}

void Camera::Draw()
{
    SortFaces();
    for(size_t i = 0; i < face_count; i++)
    {
        if (faces[i].face->length == 4)
            DrawQuad(faces[i]);
        else
            DrawTris(faces[i]);
        //DrawWireframeQuad(faces[i], C_WHITE);
    }
    last_face_count = face_count;
    face_count = 0;
}

void Camera::PrintRenderFaceCount()
{
    char buffer[20];
    sprintf(buffer, "Faces: %d", last_face_count);
    dtext(0, 20, C_WHITE, buffer);
}
