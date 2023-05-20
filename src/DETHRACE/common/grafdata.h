#ifndef _GRAFDATA_H_
#define _GRAFDATA_H_

#include "brender/br_types.h"
#include "dr_types.h"

#if defined(DETHRACE_FIX_BUGS)
#define COUNT_GRAF_DATA 3
#else
#define COUNT_GRAF_DATA 2
#endif

extern tGraf_data gGraf_data[COUNT_GRAF_DATA];
extern tGraf_data* gCurrent_graf_data;
extern int gGraf_data_index;

void CalcGrafDataIndex(void);

#endif
