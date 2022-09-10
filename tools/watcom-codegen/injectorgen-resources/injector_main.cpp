#include "injector.h"

#include <windows.h>
#include <detours.h>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

#define ASIZE(V) (sizeof(V) / sizeof((V)[0]))

static const char* reference_sha256_hashes[] = {
    "9028120f4e3fd84b790fed1cbc074d2d7a547fed49cfdaec60a6bb0f28c45de2",
    "c6040203856b71e6a22d2a29053a1eadd1a2ab41bce97b6031d745079bc07bdf",
};

namespace fs = std::filesystem;

int CDECL main(int argc, char *argv[])
{
#if 0
    auto workPathDir = fs::current_path();
#else
    char exePath[512];
    DWORD nLen = GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    if (nLen == sizeof(exePath)) {
        return 1;
    }
    exePath[nLen] = '\0';
    auto workPathDir = fs::path(exePath).parent_path();
#endif
    const auto workPathDirStr = workPathDir.generic_string();
    const auto pluginsDirPath = workPathDir / "plugins";

    const auto victimPath = (workPathDir / VICTIM).generic_string();
    if (!fs::is_regular_file(victimPath)) {
        fprintf(stderr, "%s does not exist.\n", victimPath.c_str());
        return 1;
    }

    char *sha256 = hook_calculate_sha256(victimPath.c_str());
    int valid = 0;
    for (int i = 0; i < ASIZE(reference_sha256_hashes); i++) {
        if (stricmp(sha256, reference_sha256_hashes[i]) == 0) {
            valid = 1;
            break;
        }
    }
    if (!valid) {
        fprintf(stderr, "SHA256 hash of \"%s\" is not in database (%s).\n", victimPath, sha256);
        return 1;
    }
    free(sha256);

    std::vector<std::string> pluginPaths;

    printf("plugins:\n");
    if (fs::is_directory(pluginsDirPath)) {
        for (auto pluginItem: fs::directory_iterator(pluginsDirPath)) {
            if (!pluginItem.exists() || !pluginItem.is_regular_file()) {
                continue;
            }
            auto pluginPath = pluginItem.path();
            if (pluginPath.extension() != ".dll") {
                continue;
            }
            auto pluginPathStr = pluginPath.generic_string();
            pluginPaths.push_back(pluginPathStr);
            printf("- %s\n", pluginPaths.back().c_str());
        }
    }

    //////////////////////////////////////////////////////////////////////////
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    std::string command;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    command = victimPath;

    for (int i = 1; i < argc; i++) {
        command += " ";
        command += argv[i];
    }
    char* commandBuffer = new char[command.size() + 1];
    command.copy(commandBuffer, command.size());
    commandBuffer[command.size()] = '\0';

    std::vector<const char*> pluginPathsCStrs;
    pluginPathsCStrs.resize(pluginPaths.size());
    std::transform(pluginPaths.begin(), pluginPaths.end(), pluginPathsCStrs.begin(), [](const auto &str) { return str.data(); });

    printf(TARGET ": Starting: `%s'\n", command.c_str());
    for (const auto& pluginPath : pluginPaths) {
        printf(TARGET ":   with `%s'\n", pluginPath.c_str());
    }
    fflush(stdout);

    DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

    SetLastError(0);
    if (!DetourCreateProcessWithDllsA(
            victimPath.c_str(),
            commandBuffer,
            nullptr,
            nullptr,
            TRUE,
            dwFlags,
            nullptr,
            workPathDirStr.c_str(),
            &si,
            &pi,
            pluginPathsCStrs.size(),
            pluginPathsCStrs.data(),
            nullptr)) {
        DWORD dwError = GetLastError();
        printf(TARGET ": DetourCreateProcessWithDllEx failed: %ld\n", dwError);
        printf("ERROR");
        ExitProcess(9009);
    }

    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD dwResult = 0;
    if (!GetExitCodeProcess(pi.hProcess, &dwResult)) {
        printf(TARGET ": GetExitCodeProcess failed: %ld\n", GetLastError());
    }

    return dwResult;
}
