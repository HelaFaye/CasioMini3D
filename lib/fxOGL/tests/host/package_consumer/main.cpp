#include <GLES2/gl2.h>
#include <fxgl/platform.h>

int main()
{
    /* Taking the address forces the installed archive to provide the API
       symbol at link time without requiring a render surface for this smoke
       test. */
    auto *get_error = &glGetError;
    return get_error == nullptr ? 1 : 0;
}
