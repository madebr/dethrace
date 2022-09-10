#!/usr/bin/env python
import enum
import itertools
import json
import logging
import os
from pathlib import Path
import shutil
import textwrap

from dumpparser import *
from codegen2 import *


logger = logging.getLogger(__name__)


class AddressDatabase:
    def get_function_address(self, name: str) -> Optional[int]:
        return None

    def get_data_address(self, name: str) -> Optional[int]:
        return None


class InjectorGenerator(DethRaceProjectGenerator):

    def __init__(self, address_db: AddressDatabase, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.address_db = address_db

    def _generate_carm95_hooks(self):
        with (self.basepath / "include/carm95_hooks.h").open("w") as f:
            f.write(textwrap.dedent(r"""
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
                        hook_function_register((void**)&ORIGINAL, (void*)FUNCTION);   \
                    }                                                                 \
                    HOOK_FUNCTION_SHUTDOWN(FUNCTION) {                                \
                        hook_function_deregister((void**)&ORIGINAL, (void*)FUNCTION); \
                    }
                
                #define HV(N) (*HOOK_JOIN2(hookvar_,N))
                    
                void hook_register(void (*function)(void));
                void hook_function_register(void **victim, void *detour);
                void hook_function_deregister(void **victim, void *detour);
                
                void hook_run_functions(void);
                void hook_apply_all(void);
                void hook_unapply_all(void);
                void hook_check(void);
    
                void hook_print_stats(void);
                
                #if defined(__cplusplus)
                }
                #endif
            """))
            with (self.basepath / "carm95_hooks.cpp").open("w") as f:
                f.write(textwrap.dedent(r"""
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
                        std::function<void(void)> callback;
                    } hook_function_information;
                    
                    static std::vector<std::function<void(void)>> hook_startups;
                    static std::vector<hook_function_information> hook_function_startups;
                    static std::vector<hook_function_information> hook_function_shutdowns;
                    
                    void hook_register(void (*function)(void)) {
                        hook_startups.push_back(function);
                    }
                    
                    void hook_function_register(void **victim, void *detour) {
                        hook_function_startups.push_back({
                            victim,
                            *victim,
                            detour,
                            [=]() { DetourAttach(victim, detour); },
                        });
                    }
                    
                    void hook_function_deregister(void **victim, void *detour) {
                        hook_function_shutdowns.push_back({
                            victim,
                            *victim,
                            detour,
                            [=]() { DetourDetach(victim, detour); },
                        });
                    }
                    
                    void hook_run_functions(void) {
                        for (const auto &hook_startup : hook_startups) {
                            hook_startup();
                        }
                    }
                    
                    void hook_apply_all(void) {
                        for (const auto &info : hook_function_startups) {
                            info.callback();
                        }
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
                        for (const auto &info : hook_function_shutdowns) {
                            info.callback();
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
                """))
            with (self.basepath / "hookmain.c").open("w") as f:
                f.write(textwrap.dedent(r"""
                    #include "carm95_hooks.h"
                    
                    #include <windows.h>
                    #include <detours.h>
                    
                    #include <stdio.h>
                    
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
                        if (DetourIsHelperProcess()) {
                            return TRUE;
                        }
                    
                        if (dwReason == DLL_PROCESS_ATTACH) {
                            AllocConsole();
                    
                            SetConsoleTitle(TEXT(CONSOLE_TITLE));
                            freopen("CONIN$", "r", stdin);
                            freopen("CONOUT$", "w", stdout);
                            freopen("CONOUT$", "w", stderr);
                    
                            DetourRestoreAfterWith();
                    
                            DetourTransactionBegin();
                    
                            DetourUpdateThread (GetCurrentThread());
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
                    
                    #if defined(HOOK_INIT_FUNCTION)
                            HOOK_INIT_FUNCTION();
                    #endif
                        } else if (dwReason == DLL_PROCESS_DETACH) {
                    #if defined(HOOK_DEINIT_FUNCTION)
                            HOOK_DEINIT_FUNCTION();
                    #endif
                    
                            DetourTransactionBegin();
                    
                            hook_unapply_all();
                            DetourUpdateThread (GetCurrentThread());
                    
                            DetourTransactionCommit();
                        }
                        return TRUE;
                    }

                """))
            with (self.basepath / "injector.cpp").open("w") as f:
                f.write(textwrap.dedent(r"""
                    #include <windows.h>
                    #include <detours.h>
                    #include <filesystem>
                    #include <cstdio>
                    #include <string>
                    #include <vector>
                    
                    namespace fs = std::filesystem;
                    
                    int CDECL main()
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
                            printf("%s does not exist.\n", victimPath.c_str());
                            return 1;
                        }
                    
                        std::vector<std::string> pluginPaths;
                    
                        printf("plugins:\n");
                        for (auto pluginItem : fs::directory_iterator(pluginsDirPath)) {
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
                    
                        //////////////////////////////////////////////////////////////////////////
                        STARTUPINFOA si;
                        PROCESS_INFORMATION pi;
                        std::string command;
                    
                        ZeroMemory(&si, sizeof(si));
                        ZeroMemory(&pi, sizeof(pi));
                        si.cb = sizeof(si);
                    
                        command = victimPath;
                    #if defined(ARGUMENTS)
                        command += " ";
                        command += ARGUMENTS;
                    #endif
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
                """))

        with (self.basepath / "hook_msvc.def").open("w") as f:
            f.write(textwrap.dedent(r"""
            EXPORTS
                DetourFinishHelperProcess @1 NONAME
            """))

        with (self.basepath / "hook_mingw.def").open("w") as f:
            f.write(textwrap.dedent(r"""
                EXPORTS
                    DetourFinishHelperProcess@16 @1 NONAME
            """))

    def _get_original_symbol_name(self, function: DumpedFunction, name: str) -> str:
        return "original_" + name

    def get_source_extra(self, module: DumpedModule) -> str:
        s  = "#include \"harness/trace.h\"\n\n"
        s += "#include \"carm95_hooks.h\"\n\n"
        if any(part in module.name for part in ("stdfile", )):
            s += "#include <stdio.h>\n"
        if any(part in module.name for part in ("pc-dos", )):
            s += "#include <sys/stat.h>\n"
        return s

    def get_c_function_prefix(self, function: DumpedFunction, name: Optional[str]) -> str:
        original_function_address = self.address_db.get_function_address(name or function.name)

        s = ""
        if original_function_address:
            if function.is_vararg():
                name = function.name
                function_type_str = self.formatter.type_to_str(function.function_type)
                original_symbol_name = name
                original_symbol_decl = self.formatter.type_name_to_str(function.function_type, original_symbol_name)

                s += original_symbol_decl + f" = ({function_type_str})0x{original_function_address:08x};\n"
            else:
                name = name or function.name
                function_type_str = self.formatter.type_to_str(function.function_type)
                original_symbol_name = self._get_original_symbol_name(function, name)
                original_symbol_decl = self.formatter.type_name_to_str(function.function_type, original_symbol_name)

                s += "static " + original_symbol_decl + f" = ({function_type_str})0x{original_function_address:08x};\n"
                s += f"CARM95_HOOK_FUNCTION({original_symbol_name}, {name})\n"

        return s

    def get_h_global_variable_extern_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        s = ""
        address = self.address_db.get_data_address(name or gv.name)
        var_s = ""
        if not address:
            var_s += "// "
        var_s += f"extern {self.formatter.pointer_type_name_to_str(gv.kind, 'hookvar_' + (name or gv.name))};"
        if address:
            var_s += f" // addr: {address:08X}"
        if "struct" not in var_s and "{" not in var_s:
            s += var_s
            if name:
                s += " // suffix added to avoid duplicate symbol"
            s += "\n"
        return s

    def get_c_global_variable_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        s = ""
        address = self.address_db.get_data_address(name or gv.name)
        if name:
            s += " // Suffix added to avoid duplicate symbol\n"
        if not address:
            s += "#if 0\n"
        s += f"{self.formatter.pointer_type_name_to_str(gv.kind, 'hookvar_' + (name or gv.name))}"
        if address:
            s += f" = (void*)0x{address:08x}"
        s += ";\n"
        if not address:
            s += "#endif\n"
        return s

    def get_h_function_prefix(self, function: DumpedFunction, name: Optional[str]) -> str:
        original_function_address = self.address_db.get_function_address(function.name)

        s = ""
        if original_function_address and function.is_vararg():
            name = function.name
            function_type_str = self.formatter.type_to_str(function.function_type)
            if function.is_vararg():
                original_symbol_name = name
                original_symbol_decl = self.formatter.type_name_to_str(function.function_type, original_symbol_name)

                s += "extern " + original_symbol_decl + f";\n"
        return s

    def get_function_body(self, function: DumpedFunction, name: Optional[str], depth: int) -> str:
        prefix = " " * self.indentation * depth
        original_function_address = self.address_db.get_function_address(name or function.name)

        s = ""
        s = prefix + self._function_to_logtrace(function) + ";\n"
        s += "\n"

        for arg in function.arguments():
            s += prefix + "(void)" + arg.name + ";\n"
        for arg in function.local_variables():
            s += prefix + "(void)" + arg.name + ";\n"
        s += "\n"

        if original_function_address is not None and not function.is_vararg():
            name = name or function.name
            original_symbol_name = self._get_original_symbol_name(function, name)
            retstr = ""
            if type(function.return_type()) != DumpedVoidType:
                retstr = "return "
            s += prefix + retstr + f"{original_symbol_name}(" + ", ".join(arg.name for arg in function.arguments()) + ");\n"
        else:
            s += prefix + "NOT_IMPLEMENTED();\n"
        return s

    def modify_symbol_name(self, name: str, module: DumpedModule) -> Optional[str]:
        for fn in module.iter_functions():
            if fn.name == name:
                if fn.is_vararg():
                    return name + "_do_not_use"
                break
        return super().modify_symbol_name(name, module)

    def is_function_ok_to_add(self, fn: DumpedFunction) -> bool:
        if fn.name in ("CriticalISR", "matherr", "LoopLimitTooLow", ):
            return False
        return True

    def _generate_cmakelists(self):
        sources = []
        for module in self.modules:
            if "BRSRC" in module.name or "DETHRACE" in module.name or "pc-dos" in module.name:
                sources.extend((
                    str(Path(self.module_to_filename(module, ".c")).relative_to(self.basepath)).replace("\\", "/"),
                    str(Path(self.module_to_filename(module, ".h")).relative_to(self.basepath)).replace("\\", "/"),
                ))
        with (self.basepath / "CMakeLists.txt").open("w") as f:
            f.write(textwrap.dedent("""
                cmake_minimum_required(VERSION 3.15)
                project(carm95hooks C CXX)
                
                include(FetchContent)
                FetchContent_Declare(
                  Detours
                  GIT_REPOSITORY https://github.com/microsoft/detours.git
                  GIT_TAG        734ac64899c44933151c1335f6ef54a590219221
                )
                FetchContent_Populate(Detours)
                file(GLOB DETOURS_SOURCES "${detours_SOURCE_DIR}/src/*.cpp")
                list(REMOVE_ITEM DETOURS_SOURCES
                    "${detours_SOURCE_DIR}/src/uimports.cpp"
                )
                file(GLOB DETOURS_HEADERS "${detours_SOURCE_DIR}/src/*.h")
                add_library(detours STATIC ${DETOURS_SOURCES} ${DETOURS_HEADERS})
                target_include_directories(detours INTERFACE "$<BUILD_INTERFACE:${detours_SOURCE_DIR}/src>")
                set_target_properties(detours
                    PROPERTIES
                        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                )
                if(MINGW)
                    target_compile_options(detours PRIVATE -w)
                endif()
                add_library(Detours::Detours ALIAS detours)

                add_library(carm95hooks SHARED
                    hookmain.c
                    include/carm95_hooks.h
                    carm95_hooks.cpp
                    dummy.c
                    harness_trace.c
            """))
            for source in sources:
                f.write(f"    {source}\n")
            f.write(textwrap.dedent(r"""
                )
                target_compile_definitions(carm95hooks PRIVATE
                    PLUGIN_NAME="carm95hooks"
                )
                target_include_directories(carm95hooks PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/BRSRC13
                   ${CMAKE_CURRENT_SOURCE_DIR}/DETHRACE/source
                   ${CMAKE_CURRENT_SOURCE_DIR}/include
                   ${CMAKE_CURRENT_SOURCE_DIR}/include/harness
                   ${CMAKE_CURRENT_SOURCE_DIR}/include/brender
                )
                target_link_libraries(carm95hooks PRIVATE
                    Detours::Detours
                )
                set_target_properties(carm95hooks
                    PROPERTIES
                        PREFIX ""
                        OUTPUT_NAME carm95hooks
                        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
                        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                )
                if(MSVC)
                    target_sources(carm95hooks PRIVATE hook_msvc.def)
                elseif(MINGW)
                    target_sources(carm95hooks PRIVATE hook_mingw.def)
                endif()
                
                add_executable(carm95_injector injector.cpp)
                target_compile_definitions(carm95_injector PRIVATE
                    TARGET="carm95_injector"
                    VICTIM="CARM95.EXE"
                    
                )
                target_link_libraries(carm95_injector PRIVATE
                    Detours::Detours
                )
                set_target_properties(carm95_injector
                    PROPERTIES
                        CXX_STANDARD 17
                        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                )
                
                set(CARM95_LOCATION "" CACHE FILEPATH "Path of CARM95.EXE (+game data)")
                if(CARM95_LOCATION)
                    add_custom_target(copy_hooks
                        COMMENT "Copying carm95 hooks to \"${CARM95_LOCATION}\""
                        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CARM95_LOCATION}/plugins"
                        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:carm95hooks>" "${CARM95_LOCATION}/plugins/$<TARGET_FILE_NAME:carm95hooks>"
                        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:carm95_injector>" "${CARM95_LOCATION}/$<TARGET_FILE_NAME:carm95_injector>"
                    )
                endif()
            """))

    def generate(self):
        mkdir_p(self.basepath)
        mkdir_p(self.basepath / "include/brender")
        mkdir_p(self.basepath / "include/harness")
        mkdir_p(self.basepath / "include/s3")
        with (self.basepath / "dummy.c").open("w") as f:
            f.write("int OS_IsDebuggerPresent(void) { return 1; }\n")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/harness/include/harness/trace.h", self.basepath / "include/harness")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/harness/harness_trace.c", self.basepath)
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/brender.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_defs.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_inline_funcs.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_types.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/constants.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/dr_types.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/macros.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/S3/include/s3/s3.h", self.basepath / "include/s3")
        self._generate_carm95_hooks()
        self._generate_cmakelists()
        self._generate()
        for element in self.basepath.iterdir():
            if element.is_dir():
                if element.name not in ("include", "BRSRC13", "DETHRACE"):
                    shutil.rmtree(element)


class JSONAddressDatabase(AddressDatabase):
    def __init__(self, symbols):
        self.symbols = symbols

    def get_function_address(self, name: str) -> Optional[int]:
        for item in self.symbols["functions"]:
            if item["name"] == name:
                return item["offset"]
        return None

    def get_data_address(self, name: str) -> Optional[int]:
        for item in self.symbols["datas"]:
            if item["label"] == name:
                return item["offset"]
        return None

    @classmethod
    def from_exported_json(cls, path: Path) -> "GhidraAddressFile":
        with open(path, "r") as f:
            data = json.load(f)
        return cls(data)


class IDAAddressDatabase(AddressDatabase):
    def __init__(self, func_map: dict[str, int], gv_map: dict[str, int]):
        self.func_map = func_map
        self.gv_map = gv_map

    def get_function_address(self, name: str) -> Optional[int]:
        return self.func_map.get(name)

    def get_data_address(self, name: str) -> Optional[int]:
        return self.gv_map.get(name)

    @classmethod
    def from_exported_map(cls, path: Path) -> "IDAAddressFile":
        func_offset = 0x00401000
        gv_offset = 0x00507000

        func_map = {}
        gv_map = {}
        with path.open() as f:
            while True:
                line = f.readline()
                if line == "":
                    break

                line = line.strip()
                if line.startswith("0001:"):
                    parts = [s for s in line[5:].split(" ") if s]
                    # map function name to virtual address
                    func_map[parts[1]] = int(parts[0], 16) + func_offset
                elif line.startswith("0003:"):
                    parts = [s for s in line[5:].split(" ") if s]

                    # skip over most autogenerated stuff
                    if parts[1].startswith("g"):
                        # map global variable name to virtual address
                        gv_map[parts[1]] = int(parts[0], 16) + gv_offset
        return cls(func_map, gv_map)


def main():
    import argparse
    import sys

    logging.basicConfig(format="%(levelname)s: %(message)s")

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("dumpfile", type=Path, help="Path to output of wdump")
    parser.add_argument("addressfile", type=Path, help="Path to file containing symbol addresses")
    parser.add_argument("base", default=Path("_generated"), type=Path, nargs="?", help="Path where to generate the symbols")
    parser.add_argument("--type", choices=("json", "ida"), default="ida", help="Symbol type file")
    parser.add_argument("--indent", default=4, type=int, help="Indentation of generated code")
    parser.add_argument("--no-clear", dest="clear", action="store_false", help="Don't remove base folder")
    parser.add_argument("-v", dest="verbose", action="store_true", help="Verbose")
    args = parser.parse_args()

    if args.verbose:
        logging.root.setLevel(logging.INFO)

    args.base = args.base.resolve()

    if args.type == "json":
        address_db = JSONAddressDatabase.from_exported_json(args.addressfile)
    elif args.type == "ida":
        address_db = IDAAddressDatabase.from_exported_map(args.addressfile)
    else:
        raise NotImplementedError

    formatter = CFormatter(
        indentation=args.indent,
        functions_vararg=True,
    )

    modules = list(DumpedModuleReader.open(args.dumpfile).read_modules())

    project_generator = InjectorGenerator(
        address_db=address_db,
        modules=modules,
        basepath=args.base,
        indentation=args.indent,
        formatter=formatter,
    )

    if args.clear:
        project_generator.clear()

    project_generator.generate()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
