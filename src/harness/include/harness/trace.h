#ifndef HARNESS_TRACE_H
#define HARNESS_TRACE_H

#include "brender/br_types.h"

#include <stdlib.h>

#if defined(STDIO_NOCOLOR)
#define COLOR_RED ""
#define COLOR_GREEN ""
#define COLOR_BROWN ""
#define COLOR_BLUE ""
#define COLOR_WHITE ""
#define COLOR_RESET ""
#else
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_BROWN "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_WHITE "\033[38m"
#define COLOR_RESET "\033[0m"
#endif

extern int harness_debug_level;
extern int OS_IsDebuggerPresent(void);

void debug_printf(const char* fmt, const char* fn, const char* fmt2, ...);
void debug_print_vector3(const char* fmt, const char* fn, char* msg, br_vector3* v);
void debug_print_matrix34(const char* fmt, const char* fn, char* name, br_matrix34* m);
void debug_print_matrix4(const char* fmt, const char* fn, char* name, br_matrix4* m);

#define BLUE

#define LOG_TRACE(...)                                         \
    if (harness_debug_level >= 5) {                            \
        debug_printf("[TRACE] %s", __FUNCTION__, __VA_ARGS__); \
    }

#define LOG_TRACE10(...)                                       \
    if (harness_debug_level >= 10) {                           \
        debug_printf("[TRACE] %s", __FUNCTION__, __VA_ARGS__); \
    }
#define LOG_TRACE9(...)                                        \
    if (harness_debug_level >= 9) {                            \
        debug_printf("[TRACE] %s", __FUNCTION__, __VA_ARGS__); \
    }

#define LOG_TRACE8(...)                                        \
    if (harness_debug_level >= 8) {                            \
        debug_printf("[TRACE] %s", __FUNCTION__, __VA_ARGS__); \
    }

#define LOG_DEBUG(...) debug_printf(COLOR_BLUE "[DEBUG] %s ", __FUNCTION__, __VA_ARGS__)
#define LOG_VEC(msg, v) debug_print_vector3(COLOR_BLUE "[DEBUG] %s ", __FUNCTION__, msg, v)
#define LOG_MATRIX(msg, m) debug_print_matrix34(COLOR_BLUE "[DEBUG] %s ", __FUNCTION__, msg, m)
#define LOG_MATRIX4(msg, m) debug_print_matrix4(COLOR_BLUE "[DEBUG] %s ", __FUNCTION__, msg, m)
#define LOG_INFO(...) debug_printf(COLOR_BLUE "[INFO] %s ", __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...) debug_printf(COLOR_BROWN "[WARN] %s ", __FUNCTION__, __VA_ARGS__)
#define LOG_PANIC(...)                                                    \
    do {                                                                  \
        debug_printf(COLOR_RED "[PANIC] %s ", __FUNCTION__, __VA_ARGS__); \
        if (OS_IsDebuggerPresent())                                       \
            abort();                                                      \
        else                                                              \
            exit(1);                                                      \
    } while (0)

#define LOG_WARN_ONCE(...)                                                 \
    static int warn_printed = 0;                                           \
    if (!warn_printed) {                                                   \
        debug_printf(COLOR_BROWN "[WARN] %s ", __FUNCTION__, __VA_ARGS__); \
        warn_printed = 1;                                                  \
    }

#define NOT_IMPLEMENTED() \
    LOG_PANIC("not implemented")

#define TELL_ME_IF_WE_PASS_THIS_WAY() \
    LOG_PANIC("code path not expected")

#define STUB() \
    debug_printf(COLOR_RED "[WARN] %s ", __FUNCTION__, "%s", "stubbed");

#define STUB_ONCE()                                                          \
    static int stub_printed = 0;                                             \
    if (!stub_printed) {                                                     \
        debug_printf(COLOR_RED "[WARN] %s ", __FUNCTION__, "%s", "stubbed"); \
        stub_printed = 1;                                                    \
    }

// int count_open_fds();

#endif
