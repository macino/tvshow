// Single translation unit that compiles the vendored stb_image implementation
// (ADR-004). STBI_NO_STDIO: we only ever decode from memory (HTTP-fetched
// bytes), so drop the file-based API to shrink the compiled surface.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
