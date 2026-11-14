// fxGLMark scene driver for host-side regression checks and profiling.
//
//   fxglmark_scenes <scene> [frames]       print a hash of color+depth per frame
//   fxglmark_scenes --check <golden-file>  compare every scene with stored hashes
//   fxglmark_scenes --write <golden-file>  regenerate the stored hashes
//
// Frames render at the fx-CG50 resolution (396x224) through the real fxGLMark
// scene code and fxOGL. The hash covers both the RGB565 color buffer and the
// 16-bit depth buffer, so any change in rendered output is detected.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <GLES2/gl2.h>
#include <fxgl/platform.h>
#include "glmark2/glmark2.hpp"

// Gint/libprof stubs required by glmark2.cpp on the host.
uint32_t fxmark_test_prof_counter = 0;
void dupdate(void) {}

namespace
{
constexpr int WIDTH = 396;
constexpr int HEIGHT = 224;
constexpr unsigned GOLDEN_FRAMES = 3;
const char *const SCENES[] = {
    "clear",
    "build:use-vbo=true",
    "build:use-vbo=false",
    "texture:texture-filter=nearest",
    "texture:texture-filter=linear",
    "shading:shading=gouraud",
};

uint16_t color[WIDTH * HEIGHT];
uint16_t depth[WIDTH * HEIGHT];

uint64_t fnv1a(const void *data, size_t size, uint64_t hash)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Render `frames` frames of one scene and append one line per frame. */
bool render_scene(const char *description, unsigned frames, std::string& out)
{
    std::memset(color, 0, sizeof(color));
    std::memset(depth, 0xff, sizeof(depth));
    const fxgl_surface_t surface = {color, depth, nullptr, WIDTH, HEIGHT, WIDTH};
    if (fxglMakeCurrent(&surface) != GL_TRUE)
        return false;

    char error[160] = {};
    fxglmark2::BenchmarkRequest request = {};
    fxglmark2::SceneInstance scene = {};
    if (!fxglmark2::parse_benchmark(description, request, error, sizeof(error)) ||
        !fxglmark2::prepare_scene(request, scene, error, sizeof(error)))
    {
        std::fprintf(stderr, "%s: %s\n", description, error);
        return false;
    }
    bool ok = true;
    for (unsigned frame = 0; frame < frames && ok; ++frame)
    {
        if (!fxglmark2::render_scene_frame(scene, frame))
        {
            std::fprintf(stderr, "%s: frame %u failed to render\n", description, frame);
            ok = false;
            break;
        }
        uint64_t hash = fnv1a(color, sizeof(color), 1469598103934665603ull);
        hash = fnv1a(depth, sizeof(depth), hash);
        unsigned lit = 0;
        for (uint16_t pixel : color)
            lit += pixel != 0;
        char line[160];
        std::snprintf(line, sizeof(line), "%s frame %u hash %016llx lit %u\n",
                      description, frame, static_cast<unsigned long long>(hash), lit);
        out += line;
    }
    fxglmark2::finish_scene(scene);
    fxglReleaseCurrent();
    return ok;
}

bool render_all(std::string& out)
{
    for (const char *scene : SCENES)
        if (!render_scene(scene, GOLDEN_FRAMES, out))
            return false;
    return true;
}

bool read_file(const char *path, std::string& contents)
{
    FILE *file = std::fopen(path, "rb");
    if (!file)
        return false;
    char buffer[4096];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        contents.append(buffer, count);
    std::fclose(file);
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 3 && (std::strcmp(argv[1], "--check") == 0 ||
                      std::strcmp(argv[1], "--write") == 0))
    {
        std::string actual;
        if (!render_all(actual))
            return 1;
        if (argv[1][2] == 'w')
        {
            FILE *file = std::fopen(argv[2], "wb");
            if (!file || std::fwrite(actual.data(), 1, actual.size(), file) != actual.size())
                return 1;
            std::fclose(file);
            std::printf("wrote %s\n", argv[2]);
            return 0;
        }
        std::string expected;
        if (!read_file(argv[2], expected))
        {
            std::fprintf(stderr, "cannot read %s\n", argv[2]);
            return 1;
        }
        if (actual != expected)
        {
            std::fprintf(stderr, "Rendered frames differ from %s.\nExpected:\n%s\nActual:\n%s",
                         argv[2], expected.c_str(), actual.c_str());
            std::fprintf(stderr, "If the change is intended, regenerate with --write.\n");
            return 1;
        }
        std::printf("fxGLMark golden frames: all %zu scenes x %u frames bit-identical\n",
                    sizeof(SCENES) / sizeof(SCENES[0]), GOLDEN_FRAMES);
        return 0;
    }
    if (argc >= 2 && argc <= 3 && argv[1][0] != '-')
    {
        const unsigned frames = argc == 3
            ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 1;
        std::string out;
        if (!render_scene(argv[1], frames, out))
            return 1;
        std::fputs(out.c_str(), stdout);
        return 0;
    }
    std::fprintf(stderr,
        "usage: %s <scene> [frames] | --check <golden-file> | --write <golden-file>\n",
        argv[0]);
    return 2;
}
