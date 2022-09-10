#pragma once

#include "carm95_hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HOOK_UNAVAILABLE = -1,
    HOOK_DISABLED = 0,
    HOOK_ENABLED = 1,
} function_hook_state_t;

typedef struct {
    function_hook_state_t *hook_state;
    const char *name;
    const char *location;
} function_hook_data_t;

int start_hook_webserver(int port);
void stop_hook_webserver(void);
void webserver_register_variable(const char *name, function_hook_state_t *state, const char *file, int line);

#define CARM95_WEBSERVER_STATE(VARIABLE)                                            \
    HOOK_FUNCTION_STARTUP(VARIABLE) {                                               \
        webserver_register_variable(#VARIABLE, &(VARIABLE), __FILE__, __LINE__);    \
    }

#ifdef __cplusplus
}
#endif
