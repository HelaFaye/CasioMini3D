#include "world.hpp"
#include <algorithm>
#include <tuple>

Camera *Camera::current = nullptr;

Camera::Camera(Vector3<int> position, int angle)
{
    this->position = position;
    this->offset = {0, 0};
    this->angle = angle;
    Camera::current = this;
    quad_count = 0;
}

void Camera::UpdateCoordinates()
{
    fposition = {INT_TO_FIXED(position.x), INT_TO_FIXED(position.y), INT_TO_FIXED(position.z)};
    angle %= 360;
    fixed_t temp = INT_TO_FIXED(angle)/180;
    fangle = {fcos_approx(temp), fsin_approx(temp)};
}


inline static void ApplyRotationX(fixed_t *y, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_y = fmul(*y, cos_theta) - fmul(*z, sin_theta);
    const fixed_t new_z = fmul(*y, sin_theta) + fmul(*z, cos_theta);
    *y = new_y;
    *z = new_z;
}

void Camera::CalculateProjection(Vertex* vertice, fVector3 offset)
{
    const fixed_t half_screen_width = INT_TO_FIXED(SCREEN_WIDTH / 2);
    const fixed_t half_screen_height = INT_TO_FIXED(SCREEN_HEIGHT / 2);
    const fixed_t fov = INT_TO_FIXED(300);

    fVector3 m = {
            INT_TO_FIXED(vertice->position.x),
            INT_TO_FIXED(vertice->position.y),
            INT_TO_FIXED(vertice->position.z)
        };
    /* Application de la translation (incluant l'inverse de la translation de la caméra) */
    m.x += offset.x - this->fposition.x;
    m.y += offset.y - this->fposition.y;
    m.z += offset.z - this->fposition.z;
    /* Application de la rotation de la caméra */
    ApplyRotationX(&m.y, &m.z, fangle.x, fangle.y);
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


void Camera::AddQuad(Vertex *points[4])
{
    if (quad_count >= MAX_QUADS)
        return;
    fixed_t z = (points[0]->projected.z + points[1]->projected.z + points[2]->projected.z + points[3]->projected.z) / 4;
    if (z <= FIXED_ONE*BLOCK_SIZE*2)
        return;
    quads[quad_count].points[0] = (fVector2){points[0]->projected.x, points[0]->projected.y};
    quads[quad_count].points[1] = (fVector2){points[1]->projected.x, points[1]->projected.y};
    quads[quad_count].points[2] = (fVector2){points[2]->projected.x, points[2]->projected.y};
    quads[quad_count].points[3] = (fVector2){points[3]->projected.x, points[3]->projected.y};
    quads[quad_count].z = z;
    quad_count++;
}

void Camera::SortQuads()
{
    std::sort(quads, quads + quad_count, [](const RenderQuad& lhs, const RenderQuad& rhs) {
        return lhs.z > rhs.z; // Pour un tri décroissant
    }); 
}

void DrawWireframeQuad(RenderQuad quad, int color)
{
    for (int i = 0; i < 4; i++)
    {
        dline(TO_INT(quad.points[i].x), TO_INT(quad.points[i].y), TO_INT(quad.points[(i + 1) % 4].x), TO_INT(quad.points[(i + 1) % 4].y), color);
    }
}

inline int Clamp(int x, int min, int max) {
    return (x < min) ? min : ((x > max) ? max : x);
}

void DebugText(const char *text)
{
    drect(0, 30, 120, 50, C_BLACK);
    dtext(0, 30, C_WHITE, text);
    dupdate();
    while (!keydown(KEY_MENU))
        clearevents();
    while (keydown(KEY_MENU))
        clearevents();
}

struct VertexLocal { int x, y; fixed_t u, v; };
struct Edge { int x, y; fixed_t u, v; };

inline Edge CalcEdge(VertexLocal a, VertexLocal b, int y)
{
    const int deltaY = b.y - a.y;
    if (deltaY == 0) return {a.x, y, a.u, a.v};
    const int factor = ((y - a.y) << PRECISION) / deltaY;
    const int x = a.x + TO_INT((b.x - a.x) * factor);
    const fixed_t u = a.u + ((b.u - a.u) * factor >> PRECISION);
    const fixed_t v = a.v + ((b.v - a.v) * factor >> PRECISION);
    return Edge{x, y, u, v};
};

inline void ProcessScanline(int y, int Ax, int Bx, fixed_t Au, fixed_t Av, fixed_t Bu, fixed_t Bv) {
    if (Bx - Ax == 0) return;
    if (Ax > Bx) {
        std::swap(Ax, Bx);
        std::swap(Au, Bu);
        std::swap(Av, Bv);
    }

    const int startX = std::max(Ax, 0);
    const int endX = std::min(Bx, SCREEN_WIDTH - 1);
    const int len = Bx - Ax;

    const int du = (Bu - Au) / len;
    const int dv = (Bv - Av) / len;

    fixed_t u = Au + du * (startX - Ax);
    fixed_t v = Av + dv * (startX - Ax);

    int pixelColor = C_WHITE;
    for (int x = startX; x <= endX; ++x) {
        pixelColor = get_uv_map(TO_INT(u), TO_INT(v));
        DrawPixel(x, y, pixelColor);
        u += du;
        v += dv;
    }
};

void DrawTriangle(VertexLocal v0, VertexLocal v1, VertexLocal v2) {
    if (v1.y < v0.y) std::swap(v0, v1);
    if (v2.y < v0.y) std::swap(v0, v2);
    if (v2.y < v1.y) std::swap(v1, v2);

    if (v2.y - v0.y == 0) return;

    int startY = std::max(v0.y, 0);
    int endY = std::min(v2.y, SCREEN_HEIGHT - 1);

    for (int y = startY; y <= endY; ++y) {
        const bool second_half = y > v1.y || v1.y == v0.y;
        Edge A = CalcEdge(v0, v2, y);
        Edge B = second_half ? CalcEdge(v1, v2, y) : CalcEdge(v0, v1, y);
        ProcessScanline(y, A.x, B.x, A.u, A.v, B.u, B.v);
    }
};

void DrawRectOptimized(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    const int startY = std::max(tl.y, 0);
    const int endY = std::min(br.y, SCREEN_HEIGHT - 1);
    const int startX = std::max(tl.x, 0);
    const int endX = std::min(br.x, SCREEN_WIDTH - 1);

    const int height = br.y - tl.y;
    const int width = br.x - tl.x;

    if (width <= 0 || height <= 0) return;

    const fixed_t inv_height = FIXED_ONE / height;
    const fixed_t du_st = fmul((bl.u - tl.u), inv_height);
    const fixed_t dv_st = fmul((bl.v - tl.v), inv_height);
    const fixed_t du_end = fmul((br.u - tr.u), inv_height);
    const fixed_t dv_end = fmul((br.v - tr.v), inv_height);

    const int deltaStartY = startY - tl.y;
    fixed_t u_st = tl.u + du_st * deltaStartY;
    fixed_t v_st = tl.v + dv_st * deltaStartY;
    fixed_t u_end = tr.u + du_end * deltaStartY;
    fixed_t v_end = tr.v + dv_end * deltaStartY;

    const fixed_t inv_length = FIXED_ONE / (br.x - tl.x);

    for (int y = startY; y <= endY; ++y) {
        const fixed_t du = fmul(u_end - u_st, inv_length);
        const fixed_t dv = fmul(v_end - v_st, inv_length);

        fixed_t u = u_st + du * (startX - tl.x);
        fixed_t v = v_st + dv * (startX - tl.x);

        for (int x = startX; x <= endX; ++x) {
            DrawPixel(x, y, get_uv_map(TO_INT(u), TO_INT(v)));
            u += du;
            v += dv;
        }

        u_st += du_st;
        v_st += dv_st;
        u_end += du_end;
        v_end += dv_end;
    }
}


inline bool isAxisAligned(VertexLocal *v) {
    return (v[0].y == v[1].y && v[2].y == v[3].y && v[0].x == v[3].x && v[1].x == v[2].x);
}

void DrawColoredQuad(RenderQuad quad, int color)
{
    VertexLocal verts[4];
    constexpr fixed_t texW_minus1 = FLOAT_TO_FIXED(39);
    constexpr fixed_t texH_minus1 = FLOAT_TO_FIXED(39);

    constexpr fixed_t uv_fixed[4][2] = {
        {0, 0},
        {texW_minus1, 0},
        {texW_minus1, texH_minus1},
        {0, texH_minus1}
    };

    for (int i = 0; i < 4; ++i) {
        verts[i].x = TO_INT(quad.points[i].x);
        verts[i].y = TO_INT(quad.points[i].y);
        verts[i].u = uv_fixed[i][0];
        verts[i].v = uv_fixed[i][1];
    }

    if (isAxisAligned(verts)) {
        DrawRectOptimized(verts[0], verts[1], verts[2], verts[3]);
        return;
    }

    DrawTriangle(verts[0], verts[1], verts[2]);
    DrawTriangle(verts[0], verts[2], verts[3]);
}


void Camera::Render()
{
    SortQuads();
    for(size_t i = 0; i < quad_count; i++)
    {
        //DrawWireframeQuad(quads[i], C_WHITE);
        DrawColoredQuad(quads[i], C_WHITE);
        //DrawWireframeQuad(quads[i], C_RED);
    }
    quad_count = 0;
}


Cube::Cube()
{
    uv_start.x = 0;
    uv_start.y = 0;
    uv_end.x = FIXED_ONE * 16;
    uv_end.y = FIXED_ONE * 16;
    const int half_size = BLOCK_SIZE / 2;
    vertices[0].position = Vector3<int>(-half_size, half_size, half_size);
    vertices[1].position = Vector3<int>(half_size, half_size, half_size);
    vertices[2].position = Vector3<int>(half_size, -half_size, half_size);
    vertices[3].position = Vector3<int>(-half_size, -half_size, half_size);
    vertices[4].position = Vector3<int>(-half_size, half_size, -half_size);
    vertices[5].position = Vector3<int>(half_size, half_size, -half_size);
    vertices[6].position = Vector3<int>(half_size, -half_size, -half_size);
    vertices[7].position = Vector3<int>(-half_size, -half_size, -half_size);
}

void Cube::CalculateProjection(fVector3 offset)
{
    for(int i = 0; i < 8; i++)
    {
        Camera::current->CalculateProjection(&vertices[i], offset);
    }
}

void Cube::Render()
{
    //Camera::current->AddQuad((Vertex*[4]){&vertices[0], &vertices[1], &vertices[2], &vertices[3]});
    Camera::current->AddQuad((Vertex*[4]){&vertices[4], &vertices[5], &vertices[6], &vertices[7]});
    if (vertices[0].projected.y < vertices[4].projected.y)
        Camera::current->AddQuad((Vertex*[4]){&vertices[0], &vertices[1], &vertices[5], &vertices[4]});
    if (vertices[5].projected.x < vertices[1].projected.x)
        Camera::current->AddQuad((Vertex*[4]){&vertices[5], &vertices[1], &vertices[2], &vertices[6]});
    if (vertices[6].projected.y < vertices[2].projected.y)
        Camera::current->AddQuad((Vertex*[4]){&vertices[3], &vertices[2], &vertices[6], &vertices[7]});
    if (vertices[3].projected.x < vertices[7].projected.x)
        Camera::current->AddQuad((Vertex*[4]){&vertices[0], &vertices[4], &vertices[7], &vertices[3]});
}

World::World(int width, int height, int depth, Block *blocks[])
{
    this->width = width;
    this->height = height;
    this->depth = depth;
    this->blocks = blocks;
    this->offset = Vector3<int>(0, 0, 0);
    ids = new uint8_t[width * height * depth];
    memset(ids, 0, width * height * depth);
}

World::~World()
{
    delete[] ids;
}

Block *World::get_block(int x, int y, int z)
{
    if(x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth)
        return nullptr;
    return blocks[ids[x + y * width + z * width * height]];
}

Block *World::get_block_unsafe(int x, int y, int z)
{
    return blocks[ids[x + y * width + z * width * height]];
}

void World::set_block(int x, int y, int z, Block *block)
{
    if(x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth)
        return;
    ids[x + y * width + z * width * height] = block == nullptr ? 0 : 1;
    blocks[ids[x + y * width + z * width * height]] = block;
}

void World::set_block_unsafe(int x, int y, int z, Block *block)
{
    ids[x + y * width + z * width * height] = block == nullptr ? 0 : 1;
    blocks[ids[x + y * width + z * width * height]] = block;
}

int World::get_id_unsafe(int x, int y, int z)
{
    return ids[x + y * width + z * width * height];
}

void World::set_id_unsafe(int x, int y, int z, int id)
{
    ids[x + y * width + z * width * height] = id;
}

bool World::DetectInRange(fVector3 position)
{
    Vertex vertice;
    Camera::current->CalculateProjection(&vertice, position);
    if (vertice.projected.z <= FIXED_ONE*BLOCK_SIZE*2)
        return false;
    const fVector2 screen = {INT_TO_FIXED(SCREEN_WIDTH), INT_TO_FIXED(SCREEN_HEIGHT)};
    const fixed_t extra = FIXED_ONE*BLOCK_SIZE*2;
    if (vertice.projected.x > screen.x + extra || vertice.projected.x < -extra)
        return false;
    if (vertice.projected.y > screen.y + extra || vertice.projected.y < -extra)
        return false;
    return true;
}

void World::Render()
{
    Camera::current->UpdateCoordinates();
    for(int x = 0; x < width; x++)
    {
        for(int y = 0; y < height; y++)
        {
            for(int z = 0; z < depth; z++)
            {
                Block *block = get_block_unsafe(x, y, z);
                if(block != nullptr)
                {
                    Vector3<int> pos = this->offset + Vector3<int>(x * BLOCK_SIZE, y * BLOCK_SIZE, z * BLOCK_SIZE);
                    fVector3 offset = {INT_TO_FIXED(pos.x), INT_TO_FIXED(pos.y), INT_TO_FIXED(pos.z)};
                    if (DetectInRange(offset))
                    {
                        block->CalculateProjection(offset);
                        block->Render();
                    }
                }
            }
        }
    }
}