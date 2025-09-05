#pragma once

#ifdef __cpulusplus
extern "C" {
#endif

#include "stdint.h"
#include "memory.h"

typedef struct
{
    char* str;
    size_t len;
    size_t capacity;
    size_t unit_len;
} dstring_t;

dstring_t* dstring_create(size_t unit_length);
size_t dstring_appendf(dstring_t* dst, const char* format, ...);
void dstring_free(dstring_t* dstr);

#ifdef __cpulusplus
}
#endif