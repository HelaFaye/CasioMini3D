#include "world.hpp"

Camera *Camera::current = nullptr;

Camera::Camera(Vector3<int> position, int angle)
{
    this->position = position;
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

static bool compareByZ(const RenderQuad& lhs, const RenderQuad& rhs) {
    return lhs.z > rhs.z; // Pour un tri décroissant
}

void Camera::SortQuads()
{
    std::sort(quads, quads + quad_count, compareByZ);
}

void DrawWireframeQuad(RenderQuad quad, int color)
{
    for (int i = 0; i < 4; i++)
    {
        dline(TO_INT(quad.points[i].x), TO_INT(quad.points[i].y), TO_INT(quad.points[(i + 1) % 4].x), TO_INT(quad.points[(i + 1) % 4].y), color);
    }
}

void sortPoints(fVector2 point[4])
{
    //sort in this order: top_left, top_right, bottom_right, bottom_left
    fVector2 temp;
    //sort by y
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4 - i - 1; j++)
        {
            if (point[j].y > point[j + 1].y)
            {
                temp = point[j];
                point[j] = point[j + 1];
                point[j + 1] = temp;
            }
        }
    }
    //sort by x
    if (point[0].x > point[1].x)
    {
        temp = point[0];
        point[0] = point[1];
        point[1] = temp;
    }
    if (point[2].x < point[3].x)
    {
        temp = point[2];
        point[2] = point[3];
        point[3] = temp;
    }

}

fixed_t slope(fVector2 p1, fVector2 p2)
{
    if (p2.x == p1.x)
        return INT_TO_FIXED(0);
    return fdiv(p2.y - p1.y, p2.x - p1.x);
}

fixed_t intersect_y(fVector2 p1, fVector2 p2, fixed_t y) {
    if (p1.y == p2.y) return p1.x;  // Cas où la ligne est horizontale
    fixed_t s = slope(p1, p2);
    return (s != 0) ? (p1.x + fdiv(y - p1.y, s)) : p1.x;
}

// Fonction pour dessiner un parallélogramme coloré
void DrawColoredQuad(RenderQuad quad, int color) {
    const fVector2 flimits = {INT_TO_FIXED(SCREEN_WIDTH), INT_TO_FIXED(SCREEN_HEIGHT)};
    const Vector2 limits = {SCREEN_WIDTH, SCREEN_HEIGHT};
    sortPoints(quad.points);

    const fVector2 uvmap[2] = //{quad.uv_start, quad.uv_end};
    {
        {INT_TO_FIXED(0), INT_TO_FIXED(0)},
        {INT_TO_FIXED(40), INT_TO_FIXED(40)}
    };

    const fVector2 top_left = quad.points[0];
    const fVector2 top_right = quad.points[1];
    const fVector2 bottom_right = quad.points[2];
    const fVector2 bottom_left = quad.points[3];

    const Vector2 itop_left = {TO_INT(top_left.x), TO_INT(top_left.y)};
    const Vector2 itop_right = {TO_INT(top_right.x), TO_INT(top_right.y)};
    const Vector2 ibottom_right = {TO_INT(bottom_right.x), TO_INT(bottom_right.y)};
    const Vector2 ibottom_left = {TO_INT(bottom_left.x), TO_INT(bottom_left.y)};

    /*fVector2 x_limits = {MIN(MIN(top_left.x, top_right.x), MIN(bottom_left.x, bottom_right.x)),
                        MAX(MAX(top_left.x, top_right.x), MAX(bottom_left.x, bottom_right.x))};
    fVector2 y_limits = {MIN(MIN(top_left.y, top_right.y), MIN(bottom_left.y, bottom_right.y)),
                        MAX(MAX(top_left.y, top_right.y), MAX(bottom_left.y, bottom_right.y))};
    if (x_limits.x >= limits.x || x_limits.y < 0)
        return;
    if (y_limits.x >= limits.y || y_limits.y < 0)
        return;
    const fVector2 x_limitsOrig = x_limits;
    const fVector2 y_limitsOrig = y_limits;
    x_limits.x = MAX(x_limits.x, 0);
    x_limits.y = MIN(x_limits.y, limits.x);
    y_limits.x = MAX(y_limits.x, 0);
    y_limits.y = MIN(y_limits.y, limits.y);*/
    Vector2 x_limits = {TO_INT(MIN(top_left.x, bottom_left.x)), TO_INT(MAX(top_right.x, bottom_right.x))};
    Vector2 y_limits = {TO_INT(MIN(top_left.y, top_right.y)), TO_INT(MAX(bottom_left.y, bottom_right.y))};
    if (x_limits.x >= limits.x || x_limits.y < 0)
        return;
    if (y_limits.x >= limits.y || y_limits.y < 0)
        return;
    const Vector2 x_limitsOrig = x_limits;
    const Vector2 y_limitsOrig = y_limits;
    x_limits.x = MAX(x_limits.x, 0);
    x_limits.y = MIN(x_limits.y, limits.x);
    y_limits.x = MAX(y_limits.x, 0);
    y_limits.y = MIN(y_limits.y, limits.y);
    if (ibottom_left.x == itop_left.x && ibottom_right.x == itop_right.x &&
        itop_right.y == itop_left.y && ibottom_right.y == ibottom_left.y)
    {
        fixed_t du = 0, dv = 0;
        if (top_left.x != top_right.x)
            du = fdiv(uvmap[1].x - uvmap[0].x, top_right.x - top_left.x);
        if (top_left.y != bottom_left.y)
            dv = fdiv(uvmap[1].y - uvmap[0].y, bottom_left.y - top_left.y);
        fixed_t u = uvmap[0].x + du*(x_limits.x - x_limitsOrig.x);
        fixed_t v = uvmap[0].y + dv*(y_limits.x - y_limitsOrig.x);
        for (int y = y_limits.x; y < y_limits.y; y++)
        {
            fixed_t u2 = u;
            for (int x = x_limits.x; x < x_limits.y; x++)
            {
                int u_int = TO_INT(u2)%40;
                int v_int = TO_INT(v)%40;
                int color = get_uv_map(u_int, v_int);
                DrawPixel(x, y, color);
                u2 += du;
            }
            v += dv;
        }
    }
    else if (itop_right.y == itop_left.y && ibottom_right.y == ibottom_left.y)
    {
        fixed_t dx_right = 0, dx_left = 0;
        if (bottom_left.y != top_left.y)
            dx_left = fdiv(bottom_left.x - top_left.x, bottom_left.y - top_left.y);
        if (bottom_right.y != top_right.y)
            dx_right = fdiv(bottom_right.x - top_right.x, bottom_right.y - top_right.y);
        fixed_t x_left = top_left.x + dx_left*(y_limits.x - y_limitsOrig.x);
        fixed_t x_right = top_right.x + dx_right*(y_limits.x - y_limitsOrig.x);
        fixed_t du = 0, dv = 0;
        if (top_left.x != top_right.x)
            du = fdiv(uvmap[1].x - uvmap[0].x, top_right.x - top_left.x);
        if (top_left.y != bottom_left.y)
            dv = fdiv(uvmap[1].y - uvmap[0].y, bottom_left.y - top_left.y);
        fixed_t u = uvmap[0].x;
        fixed_t v = uvmap[0].y + dv*(y_limits.x - y_limitsOrig.x);
        fixed_t du_end = 0;
        if (bottom_left.x != bottom_right.x)
            du_end = fdiv(uvmap[1].x - uvmap[0].x, bottom_right.x - bottom_left.x);
        fixed_t duy = 0;
        if (bottom_left.y != top_left.y)
            duy = fdiv(du_end - du, bottom_left.y - top_left.y);
        du += duy*(y_limits.x - y_limitsOrig.x);
        for (int y = y_limits.x; y < y_limits.y; y ++)
        {
            fixed_t x_left2 = MAX(x_left, 0);
            fixed_t x_right2 = MIN(x_right, flimits.x);
            //du = fdiv(uvmap[1].x - uvmap[0].x, x_right - x_left);
            fixed_t u2 = u + fmul(du,(x_left2 - x_left));
            for (fixed_t x = x_left2; x < x_right2; x+= FIXED_ONE)
            {
                int u_int = TO_INT(u2*4)%40;
                int v_int = TO_INT(v*4)%40;
                int color = get_uv_map(u_int, v_int);
                DrawPixel(TO_INT(x), y, color);
                u2 += du;
            }
            x_left += dx_left;
            x_right += dx_right;
            v += dv;
            du += duy;
        }
    }
    else //if (ibottom_left.x == itop_left.x && ibottom_right.x == itop_right.x)
    {
        fixed_t dy_top = 0, dy_bottom = 0;
        if (top_left.x != top_right.x)
            dy_top = fdiv(top_right.y - top_left.y, top_right.x - top_left.x);
        if (bottom_left.x != bottom_right.x)
            dy_bottom = fdiv(bottom_right.y - bottom_left.y, bottom_right.x - bottom_left.x);
        fixed_t y_top = top_left.y + dy_top*(x_limits.x - x_limitsOrig.x);
        fixed_t y_bottom = bottom_left.y + dy_bottom*(x_limits.x - x_limitsOrig.x);
        fixed_t du = 0, dv = 0;
        if (top_left.x != top_right.x)
            du = fdiv(uvmap[1].x - uvmap[0].x, top_right.x - top_left.x);
        if (top_left.y != bottom_left.y)
            dv = fdiv(uvmap[1].y - uvmap[0].y, bottom_left.y - top_left.y);
        fixed_t u = uvmap[0].x + du*(x_limits.x - x_limitsOrig.x);
        fixed_t v = uvmap[0].y;
        fixed_t dv_end = 0;
        if (top_right.y != bottom_right.y)
            dv_end = fdiv(uvmap[1].y - uvmap[0].y, bottom_right.y - top_right.y);
        fixed_t dvx = 0;
        if (top_left.x != top_right.x)
            dvx = fdiv(dv_end - dv, top_right.x - top_left.x);
        dv += dvx*(x_limits.x - x_limitsOrig.x);
        for (int x = x_limits.x; x < x_limits.y; x++)
        {
            fixed_t y_top2 = MAX(y_top, 0);
            fixed_t y_bottom2 = MIN(y_bottom, flimits.y);
            fixed_t v2 = v + fmul(dv,(y_top2 - y_top));
            for (fixed_t y = y_top2; y < y_bottom2; y += FIXED_ONE)
            {
                int u_int = TO_INT(u)%40;
                int v_int = TO_INT(v2)%40;
                int color = get_uv_map(u_int, v_int);
                DrawPixel(x, TO_INT(y), color);
                v2 += dv;
            }
            y_top += dy_top;
            y_bottom += dy_bottom;
            u += du;
            dv += dvx;
        }
    }
    /*else
    {
        for (fixed_t y = y_limits.x; y < y_limits.y; y += FIXED_ONE)
        {
            fixed_t left_x = INT_MAX;
            fixed_t right_x = INT_MIN;

            // Trouver les intersections avec tous les côtés
            for (int i = 0; i < 4; i++)
            {
                fVector2 p1 = quad.points[i];
                fVector2 p2 = quad.points[(i + 1) % 4];

                if ((y >= p1.y && y <= p2.y) || (y >= p2.y && y <= p1.y))
                {
                    fixed_t x = intersect_y(p1, p2, y);
                    if (x < left_x) left_x = x;
                    if (x > right_x) right_x = x;
                }
            }

            // Dessiner les pixels de gauche à droite
            left_x = MAX(left_x, 0);
            right_x = MIN(right_x, limits.x);

            for (fixed_t x = left_x; x < right_x; x += FIXED_ONE)
            {
                DrawPixel(TO_INT(x), TO_INT(y), color);
            }
        }
    }*/
}


void Camera::Render()
{
    SortQuads();
    for(size_t i = 0; i < quad_count; i++)
    {
        //DrawWireframeQuad(quads[i], C_WHITE);
        DrawColoredQuad(quads[i], C_WHITE);
        DrawWireframeQuad(quads[i], C_RED);
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