#pragma once
#include <stdio.h>

extern void(__cdecl*c95_fclose)(FILE*);
extern void(__cdecl*c95_fprintf)(FILE*,const char*,...);
extern void(__cdecl*c95_rewind)(FILE*);
