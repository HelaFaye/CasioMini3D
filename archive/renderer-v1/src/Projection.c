#include "Projection.h"
#include <math.h>

extern bopti_image_t IMG_ASSET_blocks;

int uv_map[40*40];

void set_uv_map(int x, int y, int color)
{
    uv_map[x + y * 40] = color;
}

int get_uv_map(int x, int y)
{
    return uv_map[x + y * 40];
}

int GetPixel_CG_P8(bopti_image_t* img, int x, int y){
    void *data = img->data + y * img->stride;
    uint8_t *data_u8 = data;
    return (int8_t)data_u8[x];
}

int DecodePixel_CG_P8(bopti_image_t* img, int pixel){
    return img->palette[pixel+128];
}

int get_uv_map_img(int x, int y)
{
    return DecodePixel_CG_P8(&IMG_ASSET_blocks, GetPixel_CG_P8(&IMG_ASSET_blocks, x, y));
}

void init_uv_map()
{
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            float u = i / 40.0;
            float v = j / 40.0;
            set_uv_map(i, j, rgb((int)(u * 255), (int)(v * 255), 0));
        }
    }
}



inline float sin_approx(float i)
{
    const float B = 4 / M_PI;
    const float C = -4 / (M_PI * M_PI);

    return B * i + C * i * fabs(i);
}

inline float cos_approx(float i)
{
    return sin_approx(i + M_PI_2);
}

/* Approximation de sin en virgule fixe */
inline fixed_t fsin_approx(fixed_t i)
{
    const fixed_t B = fix(4 / M_PI);
    const fixed_t C = fix(-4 / (M_PI * M_PI));

    return fmul(B, i) + fmul(C, fmul(i, abs(i)));
}

/* Approximation de cos en virgule fixe */
inline fixed_t fcos_approx(fixed_t i)
{
    return fsin_approx(i + fix(M_PI_2));
}

/* Appliquer la rotation autour de l'axe X */
inline static void ApplyRotationX(fixed_t *y, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_y = fmul(*y, cos_theta) - fmul(*z, sin_theta);
    const fixed_t new_z = fmul(*y, sin_theta) + fmul(*z, cos_theta);
    *y = new_y;
    *z = new_z;
}

/* Appliquer la rotation autour de l'axe Y */
inline static void ApplyRotationY(fixed_t *x, fixed_t *z, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = fmul(*x, cos_theta) + fmul(*z, sin_theta);
    const fixed_t new_z = fmul(-*x, sin_theta) + fmul(*z, cos_theta);
    *x = new_x;
    *z = new_z;
}

/* Appliquer la rotation autour de l'axe Z */
inline static void ApplyRotationZ(fixed_t *x, fixed_t *y, fixed_t cos_theta, fixed_t sin_theta)
{
    const fixed_t new_x = fmul(*x, cos_theta) - fmul(*y, sin_theta);
    const fixed_t new_y = fmul(*x, sin_theta) + fmul(*y, cos_theta);
    *x = new_x;
    *y = new_y;
}

void CalculateProjection(Camera *camera, Projection* obj)
{
    const fixed_t half_screen_width = INT_TO_FIXED(SCREEN_WIDTH / 2);
    const fixed_t half_screen_height = INT_TO_FIXED(SCREEN_HEIGHT / 2);

    /* Pré-calcul des cos et sin pour la rotation de l'obj */
    const fVector3 cos_transform = {
        fcos_approx(FLOAT_TO_FIXED(obj->transform.rotation.x)),
        fcos_approx(FLOAT_TO_FIXED(obj->transform.rotation.y)),
        fcos_approx(FLOAT_TO_FIXED(obj->transform.rotation.z))
    };

    const fVector3 sin_transform = {
        fsin_approx(FLOAT_TO_FIXED(obj->transform.rotation.x)),
        fsin_approx(FLOAT_TO_FIXED(obj->transform.rotation.y)),
        fsin_approx(FLOAT_TO_FIXED(obj->transform.rotation.z))
    };

    const fVector3 scale = {
        FLOAT_TO_FIXED(obj->transform.scale.x),
        FLOAT_TO_FIXED(obj->transform.scale.y),
        FLOAT_TO_FIXED(obj->transform.scale.z)
    };

    const fVector3 position = {
        INT_TO_FIXED(obj->transform.position.x),
        INT_TO_FIXED(obj->transform.position.y),
        INT_TO_FIXED(obj->transform.position.z)
    };

    /* Pré-calcul des cos et sin pour la rotation de la caméra */
    const fVector3 cos_camera = {
        fcos_approx(FLOAT_TO_FIXED(camera->transform.rotation.x)),
        fcos_approx(FLOAT_TO_FIXED(camera->transform.rotation.y)),
        fcos_approx(FLOAT_TO_FIXED(camera->transform.rotation.z))
    };

    const fVector3 sin_camera = {
        fsin_approx(FLOAT_TO_FIXED(camera->transform.rotation.x)),
        fsin_approx(FLOAT_TO_FIXED(camera->transform.rotation.y)),
        fsin_approx(FLOAT_TO_FIXED(camera->transform.rotation.z))
    };

    const fVector3 camera_position = {
        INT_TO_FIXED(camera->transform.position.x),
        INT_TO_FIXED(camera->transform.position.y),
        INT_TO_FIXED(camera->transform.position.z)
    };

    const fixed_t min_z = INT_TO_FIXED(1);

    for (size_t i = 0; i < obj->mesh->verticesCount; i++)
    {
        /* Accès direct à la position du sommet, en évitant de répéter les conversions */
        const fVector3 vertex_pos = {
            INT_TO_FIXED(obj->mesh->vertices[i].position.x),
            INT_TO_FIXED(obj->mesh->vertices[i].position.y),
            INT_TO_FIXED(obj->mesh->vertices[i].position.z)
        };

        /* Application de l'échelle */
        fVector3 m = {
            fmul(vertex_pos.x, scale.x),
            fmul(vertex_pos.y, scale.y),
            fmul(vertex_pos.z, scale.z)
        };

        // Apply local rotation
        ApplyRotationX(&m.y, &m.z, cos_transform.x, sin_transform.x);
        ApplyRotationY(&m.x, &m.z, cos_transform.y, sin_transform.y);
        ApplyRotationZ(&m.x, &m.y, cos_transform.z, sin_transform.z);

        /* Application de la translation (incluant l'inverse de la translation de la caméra) */
        m.x += position.x - camera_position.x;
        m.y += position.y - camera_position.y;
        m.z += position.z - camera_position.z;

        /* Application de la rotation de la caméra */
        ApplyRotationX(&m.y, &m.z, cos_camera.x, sin_camera.x);
        ApplyRotationY(&m.x, &m.z, cos_camera.y, sin_camera.y);
        ApplyRotationZ(&m.x, &m.y, cos_camera.z, sin_camera.z);

        /* Projection */
        const fixed_t m_z = (m.z < min_z) ? min_z : m.z;
        const fixed_t f = fdiv(INT_TO_FIXED(300), m_z);

        /* Calcul final de la position projetée */
        obj->mesh->vertices[i].projected.x = fmul(m.x, f) + half_screen_width;
        obj->mesh->vertices[i].projected.y = fmul(-m.y, f) + half_screen_height;
        obj->mesh->vertices[i].projected.z = m.z;
    }
}





void DrawPixel(int x, int y, int color)
{
    gint_vram[DWIDTH * y + x] = color;
}

fixed_t slope(fVector2 p1, fVector2 p2)
{
    if (p2.x == p1.x)
        return INT_TO_FIXED(0);
    return fdiv(p2.y - p1.y, p2.x - p1.x);
}

void DrawFilledQuadColor(fVector2 points[4], int color)
{
    fVector2 top_right = points[0];
    fVector2 top_left = points[1];
    fVector2 bottom_left = points[2];
    fVector2 bottom_right = points[3];

    if (top_left.y > bottom_left.y)
    {
        top_left = points[2];
        bottom_left = points[3];
        bottom_right = points[0];
        top_right = points[1];
    }

    const fixed_t slope_top = slope(top_left, top_right);
    const fixed_t slope_left = slope(top_left, bottom_left);
    const fixed_t slope_right = slope(top_right, bottom_right);
    const fixed_t slope_bottom = slope(bottom_left, bottom_right);

    const int y_start = max(min(TO_INT(top_left.y), TO_INT(top_right.y)), 0);
    const int y_end = min(max(TO_INT(bottom_left.y), TO_INT(bottom_right.y)), SCREEN_HEIGHT - 1);

    fixed_t x_start, x_end;
    for (int y = y_start; y <= y_end; y++)
    {
        fixed_t fy = INT_TO_FIXED(y);

        // Determine x_start based on the current Y position
        if (fy < top_left.y)
        {
            x_start = top_left.x;
            if (slope_top)
                x_start += fdiv(fy - top_left.y, slope_top);
        }
        else if (fy <= bottom_left.y)
        {
            x_start = top_left.x;
            if (slope_left)
                x_start += fdiv(fy - top_left.y, slope_left);
        }
        else
        {
            x_start = bottom_left.x;
            if (slope_bottom)
                x_start += fdiv(fy - bottom_left.y, slope_bottom);
        }

        // Determine x_end based on the current Y position
        if (fy < top_right.y)
        {
            x_end = top_right.x;
            if (slope_top)
                x_end += fdiv(fy - top_right.y, slope_top);
        }
        else if (fy <= bottom_right.y)
        {
            x_end = top_right.x;
            if (slope_right)
                x_end += fdiv(fy - top_right.y, slope_right);
        }
        else
        {
            x_end = bottom_right.x;
            if (slope_bottom)
                x_end += fdiv(fy - bottom_right.y, slope_bottom);
        }

        // Swap if necessary
        if (x_start > x_end)
            swap(&x_start, &x_end);

        // Draw the line using integer values
        int x_start_int = TO_INT(x_start);
        int x_end_int = TO_INT(x_end);
        if (y < 0 || y >= SCREEN_HEIGHT)
            continue;
        x_start_int = max(min(x_start_int, SCREEN_WIDTH - 1), 0);
        x_end_int = max(min(x_end_int, SCREEN_WIDTH - 1), 0);
        /*for (int x = x_start_int; x < x_end_int; x++)
        {
            DrawPixel(x, y, color);
        }*/
        gint_dhline(x_start_int, x_end_int, y, color);
    }
}


inline static void multiplyMatrixVector(fixed_t matrix[3][3], fVector2 vec, fVector2* result) {
    const fixed_t w = fmul(matrix[2][0] , vec.x) + fmul(matrix[2][1] , vec.y) + matrix[2][2];
    if (w == 0) {
        result->x = 0;
        result->y = 0;
        return;
    }
    result->x = fdiv((fmul(matrix[0][0] , vec.x) + fmul(matrix[0][1] , vec.y) + matrix[0][2]) , w);
    result->y = fdiv((fmul(matrix[1][0] , vec.x) + fmul(matrix[1][1] , vec.y) + matrix[1][2]) , w);
}

// Fonction utilitaire pour échanger les lignes dans A et B
void swap_rows(fixed_t A[8][8], fixed_t B[8], int row1, int row2) {
    if (row1 == row2) return; // Éviter les échanges inutiles
    for (int j = 0; j < 8; ++j) {
        fixed_t temp = A[row1][j];
        A[row1][j] = A[row2][j];
        A[row2][j] = temp;
    }
    fixed_t temp = B[row1];
    B[row1] = B[row2];
    B[row2] = temp;
}

void gauss_jordan(fixed_t A[8][8], fixed_t B[8]) {
    for (int i = 0; i < 8; ++i) {
        // Trouver le pivot non nul
        int max_row = i;
        for (int k = i + 1; k < 8; ++k) {
            if (fixed_abs(A[k][i]) > fixed_abs(A[max_row][i])) {
                max_row = k;
            }
        }

        // Échanger les lignes si nécessaire
        if (i != max_row) {
            swap_rows(A, B, i, max_row);
        }

        // Normaliser la ligne du pivot
        const fixed_t pivot = A[i][i];
        if (pivot != 0) {
            // Normaliser la ligne du pivot en une seule passe
            const fixed_t inv_pivot = fdiv(FIXED_ONE, pivot);
            for (int j = i; j < 8; ++j) {
                A[i][j] = fmul(A[i][j], inv_pivot);
            }
            B[i] = fmul(B[i], inv_pivot);
        }
        

        // Élimination des autres lignes
        for (int k = 0; k < 8; ++k) {
            if (k != i) {
                fixed_t factor = A[k][i];
                if (factor != 0) { // Éviter les opérations inutiles
                    for (int j = i; j < 8; ++j) {
                        A[k][j] = A[k][j] - fmul(factor, A[i][j]);
                    }
                    B[k] = B[k]- fmul(factor, B[i]);
                }
            }
        }
    }
}


inline static void computePerspectiveMatrix(fVector2 src[4], fVector2 dest[4], fixed_t matrix[3][3]) {
    fixed_t A[8][8] = {
        {src[0].x, src[0].y, FIXED_ONE, 0, 0, 0, -fmul(src[0].x, dest[0].x), -fmul(src[0].y, dest[0].x)},
        {0, 0, 0, src[0].x, src[0].y, FIXED_ONE, -fmul(src[0].x, dest[0].y), -fmul(src[0].y, dest[0].y)},
        {src[1].x, src[1].y, FIXED_ONE, 0, 0, 0, -fmul(src[1].x, dest[1].x), -fmul(src[1].y, dest[1].x)},
        {0, 0, 0, src[1].x, src[1].y, FIXED_ONE, -fmul(src[1].x, dest[1].y), -fmul(src[1].y, dest[1].y)},
        {src[2].x, src[2].y, FIXED_ONE, 0, 0, 0, -fmul(src[2].x, dest[2].x), -fmul(src[2].y, dest[2].x)},
        {0, 0, 0, src[2].x, src[2].y, FIXED_ONE, -fmul(src[2].x, dest[2].y), -fmul(src[2].y, dest[2].y)},
        {src[3].x, src[3].y, FIXED_ONE, 0, 0, 0, -fmul(src[3].x, dest[3].x), -fmul(src[3].y, dest[3].x)},
        {0, 0, 0, src[3].x, src[3].y, FIXED_ONE, -fmul(src[3].x, dest[3].y), -fmul(src[3].y, dest[3].y)}
    };

    fixed_t B[8] = {
        // Les valeurs de B sont les coordonnées x et y de dest
        dest[0].x, dest[0].y,
        dest[1].x, dest[1].y,
        dest[2].x, dest[2].y,
        dest[3].x, dest[3].y
    };

    // Appliquer la méthode de Gauss-Jordan pour résoudre le système d'équations
    gauss_jordan(A, B);

    // Remplir la matrice de perspective
    matrix[0][0] = B[0];
    matrix[0][1] = B[1];
    matrix[0][2] = B[2];
    matrix[1][0] = B[3];
    matrix[1][1] = B[4];
    matrix[1][2] = B[5];
    matrix[2][0] = B[6];
    matrix[2][1] = B[7];
    matrix[2][2] = FIXED_ONE;
}

void DrawWireframeQuad(fVector2 points[4], int color)
{
    for (int i = 0; i < 4; i++)
    {
        dline(TO_INT(points[i].x), TO_INT(points[i].y), TO_INT(points[(i + 1) % 4].x), TO_INT(points[(i + 1) % 4].y), color);
    }
}

void DrawFilledQuadTexture2(fVector2 points[4], ModeUV uv_mode)
{
    fVector2 top_right = points[0];
    fVector2 top_left = points[1];
    fVector2 bottom_left = points[2];
    fVector2 bottom_right = points[3];

    if (top_left.y > bottom_left.y)
    {
        top_left = points[2];
        bottom_left = points[3];
        bottom_right = points[0];
        top_right = points[1];
    }

    const fixed_t size = INT_TO_FIXED(16);
    const fVector2 start_tex = {0, size*3};
    const fVector2 end_tex = {size, size*4};

    fixed_t matrix[3][3];
    fVector2 src[4] = {{start_tex.x, start_tex.y},
                        {end_tex.x - FIXED_ONE, start_tex.y},
                        {end_tex.x - FIXED_ONE, end_tex.y - FIXED_ONE},
                        {start_tex.x, end_tex.y - FIXED_ONE}};
    fVector2 dest[4] = {top_left, top_right, bottom_right, bottom_left};
    computePerspectiveMatrix(dest, src, matrix);

    const fixed_t slope_top = slope(top_left, top_right);
    const fixed_t slope_left = slope(top_left, bottom_left);
    const fixed_t slope_right = slope(top_right, bottom_right);
    const fixed_t slope_bottom = slope(bottom_left, bottom_right);

    const int y_start = max(min(TO_INT(top_left.y), TO_INT(top_right.y)), 0);
    const int y_end = min(max(TO_INT(bottom_left.y), TO_INT(bottom_right.y)), SCREEN_HEIGHT - 1);

    
    for (int y = y_start; y < y_end; y++)
    {
        fixed_t fy = INT_TO_FIXED(y);

        fixed_t x_start, x_end;
        if (fy < top_left.y)
        {
            x_start = top_left.x;
            if (slope_top)
                x_start += fdiv(fy - top_left.y, slope_top);
        }
        else if (fy <= bottom_left.y)
        {
            x_start = top_left.x;
            if (slope_left)
                x_start += fdiv(fy - top_left.y, slope_left);
        }
        else
        {
            x_start = bottom_left.x;
            if (slope_bottom)
                x_start += fdiv(fy - bottom_left.y, slope_bottom);
        }

        if (fy < top_right.y)
        {
            x_end = top_right.x;
            if (slope_top)
                x_end += fdiv(fy - top_right.y, slope_top);
        }
        else if (fy <= bottom_right.y)
        {
            x_end = top_right.x;
            if (slope_right)
                x_end += fdiv(fy - top_right.y, slope_right);
        }
        else
        {
            x_end = bottom_right.x;
            if (slope_bottom)
                x_end += fdiv(fy - bottom_right.y, slope_bottom);
        }

        if (x_start > x_end)
            swap(&x_start, &x_end);

        int x_start_int = TO_INT(x_start);
        int x_end_int = TO_INT(x_end);
        if (y < 0 || y >= SCREEN_HEIGHT)
            continue;
        x_start_int = max(min(x_start_int, SCREEN_WIDTH - 1), 0);
        x_end_int = max(min(x_end_int, SCREEN_WIDTH - 1), 0);
        const fixed_t nx_start = INT_TO_FIXED(x_start_int);
        const fixed_t nx_end = INT_TO_FIXED(x_end_int);

        // Précalculs pour l'interpolation des UV
        if (x_end != x_start && nx_end != nx_start)
        {
            if (uv_mode == UV_NEAREST)
            {
                for (int x = x_start_int; x < x_end_int; x++)
                {
                    const fVector2 vec = {INT_TO_FIXED(x), fy};
                    
                    fVector2 uv;
                    multiplyMatrixVector(matrix, vec, &uv);
                    int tex_color = get_uv_map_img((uv.x >> PRECISION), (uv.y >> PRECISION));
                    DrawPixel(x, y, tex_color);
                }
            }
            else
            {
                //Use slope to calculate the UV
                fVector2 uv_start, uv_end;
                multiplyMatrixVector(matrix, (fVector2){nx_start, fy}, &uv_start);
                multiplyMatrixVector(matrix, (fVector2){nx_end, fy}, &uv_end);
                fixed_t du = fdiv(uv_end.x - uv_start.x, nx_end - nx_start);
                fixed_t dv = fdiv(uv_end.y - uv_start.y, nx_end - nx_start);
                fixed_t u = fixed_abs(uv_start.x);
                fixed_t v = fixed_abs(uv_start.y)+FIXED_ONE;
                for (int x = x_start_int; x < x_end_int; x++)
                {
                    int tex_color = get_uv_map_img((u >> PRECISION), (v >> PRECISION));
                    DrawPixel(x, y, tex_color);
                    u += du;
                    v += dv;
                }
            }
        }

    }
}


void DrawFilledQuadTexture(fVector2 points[4], ModeUV uv_mode)
{
    fVector2 top_right = points[0];
    fVector2 top_left = points[1];
    fVector2 bottom_left = points[2];
    fVector2 bottom_right = points[3];

    if (top_left.y > bottom_left.y)
    {
        top_left = points[2];
        bottom_left = points[3];
        bottom_right = points[0];
        top_right = points[1];
    }

    const fixed_t size = INT_TO_FIXED(40);
    const fVector2 start_tex = {0, size*3};
    const fVector2 end_tex = {size, size*4};

    fVector2 uv_coords[4] = {{start_tex.x, start_tex.y},
                        {end_tex.x, start_tex.y},
                        {end_tex.x, end_tex.y},
                        {start_tex.x, end_tex.y}};

    const fixed_t slope_top = slope(top_left, top_right);
    const fixed_t slope_left = slope(top_left, bottom_left);
    const fixed_t slope_right = slope(top_right, bottom_right);
    const fixed_t slope_bottom = slope(bottom_left, bottom_right);

    fVector2 uv_slope_left = {0, 0}, uv_slope_right = {0, 0}, uv_slope_top = {0, 0}, uv_slope_bottom = {0, 0};
    if (bottom_left.y != top_left.y)
    {
        uv_slope_left.x = fdiv(uv_coords[2].x - uv_coords[1].x, bottom_left.y - top_left.y);
        uv_slope_left.y = fdiv(uv_coords[2].y - uv_coords[1].y, bottom_left.y - top_left.y);
    }
    if (bottom_right.y != top_right.y)
    {
        uv_slope_right.x = fdiv(uv_coords[3].x - uv_coords[0].x, bottom_right.y - top_right.y);
        uv_slope_right.y = fdiv(uv_coords[3].y - uv_coords[0].y, bottom_right.y - top_right.y);
    }
    if (top_right.y != top_left.y)
    {
        uv_slope_top.x = fdiv(uv_coords[1].x - uv_coords[0].x, top_right.y - top_left.y);
        uv_slope_top.y = fdiv(uv_coords[1].y - uv_coords[0].y, top_right.y - top_left.y);
    }
    if (bottom_right.y != bottom_left.y)
    {
        uv_slope_bottom.x = fdiv(uv_coords[2].x - uv_coords[3].x, bottom_right.y - bottom_left.y);
        uv_slope_bottom.y = fdiv(uv_coords[2].y - uv_coords[3].y, bottom_right.y - bottom_left.y);
    }

    const int y_start = max(min(TO_INT(top_left.y), TO_INT(top_right.y)), 0);
    const int y_end = min(max(TO_INT(bottom_left.y), TO_INT(bottom_right.y)), SCREEN_HEIGHT - 1);

    for (int y = y_start; y < y_end; y++)
    {
        fixed_t fy = INT_TO_FIXED(y);

        fixed_t x_start, x_end;
        fixed_t u_start, u_end;
        fixed_t v_start, v_end;

        if (fy < top_left.y)
        {
            x_start = top_left.x;
            u_start = uv_coords[0].x;
            v_start = uv_coords[0].y;
            fixed_t rt = fy - top_left.y;

            if (slope_top)
            {
                x_start += fdiv(rt, slope_top);
            }
            if (uv_slope_top.x)
                u_start += fdiv(rt, uv_slope_top.x);
            if (uv_slope_top.y)
                v_start += fdiv(rt, uv_slope_top.y);
        }
        else if (fy <= bottom_left.y)
        {
            x_start = top_left.x;
            u_start = uv_coords[0].x;
            v_start = uv_coords[0].y;
            fixed_t rt = fy - top_left.y;

            if (slope_left)
            {
                x_start += fdiv(rt, slope_left);
            }
            if (uv_slope_left.x)
                u_start += fdiv(rt, uv_slope_left.x);
            if (uv_slope_left.y)
                v_start += fdiv(rt, uv_slope_left.y);
        }
        else
        {
            x_start = bottom_left.x;
            u_start = uv_coords[3].x;
            v_start = uv_coords[3].y;
            fixed_t rt = fy - bottom_left.y;

            if (slope_bottom)
            {
                x_start += fdiv(rt, slope_bottom);
                
            }
            if (uv_slope_bottom.x)
                u_start += fdiv(rt, uv_slope_bottom.x);
            if (uv_slope_bottom.y)
                v_start += fdiv(rt, uv_slope_bottom.y);
        }

        if (fy < top_right.y)
        {
            x_end = top_right.x;
            u_end = uv_coords[1].x;
            v_end = uv_coords[1].y;
            fixed_t rt = fy - top_right.y;

            if (slope_top)
            {
                x_end += fdiv(rt, slope_top);
            }
            if (uv_slope_top.x)
                u_end += fdiv(rt, uv_slope_top.x);
            if (uv_slope_top.y)
                v_end += fdiv(rt, uv_slope_top.y);
        }
        else if (fy <= bottom_right.y)
        {
            x_end = top_right.x;
            u_end = uv_coords[1].x;
            v_end = uv_coords[1].y;
            fixed_t rt = fy - top_right.y;

            if (slope_right)
            {
                x_end += fdiv(rt, slope_right);
            }
            if (uv_slope_right.x)
                u_end += fdiv(rt, uv_slope_right.x);
            if (uv_slope_right.y)
                v_end += fdiv(rt, uv_slope_right.y);
        }
        else
        {
            x_end = bottom_right.x;
            u_end = uv_coords[2].x;
            v_end = uv_coords[2].y;
            fixed_t rt = fy - bottom_right.y;

            if (slope_bottom)
            {
                x_end += fdiv(rt, slope_bottom);
            }
            if (uv_slope_bottom.x)
                u_end += fdiv(rt, uv_slope_bottom.x);
            if (uv_slope_bottom.y)
                v_end += fdiv(rt, uv_slope_bottom.y);
        }

        if (x_start > x_end)
        {
            swap(&x_start, &x_end);
            swap(&u_start, &u_end);
        }

        int x_start_int = TO_INT(x_start);
        int x_end_int = TO_INT(x_end);
        if (y < 0 || y >= SCREEN_HEIGHT)
            continue;
        const fixed_t x_start_int_org = x_start_int;
        x_start_int = max(min(x_start_int, SCREEN_WIDTH - 1), 0);
        x_end_int = max(min(x_end_int, SCREEN_WIDTH - 1), 0);

        fixed_t u = u_start;
        fixed_t v = v_start;
        if (x_end == x_start)
            continue;
        fixed_t u_step = fdiv(u_end - u_start, x_end - x_start);
        fixed_t v_step = fdiv(v_end - v_start, x_end - x_start);
        u += u_step * (x_start_int - x_start_int_org);

        for (int x = x_start_int; x < x_end_int; x++)
        {
            int tex_color = get_uv_map((u >> PRECISION)%40, (v >> PRECISION)%40);
            DrawPixel(x, y, tex_color);

            u += u_step;
            v += v_step;
        }
    }
}