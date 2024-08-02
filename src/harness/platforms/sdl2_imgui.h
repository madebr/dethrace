#ifdef __cplusplus
extern "C" {
#endif

typedef union SDL_Event SDL_Event;

int sdl2_imgui_init(void);

void imgui_sdl2_process_event(SDL_Event* event);

int imgui_sdl2_initframe(void);

void imgui_sdl2_drawframe(void);

#ifdef __cplusplus
}
#endif

#ifndef DETHRACE_IMGUI
#define sdl2_imgui_init() do { } while (0)
#define imgui_sdl2_process_event(E) do { } while (0)
#define imgui_sdl2_initframe() do { 1; } while (0)
#define imgui_sdl2_drawframe() do { } while (0)
#endif
