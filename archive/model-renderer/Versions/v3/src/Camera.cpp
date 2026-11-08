#include "Camera.hpp"
#include "shapes/shapes.hpp"

RenderFace Camera::faces[MAX_FACES];
Camera* Camera::current = nullptr;

Camera::Camera()
{
    if (Camera::current == nullptr)
        Camera::current = this;
    this->position = Vector3<int>{0, 0, 0};
    this->offset = Vector2<int>{0, 0};
    this->rotation = Quaternion();
    this->UpdateCoordinates();
}

Camera::~Camera()
{
    if (Camera::current == this)
        Camera::current = nullptr;
}

void Camera::UpdateCoordinates()
{
    this->fposition = {INT_TO_FIXED(this->position.x), INT_TO_FIXED(this->position.y), INT_TO_FIXED(this->position.z)};
}


inline static void ApplyRotationX(fixed_t *y, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_y = fmul(*y, cos_theta) - fmul(*z, sin_theta);
    const fixed_t new_z = fmul(*y, sin_theta) + fmul(*z, cos_theta);
    *y = new_y;
    *z = new_z;
}

inline static void ApplyRotationY(fixed_t *x, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = fmul(*x, cos_theta) + fmul(*z, sin_theta);
    const fixed_t new_z = fmul(-*x, sin_theta) + fmul(*z, cos_theta);
    *x = new_x;
    *z = new_z;
}

inline static void ApplyRotationZ(fixed_t *x, fixed_t *y, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = fmul(*x, cos_theta) - fmul(*y, sin_theta);
    const fixed_t new_y = fmul(*x, sin_theta) + fmul(*y, cos_theta);
    *x = new_x;
    *y = new_y;
}

void Camera::CalculateProjection(Vertex* vertice, Vector3<fixed_t> offset)
{
    const fixed_t half_screen_width = INT_TO_FIXED(SCREEN_WIDTH / 2);
    const fixed_t half_screen_height = INT_TO_FIXED(SCREEN_HEIGHT / 2);
    const fixed_t fov = INT_TO_FIXED(300);

    Vector3<fixed_t> m = {
            INT_TO_FIXED(vertice->position.x),
            INT_TO_FIXED(vertice->position.y),
            INT_TO_FIXED(vertice->position.z)
        };
    /* Application de la translation (incluant l'inverse de la translation de la caméra) */
    m.x += offset.x - this->fposition.x;
    m.y += offset.y - this->fposition.y;
    m.z += offset.z - this->fposition.z;
    /* Application de la rotation de la caméra */
    const Vector3<fixed_t> cos_camera = this->rotation.GetCos();
    const Vector3<fixed_t> sin_camera = this->rotation.GetSin();
    ApplyRotationX(&m.y, &m.z, cos_camera.x, sin_camera.x);
    ApplyRotationY(&m.x, &m.z, cos_camera.y, sin_camera.y);
    ApplyRotationZ(&m.x, &m.y, cos_camera.z, sin_camera.z);
    /* Projection */
    const fixed_t m_z = (m.z < FIXED_ONE) ? FIXED_ONE : m.z;
    const fixed_t f = fdiv(fov, m_z);
    /* Calcul final de la position projetée */
    vertice->projected.x = fmul(m.x, f) + half_screen_width;
    vertice->projected.y = fmul(-m.y, f) + half_screen_height;
    vertice->projected.z = m.z;

    vertice->projected.x += FIXED_ONE*this->offset.x;
    vertice->projected.y += FIXED_ONE*this->offset.y;
}

void Camera::AddFace(RenderFace face)
{
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
            TO_INT(rface.mesh->vertices[rface.face->vertices[i]].projected.x),
            TO_INT(rface.mesh->vertices[rface.face->vertices[i]].projected.y),
            TO_INT(rface.mesh->vertices[rface.face->vertices[(i + 1) % rface.face->length]].projected.x),
            TO_INT(rface.mesh->vertices[rface.face->vertices[(i + 1) % rface.face->length]].projected.y),
            color);
    }
}

void DrawQuad(RenderFace face)
{
    VertexLocal verts[4];
    const int texW_minus1 = 1024-1;
    const int texH_minus1 = 1024-1;

    for (int i = 0; i < 4; ++i) {
        verts[i].x = TO_INT(face.mesh->vertices[face.face->vertices[i]].projected.x);
        verts[i].y = TO_INT(face.mesh->vertices[face.face->vertices[i]].projected.y);
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
    const int texW_minus1 = 1024-1;
    const int texH_minus1 = 1024-1;
    for (int i = 0; i < 3; ++i) {
        verts[i].x = TO_INT(face.mesh->vertices[face.face->vertices[i]].projected.x);
        verts[i].y = TO_INT(face.mesh->vertices[face.face->vertices[i]].projected.y);
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
    dtext(0, 25, C_WHITE, buffer);
}
