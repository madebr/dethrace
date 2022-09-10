#pragma once

#if defined(__cplusplus)
extern "C" {
#endif
#define HOOK_JOIN(V1, V2) V1##V2
#define HOOK_JOIN2(V1, V2) HOOK_JOIN(V1, V2)
#define HOOK_VALUE(V) V
#define HOOK_STRINGIFY(A) #A
#if defined(_MSC_VER)

#pragma section(".CRT$XCZ", read)
#define HOOK_STARTUP_INNER(PREFIX, NAME)                                                                       \
    static void __cdecl HOOK_JOIN(PREFIX, NAME)(void);                                                         \
    __declspec(dllexport) __declspec(allocate(".CRT$XCZ")) void (__cdecl *  HOOK_JOIN(PREFIX, NAME)##_)(void); \
    void (__cdecl * HOOK_JOIN(PREFIX, NAME)##_)(void) = HOOK_JOIN(PREFIX, NAME);                               \
    static void __cdecl HOOK_JOIN(PREFIX, NAME)(void)

#define HOOK_SHUTDOWN_INNER(PREFIX, NAME)      \
    static void HOOK_JOIN(PREFIX, NAME)(void)

#else

#define HOOK_STARTUP_INNER(PREFIX, NAME)                                    \
    static void __attribute__ ((constructor)) HOOK_JOIN(PREFIX, NAME)(void)
#define HOOK_SHUTDOWN_INNER(PREFIX, NAME)                                   \
    static void __attribute__ ((destructor)) HOOK_JOIN(PREFIX, NAME)(void)

#endif

#define HOOK_FUNCTION_STARTUP(NAME) HOOK_STARTUP_INNER(hook_startup_, NAME)
#define HOOK_FUNCTION_SHUTDOWN(NAME) HOOK_SHUTDOWN_INNER(hook_shutdown_, NAME)

#define CARM95_HOOK_FUNCTION(ORIGINAL, FUNCTION)                      \
    HOOK_FUNCTION_STARTUP(FUNCTION) {                                 \
        hook_function_register((void**)&ORIGINAL, (void*)FUNCTION, #ORIGINAL, #FUNCTION);   \
    }                                                                 \
    HOOK_FUNCTION_SHUTDOWN(FUNCTION) {                                \
        hook_function_deregister((void**)&ORIGINAL, (void*)FUNCTION); \
    }

#define CARM95_HOOK_FUNCTION_ADDR(ADDR, ORIGINAL, FUNCTION)                      \
    HOOK_FUNCTION_STARTUP(FUNCTION) {                                            \
        *((void**)&ORIGINAL) = (void*)ADDR;                                                  \
        hook_function_register((void**)&ORIGINAL, (void*)FUNCTION, #ORIGINAL, #FUNCTION);   \
    }                                                                 \
    HOOK_FUNCTION_SHUTDOWN(FUNCTION) {                                \
        hook_function_deregister((void**)&ORIGINAL, (void*)FUNCTION); \
    }

#define HV(N) (*HOOK_JOIN2(hookvar_,N))

void hook_register(void (*function)(void));
void hook_function_register(void **victim, void *detour, const char* victimname, const char* detourname);
void hook_function_deregister(void **victim, void *detour);

void hook_run_functions(void);
void hook_apply_all(void);
void hook_unapply_all(void);
void hook_check(void);

void hook_print_stats(void);

#if defined(__cplusplus)
}
#endif
