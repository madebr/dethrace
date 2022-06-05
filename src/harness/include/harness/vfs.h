#ifndef HARNESS_VFS_VFS_H
#define HARNESS_VFS_VFS_H

#include <stdarg.h>
#include <stdlib.h>

#ifdef DETHRACE_VFS

typedef struct VFILE VFILE;
typedef struct vfs_diriter vfs_diriter;

int VFS_Init(const char* path);

int VFS_access(const char* path, int mode);

int VFS_chdir(const char* path);

VFILE* VFS_fopen(const char* path, const char* mode);

int VFS_fclose(VFILE* stream);

int VFS_fseek(VFILE* stream, long offset, int whence);

int VFS_fprintf(VFILE* stream, const char* format, ...);

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap);

int VFS_fscanf(VFILE* stream, const char* format, ...);

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, VFILE* stream);

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, VFILE* stream);

int VFS_feof(VFILE* stream);

long VFS_ftell(VFILE* stream);

void VFS_rewind(VFILE* stream);

int VFS_fgetc(VFILE* stream);

int VFS_ungetc(int c, VFILE* stream);

char* VFS_fgets(char* s, int size, VFILE* stream);

int VFS_fputc(int c, VFILE* stream);

int VFS_fputs(const char* s, VFILE* stream);

int VFS_remove(const char* pathname);

int VFS_rename(const char* oldpath, const char* newpath);

vfs_diriter* VFS_OpenDir(char* path);

char* VFS_GetNextFileInDirectory(vfs_diriter* diriter);

#else

#define VFILE FILE
#define vfs_diriter os_diriter

// FIXME: implement this function
int VFS_Init(const char* path);

#define VFS_access access

#define VFS_chdir chdir

#define VFS_fopen fopen

#define VFS_fclose fclose

#define VFS_fseek fseek

#define VFS_fprintf fprintf

#define VFS_vfprintf vfprintf

#define VFS_fscanf fscanf

#define VFS_fread fread

#define VFS_fwrite fwrite

#define VFS_feof feof

#define VFS_ftell ftell

#define VFS_rewind rewind

#define VFS_fgetc fgetc

#define VFS_ungetc ungetc

#define VFS_fgets fgets

#define VFS_fputc fputc

#define VFS_fputs fputs

#define VFS_remove remove

#define VFS_rename rename

#define VFS_OpenDir OS_OpenDir

#define VFS_GetNextFileInDirectory OS_GetNextFileInDirectory

#endif

#endif // HARNESS_VFS_VFS_H
