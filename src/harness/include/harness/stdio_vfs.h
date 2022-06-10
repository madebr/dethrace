#ifndef HARNESS_STDIO_VFS_H
#define HARNESS_STDIO_VFS_H

#if defined(DETHRACE_VFS)
#include "vfs.h"
#endif

// Include stdio.h first to make sure we don't mess with its defines
#include <stdio.h>

#if defined(DETHRACE_VFS)

// define this macro to detect aliasing
#define dethrace_stdio_vfs_aliased 1

#define access VFS_access

#define chdir VFS_chdir

#define fopen VFS_fopen

#define fclose VFS_fclose

#define fseek VFS_fseek

#define fprintf VFS_fprintf

#define vfprintf VFS_vfprintf

#define fscanf VFS_fscanf

#define fread VFS_fread

#define fwrite VFS_fwrite

#define feof VFS_feof

#define ftell VFS_ftell

#define rewind VFS_rewind

#define fgetc VFS_fgetc

#define ungetc VFS_ungetc

#define fgets VFS_fgets

#define fputc VFS_fputc

#define fputs VFS_fputs

#define remove VFS_remove

#define rename VFS_rename

#define OS_OpenDir VFS_OpenDir

#define OS_GetNextFileInDirectory VFS_GetNextFileInDirectory

#endif

#endif
