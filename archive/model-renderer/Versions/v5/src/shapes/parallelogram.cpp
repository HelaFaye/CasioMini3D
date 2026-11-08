#include "shapes.hpp"

void DrawHorzParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.y > bl.y){
        std::swap(tl, bl);
        std::swap(tr, br);
    }
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    const int startY = std::max(tl.y, 0);
    const int endY = std::min(br.y, SCREEN_HEIGHT - 1);

    const int height = br.y - tl.y;
    if (height <= 0) return;

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

    fixed_t x_st = INT_TO_FIXED(tl.x) + fmul(INT_TO_FIXED((bl.x - tl.x)), inv_height * deltaStartY);
    fixed_t x_end = INT_TO_FIXED(tr.x) + fmul(INT_TO_FIXED((br.x - tr.x)), inv_height * deltaStartY);

    fixed_t dx_st = fmul(INT_TO_FIXED((bl.x - tl.x)), inv_height);
    fixed_t dx_end = fmul(INT_TO_FIXED((br.x - tr.x)), inv_height);

    for (int y = startY; y <= endY; ++y) {
        const int x_st_int = TO_INT(x_st);
        const int x_end_int = TO_INT(x_end);

        const int startX = std::max(x_st_int, 0);
        const int endX = std::min(x_end_int, SCREEN_WIDTH - 1);

        const int length = x_end_int - x_st_int;
        if (length > 0) {
            const fixed_t inv_length = FIXED_ONE / length;
            const fixed_t du = fmul(u_end - u_st, inv_length);
            const fixed_t dv = fmul(v_end - v_st, inv_length);

            fixed_t u = u_st + du * (startX - x_st_int);
            fixed_t v = v_st + dv * (startX - x_st_int);

            for (int x = startX; x <= endX; ++x) {
                DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
                u += du;
                v += dv;
            }
        }
        x_st += dx_st;
        x_end += dx_end;
        u_st += du_st;
        v_st += dv_st;
        u_end += du_end;
        v_end += dv_end;
    }
}

//simplified version of DrawHorzParallelogram and optimized
void DrawHorzParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.y > bl.y){
        std::swap(tl, bl);
        std::swap(tr, br);
    }
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    const int startY = std::max(tl.y, 0);
    const int endY = std::min(br.y, SCREEN_HEIGHT - 1);

    const int height = br.y - tl.y;
    if (height <= 0) return;

    const fixed_t inv_height = FIXED_ONE / height;
    const int deltaStartY = startY - tl.y;
    fixed_t x_st = INT_TO_FIXED(tl.x) + fmul(INT_TO_FIXED((bl.x - tl.x)), inv_height * deltaStartY);
    fixed_t x_end = INT_TO_FIXED(tr.x) + fmul(INT_TO_FIXED((br.x - tr.x)), inv_height * deltaStartY);
    fixed_t dx_st = fmul(INT_TO_FIXED((bl.x - tl.x)), inv_height);
    fixed_t dx_end = fmul(INT_TO_FIXED((br.x - tr.x)), inv_height);

    const fixed_t dv = fmul((br.v - tl.v), inv_height);
    fixed_t v = tl.v + dv * deltaStartY;

    for (int y = startY; y <= endY; ++y) {
        const int x_st_int = TO_INT(x_st);
        const int x_end_int = TO_INT(x_end);

        const int startX = std::max(x_st_int, 0);
        const int endX = std::min(x_end_int, SCREEN_WIDTH - 1);

        const int line_width = x_end_int - x_st_int;
        if (line_width > 0) {
            const fixed_t du = fmul(br.u - tl.u, FIXED_ONE / line_width);
            fixed_t u = tl.u + du * (startX - x_st_int);

            for (int x = startX; x <= endX; ++x) {
                DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
                u += du;
            }
        }

        x_st += dx_st;
        x_end += dx_end;
        v += dv;
    }
}



void DrawVertParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    if (tl.y > bl.y) {
        std::swap(tl, bl);
        std::swap(tr, br);
    }
    const int startX = std::max(tl.x, 0);
    const int endX = std::min(tr.x, SCREEN_WIDTH - 1);

    const int width = tr.x - tl.x;
    if (width <= 0) return;

    const fixed_t inv_width = FIXED_ONE / width;
    const fixed_t du_top = fmul((tr.u - tl.u), inv_width);
    const fixed_t dv_top = fmul((tr.v - tl.v), inv_width);
    const fixed_t du_bottom = fmul((br.u - bl.u), inv_width);
    const fixed_t dv_bottom = fmul((br.v - bl.v), inv_width);

    const fixed_t dy_top = fmul(INT_TO_FIXED((tr.y - tl.y)), inv_width);
    const fixed_t dy_bottom = fmul(INT_TO_FIXED((br.y - bl.y)), inv_width);

    const int deltaX = startX - tl.x;

    fixed_t u_top = tl.u + du_top * deltaX;
    fixed_t v_top = tl.v + dv_top * deltaX;
    fixed_t u_bottom = bl.u + du_bottom * deltaX;
    fixed_t v_bottom = bl.v + dv_bottom * deltaX;

    fixed_t y_top = INT_TO_FIXED(tl.y) + dy_top * deltaX;
    fixed_t y_bottom = INT_TO_FIXED(bl.y) + dy_bottom * deltaX;

    for (int x = startX; x <= endX; ++x) {
        const int y_top_int = TO_INT(y_top);
        const int y_bottom_int = TO_INT(y_bottom);

        const int startY = std::max(y_top_int, 0);
        const int endY = std::min(y_bottom_int, SCREEN_HEIGHT - 1);

        const int length = y_bottom_int - y_top_int;
        if (length > 0) {
            const fixed_t inv_length = FIXED_ONE / length;
            const fixed_t du = fmul(u_bottom - u_top, inv_length);
            const fixed_t dv = fmul(v_bottom - v_top, inv_length);

            fixed_t u = u_top + du * (startY - y_top_int);
            fixed_t v = v_top + dv * (startY - y_top_int);

            for (int y = startY; y <= endY; ++y) {
                DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
                u += du;
                v += dv;
            }
        }
        y_top += dy_top;
        y_bottom += dy_bottom;
        u_top += du_top;
        v_top += dv_top;
        u_bottom += du_bottom;
        v_bottom += dv_bottom;
    }
}

void DrawVertParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.x > tr.x) {
        std::swap(tl, tr);
        std::swap(bl, br);
    }
    if (tl.y > bl.y) {
        std::swap(tl, bl);
        std::swap(tr, br);
    }
    const int startX = std::max(tl.x, 0);
    const int endX = std::min(tr.x, SCREEN_WIDTH - 1);

    const int width = tr.x - tl.x;
    if (width <= 0) return;

    const fixed_t inv_width = FIXED_ONE / width;
    const fixed_t dy_top = fmul(INT_TO_FIXED((tr.y - tl.y)), inv_width);
    const fixed_t dy_bottom = fmul(INT_TO_FIXED((br.y - bl.y)), inv_width);

    const int deltaX = startX - tl.x;

    fixed_t y_top = INT_TO_FIXED(tl.y) + dy_top * deltaX;
    fixed_t y_bottom = INT_TO_FIXED(bl.y) + dy_bottom * deltaX;

    const fixed_t du = fmul((br.u - tl.u), inv_width);

    fixed_t u = tl.u + du * deltaX;

    for (int x = startX; x <= endX; ++x) {
        const int y_top_int = TO_INT(y_top);
        const int y_bottom_int = TO_INT(y_bottom);

        const int startY = std::max(y_top_int, 0);
        const int endY = std::min(y_bottom_int, SCREEN_HEIGHT - 1);

        const int line_height = y_bottom_int - y_top_int;
        if (line_height > 0) {
            const fixed_t dv = fmul(br.v - tl.v, FIXED_ONE / line_height);
            fixed_t v = tl.v + dv * (startY - y_top_int);

            for (int y = startY; y <= endY; ++y) {
                DrawPixelTexture(TO_INT(u), TO_INT(v), x, y);
                v += dv;
            }
        }
        y_top += dy_top;
        y_bottom += dy_bottom;
        u += du;
    }
}