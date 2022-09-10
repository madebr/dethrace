#include "carm95_hooks.h"

#include <windows.h>
#include <detours.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <vector>

extern "C" {

typedef struct hook_function_information {
    void** victim;
    void* original_victim;
    void* detour;
    const char* victimname;
    const char* detourname;
} hook_function_information;

static std::vector<std::function<void(void)>> hook_startups;
static std::vector<hook_function_information> hook_function_startups;
static std::vector<hook_function_information> hook_function_shutdowns;

void hook_register(void (*function)(void)) {
    hook_startups.push_back(function);
}

const char* detour_errcode_to_string(LONG code) {
    switch (code) {
        case ERROR_INVALID_BLOCK:
            return "ERROR_INVALID_BLOCK";
        case ERROR_INVALID_HANDLE:
            return "ERROR_INVALID_HANDLE";
        case ERROR_INVALID_OPERATION:
            return "ERROR_INVALID_OPERATION";
        case ERROR_NOT_ENOUGH_MEMORY:
            return "ERROR_NOT_ENOUGH_MEMORY";
        default:
            return "<unknown>";
    }
}

void hook_function_register(void **victim, void *detour, const char* victimname, const char* detourname) {
    hook_function_startups.push_back({
        victim,
        *victim,
        detour,
        victimname,
        detourname,
    });
}

void hook_function_deregister(void **victim, void *detour) {
    hook_function_shutdowns.push_back({
        victim,
        *victim,
        detour,
        NULL,
        NULL,
    });
}

void hook_run_functions(void) {
    for (const auto &hook_startup : hook_startups) {
        hook_startup();
    }
}

void hook_apply_all(void) {
    FILE* f = fopen("hook.log", "w");
    for (auto &info : hook_function_startups) {
        fprintf(f, "Hooking %s (0x%p) with %s: ", info.victimname, info.original_victim, info.detourname);
        fflush(f);
        LONG r = DetourAttach(info.victim, info.detour);
        if (r == NO_ERROR) {
            fprintf(f, "SUCCESS!\n");
        } else {
            fprintf(f, "ERROR! (code=%ld, txt=\"%s\")\n",
                    r, detour_errcode_to_string(r));
        }
        fflush(f);
    }
    fclose(f);
    f = NULL;
}

void hook_check(void) {
    bool problem = false;
    std::unordered_map<uintptr_t, const hook_function_information*> detected_original_victims;
    for (const auto &info : hook_function_startups) {
        auto already_in = detected_original_victims.find((uintptr_t)info.original_victim);
        if (already_in != detected_original_victims.end()) {
            fprintf(stderr, "Already found a function at 0x%p.\n", info.original_victim);
            problem = true;
        }
        detected_original_victims[(uintptr_t)info.original_victim] = &info;
    }
    if (problem) {
        abort();
    }
}

void hook_unapply_all(void) {
    for (auto &info : hook_function_shutdowns) {
        DetourDetach(info.victim, info.detour);
    }
}

void hook_print_stats(void) {
    printf("===================================================\n");
    printf("Plugin name: %s\n", PLUGIN_NAME);
    printf("Hook count: %d\n", (int)hook_startups.size());
    printf("Function hook startup count: %d\n", (int)hook_function_startups.size());
    printf("Function hook startup count: %d\n", (int)hook_function_shutdowns.size());
    printf("===================================================\n");
}

}
