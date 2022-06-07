#include "harness/vfs.h"

#include "harness/trace.h"

#include <physfs.h>
#include <unistd.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct VFILE {
    PHYSFS_File *file;
    char ungetc_char;
    char ungetc_valid;
} VFILE;

typedef struct vfs_diriter {
    char** list;
    size_t index;
} vfs_diriter;

static VFILE* create_VFILE(PHYSFS_File* file) {
    if (file == NULL) {
        return NULL;
    }
    VFILE* vfile = malloc(sizeof(VFILE));
    vfile->file = file;
    vfile->ungetc_valid = 0;
    return vfile;
}

static int getc_VFILE(VFILE* stream) {
    char c;
    PHYSFS_sint64 result;

    if (stream->ungetc_valid) {
        stream->ungetc_valid = 0;
        return stream->ungetc_char;
    }
    result = PHYSFS_readBytes(stream->file, &c, 1);
    if (result == 1) {
        return c;
    }
    return EOF;
}

int VFS_Init(int argc, const char* argv[], const char* paths) {
    int result;

    result = PHYSFS_init(argv[0]);
    if (result == 0) {
        PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
        LOG_WARN("PHYSFS_init failed with %d (%s)", ec, PHYSFS_getErrorByCode(ec));
        return 1;
    }
    if (paths == NULL) {
        LOG_INFO("DETHRACE_ROOT_DIR is not set, assuming '.'");
        paths = ".";
    }
    const char* currentPath = paths;
    char pathBuffer[260];
    while (currentPath != NULL) {
        char* endPos = strchr(currentPath, ':');
        size_t pathLen;
        if (endPos == NULL) {
            pathLen = strlen(currentPath);
            strncpy(pathBuffer, currentPath, pathLen);
            currentPath = NULL;
        } else {
            pathLen = endPos - currentPath;
            strncpy(pathBuffer, currentPath, pathLen);
            currentPath += pathLen + 1;
            if (*currentPath == '\0') {
                currentPath = NULL;
            }
        }
        pathBuffer[pathLen] = '\0';
        result = PHYSFS_mount(pathBuffer, NULL, 1);
        if (result == 0) {
            PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
            LOG_WARN("PHYSFS_mount(\"%s\", NULL, 1) failed with %d (%s)", ec, PHYSFS_getErrorByCode(ec));
            continue;
        }
        LOG_INFO("VFS search path: %s", pathBuffer);
    }
    return 0;
}

vfs_diriter* VFS_OpenDir(char* path) {
    char** list;
    vfs_diriter* diriter;

    list = PHYSFS_enumerateFiles(path);
    if (list == NULL) {
        return NULL;
    }
    diriter = malloc(sizeof(vfs_diriter));
    diriter->list = list;
    diriter->index = 0;
    return diriter;
}

char* VFS_GetNextFileInDirectory(vfs_diriter* diriter) {
    char* result;

    result = diriter->list[diriter->index];
    diriter->index++;
    if (result == NULL) {
        free(diriter);
    }
    return result;
}

int VFS_access(const char* path, int mode) {
    PHYSFS_Stat stat;
    int result;

    result = PHYSFS_stat(path, &stat);
    if (result == 0) {
        return -1;
    }
    if ((mode & W_OK) != 0) {
        if (stat.readonly) {
            return -1;
        }
    }
    return 0;
}

int VFS_chdir(const char* path) {
    NOT_IMPLEMENTED();
}

VFILE* VFS_fopen(const char* path, const char* mode) {
    if (strncmp(path, "./", 2) == 0) {
        path += 2;
    }
    if (strchr(mode, 'r') != NULL) {
        return create_VFILE(PHYSFS_openRead(path));
    }
    if (strchr(mode, 'w') != NULL) {
        return create_VFILE(PHYSFS_openWrite(path));
    }
    if (strchr(mode, 'a') != NULL) {
        return create_VFILE(PHYSFS_openAppend(mode));
    }
    return NULL;
}

int VFS_fclose(VFILE* stream) {
    int result;
    result = PHYSFS_close(stream->file);
    free(stream);
    return result != 0 ? 0 : EOF;
}

int VFS_fseek(VFILE* stream, long offset, int whence) {
    int result;
    switch (whence) {
    case SEEK_SET:
        result = PHYSFS_seek(stream->file, offset);
        break;
    case SEEK_CUR:
        result = PHYSFS_seek(stream->file, PHYSFS_tell(stream->file) + offset);
        break;
    case SEEK_END:
        result = PHYSFS_seek(stream->file, PHYSFS_fileLength(stream->file) + offset);
        break;
    default:
        return -1;
    }
    return result != 0 ? 0 : -1;
}

int VFS_fprintf(VFILE* stream, const char* format, ...) {
    NOT_IMPLEMENTED();
}

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap) {
    NOT_IMPLEMENTED();
}

size_t vfs_scanf_marker_internal;

int VFS_fscanf_internal(VFILE* stream, const char* format, ...) {
    PHYSFS_sint64 location;
    char buf[256];
    char format_modified[256];
    va_list ap;
    int nb;

    strcpy(format_modified, format);
    strcat(format_modified, "%n");

    location = PHYSFS_tell(stream->file);
    PHYSFS_readBytes(stream->file, buf, sizeof(buf));
    va_start(ap, format);
    nb = vsscanf(buf, format_modified, ap);
    va_end(ap);
    PHYSFS_seek(stream->file, location + vfs_scanf_marker_internal);

    return nb;
}

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    size_t nb_items;
    PHYSFS_sint64 location;
    PHYSFS_sint64 actualRead;

    nb_items = 0;
    while (nb_items < nmemb) {
        location = PHYSFS_tell(stream->file);
        actualRead = PHYSFS_readBytes(stream->file, ptr, size);
        if ((size_t)actualRead != size) {
            PHYSFS_seek(stream->file, location);
            break;
        }
        nb_items++;
        ptr = &((char*)ptr)[size];
    }
    return nb_items;
}

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_feof(VFILE* stream) {

    return PHYSFS_eof(stream->file);
}

long VFS_ftell(VFILE* stream) {

    return PHYSFS_tell(stream->file);
}

void VFS_rewind(VFILE* stream) {

    PHYSFS_seek(stream->file, 0);
}

int VFS_fgetc(VFILE* stream) {

    return getc_VFILE(stream);
}

int VFS_ungetc(int c, VFILE* stream) {

    if (stream->ungetc_valid) {
        return EOF;
    }
    stream->ungetc_char = c;
    stream->ungetc_valid = 1;
    return c;
}

char* VFS_fgets(char* s, int size, VFILE* stream) {
    int c;
    PHYSFS_uint64 count;

    if (size <= 0) {
        return NULL;
    }

    count = 0;

    while (1) {
        if (count + 1 >= (PHYSFS_uint64)size) {
            break;
        }
        c = getc_VFILE(stream);
        if (c == EOF) {
            break;
        } else if (c == '\n') {
            s[count] = (char)c;
            count++;
            break;
        } else {
            s[count] = (char)c;
            count++;
        }
    }
    if (count <= 0) {
        return NULL;
    }
    s[count] = '\0';
    return s;
}

int VFS_fputc(int c, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_fputs(const char* s, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_remove(const char* pathname) {
    NOT_IMPLEMENTED();
}

int VFS_rename(const char* oldpath, const char* newpath) {
    NOT_IMPLEMENTED();
}
