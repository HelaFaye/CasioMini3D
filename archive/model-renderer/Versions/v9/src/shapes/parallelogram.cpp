#include "shapes.hpp"

void DrawHorzParallelogramUV(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.y > bl.y) { std::swap(tl, bl); std::swap(tr, br); }
    if (tl.x > tr.x) { std::swap(tl, tr); std::swap(bl, br); }

    const int startY = std::max(tl.y, 0);
    const int endY = std::min(br.y, SCREEN_HEIGHT - 1);
    const int height = br.y - tl.y;
    if (height <= 0) return;

    const fixed_t inv_height = fixed_t::one() / height;
    const fixed_t du_st = (bl.u - tl.u) * inv_height;
    const fixed_t dv_st = (bl.v - tl.v) * inv_height;
    const fixed_t du_end = (br.u - tr.u) * inv_height;
    const fixed_t dv_end = (br.v - tr.v) * inv_height;

    const int deltaStartY = startY - tl.y;
    fixed_t u_st = tl.u + du_st * deltaStartY;
    fixed_t v_st = tl.v + dv_st * deltaStartY;
    fixed_t u_end = tr.u + du_end * deltaStartY;
    fixed_t v_end = tr.v + dv_end * deltaStartY;

    fixed_t x_st = fixed_t(tl.x) + fixed_t(bl.x - tl.x) * (inv_height * deltaStartY);
    fixed_t x_end = fixed_t(tr.x) + fixed_t(br.x - tr.x) * (inv_height * deltaStartY);

    fixed_t dx_st = fixed_t(bl.x - tl.x) * inv_height;
    fixed_t dx_end = fixed_t(br.x - tr.x) * inv_height;

    for (int y = startY; y <= endY; ++y) {
        int x_st_int = int(x_st);
        int x_end_int = int(x_end);

        int startX = std::max(x_st_int, 0);
        int endX = std::min(x_end_int, SCREEN_WIDTH - 1);
        int length = x_end_int - x_st_int;

        if (length > 0) {
            fixed_t inv_length = fixed_t::one() / length;
            fixed_t du = (u_end - u_st) * inv_length;
            fixed_t dv = (v_end - v_st) * inv_length;
            fixed_t u = u_st + du * (startX - x_st_int);
            fixed_t v = v_st + dv * (startX - x_st_int);

            for (int x = startX; x <= endX; ++x) {
                DrawPixelTexture(int(u), int(v), x, y);
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

void DrawHorzParallelogram(VertexLocal &tl, VertexLocal &tr, VertexLocal &br, VertexLocal &bl)
{
    if (tl.y > bl.y) { std::swap(tl, bl); std::swap(tr, br); }
    if (tl.x > tr.x) { std::swap(tl, tr); std::swap(bl, br); }

    int startY = std::max(tl.y, 0);
    int endY = std::min(br.y, SCREEN_HEIGHT - 1);
    int height = br.y - tl.y;
    if (height <= 0) return;

    fixed_t inv_height = fixed_t::one() / height;
    int deltaStartY = startY - tl.y;

    fixed_t x_st = fixed_t(tl.x) + fixed_t(bl.x - tl.x) * (inv_height * deltaStartY);
    fixed_t x_end = fixed_t(tr.x) + fixed_t(br.x - tr.x) * (inv_height * deltaStartY);
    fixed_t dx_st = fixed_t(bl.x - tl.x) * inv_height;
    fixed_t dx_end = fixed_t(br.x - tr.x) * inv_height;

    fixed_t dv = (br.v - tl.v) * inv_height;
    fixed_t v = tl.v + dv * deltaStartY;

    for (int y = startY; y <= endY; ++y) {
        int x_st_int = int(x_st);
        int x_end_int = int(x_end);
        int startX = std::max(x_st_int, 0);
        int endX = std::min(x_end_int, SCREEN_WIDTH - 1);
        int line_width = x_end_int - x_st_int;

        if (line_width > 0) {
            fixed_t du = (br.u - tl.u) / line_width;
            fixed_t u = tl.u + du * (startX - x_st_int);

            for (int x = startX; x <= endX; ++x) {
                DrawPixelTexture(int(u), int(v), x, y);
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
    if (tl.x > tr.x) { std::swap(tl, tr); std::swap(bl, br); }
    if (tl.y > bl.y) { std::swap(tl, bl); std::swap(tr, br); }

    int startX = std::max(tl.x, 0);
    int endX = std::min(tr.x, SCREEN_WIDTH - 1);
    int width = tr.x - tl.x;
    if (width <= 0) return;

    fixed_t inv_width = fixed_t::one() / width;
    fixed_t du_top = (tr.u - tl.u) * inv_width;
    fixed_t dv_top = (tr.v - tl.v) * inv_width;
    fixed_t du_bottom = (br.u - bl.u) * inv_width;
    fixed_t dv_bottom = (br.v - bl.v) * inv_width;

    fixed_t dy_top = fixed_t(tr.y - tl.y) * inv_width;
    fixed_t dy_bottom = fixed_t(br.y - bl.y) * inv_width;

    int deltaX = startX - tl.x;

    fixed_t u_top = tl.u + du_top * deltaX;
    fixed_t v_top = tl.v + dv_top * deltaX;
    fixed_t u_bottom = bl.u + du_bottom * deltaX;
    fixed_t v_bottom = bl.v + dv_bottom * deltaX;

    fixed_t y_top = fixed_t(tl.y) + dy_top * deltaX;
    fixed_t y_bottom = fixed_t(bl.y) + dy_bottom * deltaX;

    for (int x = startX; x <= endX; ++x) {
        int y_top_int = int(y_top);
        int y_bottom_int = int(y_bottom);
        int startY = std::max(y_top_int, 0);
        int endY = std::min(y_bottom_int, SCREEN_HEIGHT - 1);
        int length = y_bottom_int - y_top_int;

        if (length > 0) {
            fixed_t inv_length = fixed_t::one() / length;
            fixed_t du = (u_bottom - u_top) * inv_length;
            fixed_t dv = (v_bottom - v_top) * inv_length;
            fixed_t u = u_top + du * (startY - y_top_int);
            fixed_t v = v_top + dv * (startY - y_top_int);

            for (int y = startY; y <= endY; ++y) {
                DrawPixelTexture(int(u), int(v), x, y);
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
    if (tl.x > tr.x) { std::swap(tl, tr); std::swap(bl, br); }
    if (tl.y > bl.y) { std::swap(tl, bl); std::swap(tr, br); }

    int startX = std::max(tl.x, 0);
    int endX = std::min(tr.x, SCREEN_WIDTH - 1);
    int width = tr.x - tl.x;
    if (width <= 0) return;

    fixed_t inv_width = fixed_t::one() / width;
    fixed_t dy_top = fixed_t(tr.y - tl.y) * inv_width;
    fixed_t dy_bottom = fixed_t(br.y - bl.y) * inv_width;

    int deltaX = startX - tl.x;

    fixed_t y_top = fixed_t(tl.y) + dy_top * deltaX;
    fixed_t y_bottom = fixed_t(bl.y) + dy_bottom * deltaX;
    fixed_t du = (br.u - tl.u) * inv_width;
    fixed_t u = tl.u + du * deltaX;

    for (int x = startX; x <= endX; ++x) {
        int y_top_int = int(y_top);
        int y_bottom_int = int(y_bottom);
        int startY = std::max(y_top_int, 0);
        int endY = std::min(y_bottom_int, SCREEN_HEIGHT - 1);
        int line_height = y_bottom_int - y_top_int;

        if (line_height > 0) {
            fixed_t dv = (br.v - tl.v) / line_height;
            fixed_t v = tl.v + dv * (startY - y_top_int);

            for (int y = startY; y <= endY; ++y) {
                DrawPixelTexture(int(u), int(v), x, y);
                v += dv;
            }
        }
        y_top += dy_top;
        y_bottom += dy_bottom;
        u += du;
    }
}
