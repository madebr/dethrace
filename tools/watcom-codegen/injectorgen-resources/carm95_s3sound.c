#include <s3/s3.h>

#include "carm95_hooks.h"

static int(__cdecl*original_S3SetEffects)(tS3_sample_filter* filter1, tS3_sample_filter* ) = (int(__cdecl*)(tS3_sample_filter* filter1, tS3_sample_filter*))0x004c946d;
CARM95_HOOK_FUNCTION(original_S3SetEffects, S3SetEffects)
int __cdecl S3SetEffects(tS3_sample_filter* filter1, tS3_sample_filter* filter2) {
    return original_S3SetEffects(filter1, filter2);
}
