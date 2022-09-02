#include "null.h"

static void Null_Init() {}
static void Null_BeginFrame(br_actor* camera, br_pixelmap* colour_buffer) {}
static void Null_EndFrame() {}
static void Null_SetPalette(uint8_t* palette) {}
static void Null_FullScreenQuad(uint8_t* src, int width, int height) {}
static void Null_Model(br_actor* actor, br_model* model, br_matrix34 model_matrix) {}
static void Null_ClearBuffers() {}
static void Null_BufferTexture(br_pixelmap* pm) {}
static void Null_BufferMaterial(br_material* mat) {}
static void Null_BufferModel(br_model* model) {}
static void Null_FlushBuffers(br_pixelmap* color_buffer, br_pixelmap* depth_buffer) {}
static void Null_GetRenderSize(int* width, int* height) { *width = 640; *height = 480; }
static void Null_GetWindowSize(int* width, int* height) { *width = 640; *height = 480; }
static void Null_SetWindowSize(int width, int height) {}
static void Null_GetViewportSize(int* x, int* y, int* width, int* height) { *x = 0; *y = 0; *width = 640; *height = 480; }

tRenderer null_renderer = {
    Null_Init,
    Null_BeginFrame,
    Null_EndFrame,
    Null_SetPalette,
    Null_FullScreenQuad,
    Null_Model,
    Null_ClearBuffers,
    Null_BufferTexture,
    Null_BufferMaterial,
    Null_BufferModel,
    Null_FlushBuffers,
    Null_GetRenderSize,
    Null_GetWindowSize,
    Null_SetWindowSize,
    Null_GetViewportSize
};
