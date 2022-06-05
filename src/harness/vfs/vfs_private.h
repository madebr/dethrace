#ifndef HARNESS_VFS_PRIVATE_H
#define HARNESS_VFS_PRIVATE_H

#include "harness/vfs.h"

struct vfs_functions;
struct vfs_state;
struct vfs_diriter_handle;

struct vfs_functions {
    char* (*GetNextFileInDirectory)(struct vfs_state*, struct vfs_diriter_handle* diriter);
    struct vfs_diriter_handle* (*OpenDir)(struct vfs_state*, char* path);
    int (*access)(struct vfs_state*, const char* path, int mode);
    int (*chdir)(struct vfs_state*, const char* path);
    int (*fclose)(struct vfs_state*, VFILE* stream);
    int (*feof)(struct vfs_state*, VFILE* stream);
    int (*fgetc)(struct vfs_state*, VFILE* stream);
    char* (*fgets)(struct vfs_state*, char* s, int size, VFILE* stream);
    VFILE* (*fopen)(struct vfs_state*, const char* path, const char* mode);
    int (*fputs)(struct vfs_state*, const char* s, VFILE* stream);
    size_t (*fread)(struct vfs_state*, void* ptr, size_t size, size_t nmemb, VFILE* stream);
    int (*fseek)(struct vfs_state*, VFILE* stream, long offset, int whence);
    long (*ftell)(struct vfs_state*, VFILE* stream);
    void (*rewind)(struct vfs_state*, VFILE* stream);
    int (*ungetc)(struct vfs_state*, int c, VFILE* stream);
    int (*vfprintf)(struct vfs_state*, VFILE *stream, const char *format, va_list ap);
    int (*vfscanf)(struct vfs_state*, VFILE *stream, const char *format, va_list ap);
};

struct vfs_state {
    struct vfs_state* next;
    void* handle;
    const struct vfs_functions* functions;
};

typedef struct VFILE {
    struct vfs_state *state;
    void* handle;
} VFILE;

typedef struct vfs_diriter {
    struct vfs_state* current_state;
    struct vfs_diriter_handle* handle;
} vfs_diriter;

#endif  // HARNESS_VFS_PRIVATE_H
