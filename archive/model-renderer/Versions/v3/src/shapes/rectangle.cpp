#include "shapes.hpp"

void DrawRectUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    if (tl.y > bl.y) {
        std::swap(tl, bl);
        std::swap(tr, br);
    }
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
            DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
            u += du;
            v += dv;
        }

        u_st += du_st;
        v_st += dv_st;
        u_end += du_end;
        v_end += dv_end;
    }
}

//simplified version of DrawRect and optimized for a rectangle
void DrawRect(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    if (tl.y > bl.y) {
        std::swap(tl, bl);
        std::swap(tr, br);
    }
    const int startY = std::max(tl.y, 0);
    const int endY = std::min(br.y, SCREEN_HEIGHT - 1);
    const int startX = std::max(tl.x, 0);
    const int endX = std::min(br.x, SCREEN_WIDTH - 1);

    const int height = br.y - tl.y;
    const int width = br.x - tl.x;

    if (width <= 0 || height <= 0) return;

    const fixed_t inv_height = FIXED_ONE / height;
    const fixed_t inv_length = FIXED_ONE / width;

    const fixed_t du = fmul((br.u - tl.u), inv_length);
    const fixed_t dv = fmul((br.v - tl.v), inv_height);

    // Ajustement initial des UV selon le décalage initial de X et Y
    fixed_t v = tl.v + dv * (startY - tl.y);

    for (int y = startY; y <= endY; ++y) {
        fixed_t u = tl.u + du * (startX - tl.x);
        for (int x = startX; x <= endX; ++x) {
            DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
            u += du;
        }
        v += dv;
    }
}