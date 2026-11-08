

def pourcent(st,end, x):
    return (x - st) / (end - st)

def lerp(a, b, x):
    return a + x * (b - a)

for i in range(0, 101):
    print(pourcent(0, 100, i))



//% between top_right and bottom_left
//(x - st) / (end - st)
fixed_t fx = INT_TO_FIXED(x);
fixed_t diagonal1_x = fdiv((fx - top_right.x), (bottom_left.x - top_right.x));
fixed_t diagonal1_y = fdiv((fy - top_right.y), (bottom_left.y - top_right.y));
fixed_t diagonal2_x = fdiv((fx - top_left.x), (bottom_right.x - top_left.x));
fixed_t diagonal2_y = fdiv((fy - top_left.y), (bottom_right.y - top_left.y));
//a + x * (b - a)
const fVector2 uvs[4] = {
    {0, 0},
    {0, size},
    {size, size},
    {size, 0}
};
fixed_t ndiagonal1_x = uvs[0].x + fmul(diagonal1_x, (uvs[2].x - uvs[0].x));
fixed_t ndiagonal1_y = uvs[0].y + fmul(diagonal1_y, (uvs[2].y - uvs[0].y));
fixed_t ndiagonal2_x = uvs[1].x + fmul(diagonal2_x, (uvs[3].x - uvs[1].x));
fixed_t ndiagonal2_y = uvs[1].y + fmul(diagonal2_y, (uvs[3].y - uvs[1].y));
//moyenne des deux diagonales
fixed_t u = ndiagonal1_x;//(ndiagonal1_x + ndiagonal2_x) / 2;
fixed_t v = ndiagonal2_y;//(ndiagonal1_y + ndiagonal2_y) / 2;