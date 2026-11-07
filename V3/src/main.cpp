static void print_benchmark_legend()
{
    const char* text1 = "F6 clear";
    const char* text2 = "F7 triangles";
    const char* text3 = "MENU quit";

    dtext(10, 190, C_WHITE, text1);
    dtext(10, 205, C_WHITE, text2);
    dtext(10, 220, C_WHITE, text3);
}
