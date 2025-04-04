#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"

#ifdef __GO32
// FIXME: conditionally enable
float fmaxf(float a, float b) {
    return a >= b ? a : b;
}
#endif

static int DOS_Harness_SetWindowPos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    return 0;
}

static void DOS_Harness_DestroyWindow(void* hWnd) {
    return;
}

static void DOS_Harness_ProcessWindowMessages(MSG_* msg) {
    return;
}

static void DOS_Harness_GetKeyboardState(unsigned int count, uint8_t* buffer) {
    return;
}

static int DOS_Harness_GetMouseButtons(int* pButton1, int* pButton2) {
    return 0;
}

static int DOS_Harness_GetMousePosition(int* pX, int* pY) {
    return 0;
}
static void DOS_Harness_CreateWindow(const char* title, int width, int height, tHarness_window_type window_type) {
    return;
}

static void DOS_Harness_Swap(br_pixelmap* back_buffer) {
    return;
}

static void DOS_Harness_PaletteChanged(br_colour entries[256]) {
    return;
}

static void DOS_Harness_GetViewport(int* x, int* y, float* width_multipler, float* height_multiplier) {
    return;
}

static void DOS_Harness_Sleep(uint32_t dwMilliseconds) {
    return;
}

static uint32_t ticks;
static uint32_t DOS_Harness_GetTicks(void) {
    ticks++;
    return ticks;
}

static int DOS_Harness_ShowCursor(int show) {
    return 1;
}

static int DOS_Harness_ShowErrorMessage(void* window, char* text, char* caption) {
    printf("ERROR: %s\n  %s\n", text, text);
    return 0;
}

static void* DOS_Harness_GL_GetProcAddress(const char* name) {
    return NULL;
}

static int DOS_Harness_Platform_Init(tHarness_platform* platform) {
    platform->ProcessWindowMessages = DOS_Harness_ProcessWindowMessages;
    platform->Sleep = DOS_Harness_Sleep;
    platform->GetTicks = DOS_Harness_GetTicks;
    platform->ShowCursor = DOS_Harness_ShowCursor;
    platform->SetWindowPos = DOS_Harness_SetWindowPos;
    platform->DestroyWindow = DOS_Harness_DestroyWindow;
    platform->GetKeyboardState = DOS_Harness_GetKeyboardState;
    platform->GetMousePosition = DOS_Harness_GetMousePosition;
    platform->GetMouseButtons = DOS_Harness_GetMouseButtons;
    platform->ShowErrorMessage = DOS_Harness_ShowErrorMessage;

    platform->CreateWindow_ = DOS_Harness_CreateWindow;
    platform->Swap = DOS_Harness_Swap;
    platform->PaletteChanged = DOS_Harness_PaletteChanged;
    platform->GL_GetProcAddress = DOS_Harness_GL_GetProcAddress;
    platform->GetViewport = DOS_Harness_GetViewport;
    return 0;
};

const tPlatform_bootstrap DOS_bootstrap = {
    "dos",
    "DOS video backend",
    ePlatform_cap_software,
    DOS_Harness_Platform_Init,
};
