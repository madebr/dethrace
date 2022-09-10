
#include "carm95_hooks.h"
#include "carm95_webserver.h"

#include <windows.h>
#include <detours.h>

#include <stdio.h>

extern "C" {

#ifndef CONSOLE_TITLE
#define CONSOLE_TITLE "console"
#endif

#if defined(HOOK_INIT_FUNCTION)
void HOOK_INIT_FUNCTION(void);
#endif
#if defined(HOOK_DEINIT_FUNCTION)
void HOOK_DEINIT_FUNCTION(void);
#endif

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    (void)hinst;
    (void)reserved;
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        AllocConsole();

        SetConsoleTitle(TEXT(CONSOLE_TITLE));
#define WRITE_TO_CONSOLE(TEXT) WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), (TEXT), strlen(TEXT), NULL, NULL)
        if (freopen("CONIN$", "r", stdin) == NULL) {
            WRITE_TO_CONSOLE("Failed to re-open stdin\n");
        }
        if (freopen("CONOUT$", "w", stdout) == NULL) {
            WRITE_TO_CONSOLE("Failed to re-open stdout\n");
        }
        if (freopen("CONERR$", "w", stderr) == NULL) {
            WRITE_TO_CONSOLE("Failed to re-open sstderr\n");
        }

        DetourRestoreAfterWith();

        DetourTransactionBegin();

        DetourUpdateThread(GetCurrentThread());
        hook_apply_all();

        DetourTransactionCommit();

        hook_run_functions();

        hook_print_stats();
        hook_check();
        char pathBuffer[512];
        GetModuleFileNameA(NULL, pathBuffer, sizeof(pathBuffer));
        printf("executable: \"%s\"\n", pathBuffer);
        GetCurrentDirectoryA(sizeof(pathBuffer), pathBuffer);
        printf("directory: \"%s\"\n", pathBuffer);
        printf("=================================\n");

        start_hook_webserver(8080);

#if defined(HOOK_INIT_FUNCTION)
        HOOK_INIT_FUNCTION();
#endif
    } else if (dwReason == DLL_PROCESS_DETACH) {
#if defined(HOOK_DEINIT_FUNCTION)
        HOOK_DEINIT_FUNCTION();
#endif

        DetourTransactionBegin();

        hook_unapply_all();
        DetourUpdateThread(GetCurrentThread());

        DetourTransactionCommit();

        stop_hook_webserver();
    }
    return TRUE;
}

}
