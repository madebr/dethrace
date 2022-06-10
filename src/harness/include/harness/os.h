#ifndef HARNESS_OS_H
#define HARNESS_OS_H

#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <io.h>
#define getcwd _getcwd
#define chdir _chdir
#define access _access
#define F_OK 0
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#if _MSC_VER < 1900
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

#else
#include <unistd.h>
#endif

#ifdef dethrace_stdio_vfs_aliased
#error "stdio functions aliased to VFS functions"
#endif

typedef struct os_diriter os_diriter;

// Required: return timestamp in milliseconds.
uint32_t OS_GetTime(void);

// Required: sleep for specified milliseconds
void OS_Sleep(int ms);

// Required: return true if path is a directory on the filesystem
int OS_IsDirectory(const char* path);

// Required: begin a directory iteration
os_diriter* OS_OpenDir(char* path);

// Required: continue directory iteration. If no more files, return NULL
char* OS_GetNextFileInDirectory(os_diriter* diriter);

// Required: copy the `basename` component of `path` into `base`
void OS_Basename(char* path, char* base);

// Optional: return true if a debugger is detected
int OS_IsDebuggerPresent(void);

// Optional: install a handler to print stack trace during a crash
void OS_InstallSignalHandler(char* program_name);

FILE* OS_fopen(const char* pathname, const char* mode);

#endif
