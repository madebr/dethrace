#ifndef HARNESS_VFS_VFS_H
#define HARNESS_VFS_VFS_H

#include "os.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(DETHRACE_VFS)
#error DethRace vfs is only available when built with DETHRACE_VFS
#endif

typedef struct vfs_diriter vfs_diriter;

int VFS_Init(int argc, const char* argv[], const char* paths);

void VFS_SetWriteDir(const char* path);

int VFS_access(const char* path, int mode);

int VFS_chdir(const char* path);

FILE* VFS_fopen(const char* path, const char* mode);

uint64_t VFS_filesize(FILE* stream);

char* VFS_internal_buffer(FILE* stream);

int VFS_fclose(FILE* stream);

int VFS_fseek(FILE* stream, long offset, int whence);

extern size_t vfs_fprintf_marker_internal;
#define VFS_fprintf(STREAM, FORMAT, ...) VFS_fprintf_internal((STREAM), FORMAT "%n", ##__VA_ARGS__, &vfs_fprintf_marker_internal)
int VFS_fprintf_internal(FILE* stream, const char* format, ...);

int VFS_vfprintf(FILE *stream, const char *format, va_list ap);

extern size_t vfs_scanf_marker_internal;
#define VFS_fscanf(STREAM, FORMAT, ...) VFS_fscanf_internal((STREAM), FORMAT "%n", ##__VA_ARGS__, &vfs_scanf_marker_internal)
int VFS_fscanf_internal(FILE* stream, const char* format, ...);

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, FILE* stream);

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, FILE* stream);

int VFS_feof(FILE* stream);

long VFS_ftell(FILE* stream);

void VFS_rewind(FILE* stream);

int VFS_fgetc(FILE* stream);

int VFS_ungetc(int c, FILE* stream);

char* VFS_fgets(char* s, int size, FILE* stream);

int VFS_fputc(int c, FILE* stream);

int VFS_fputs(const char* s, FILE* stream);

int VFS_remove(const char* pathname);

int VFS_rename(const char* oldpath, const char* newpath);

os_diriter* VFS_OpenDir(char* path);

char* VFS_GetNextFileInDirectory(os_diriter* diriter);

#endif // HARNESS_VFS_VFS_H
