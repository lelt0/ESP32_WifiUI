#include "dstring.h"
#include "stdio.h"
#include "stdarg.h"

static void dstr_realloc(dstring_t* dstr, size_t required_len);

dstring_t* dstring_create(size_t unit_length)
{
    dstring_t* dstr = (dstring_t*)malloc(sizeof(dstring_t));
    dstr->str = (char*)malloc(unit_length * 2);
    dstr->len = 0;
    dstr->capacity = unit_length * 2;
    dstr->unit_len = unit_length;
    return dstr;
}

size_t dstring_appendf(dstring_t* dst, const char* format, ...)
{
    va_list args_bakup;
    va_start(args_bakup, format);

    int len_tmp = 0;
    while(1) {
        va_list args;
        va_copy(args, args_bakup);
        len_tmp = dst->len + vsnprintf(dst->str + dst->len, dst->capacity - dst->len, format, args);
        va_end(args);
        if(len_tmp + 1 < dst->capacity) break;
        dstr_realloc(dst, len_tmp);
    }
    va_end(args_bakup);
    dst->len = len_tmp;

    return dst->len;
}

void dstring_free(dstring_t* dstr)
{
    free(dstr->str);
    free(dstr);
}

void dstr_realloc(dstring_t* dstr, size_t required_len)
{
    if(dstr->capacity > required_len + 1) return;

    int add_units = ((required_len + 1) - dstr->capacity) / dstr->unit_len + 1;
    dstr->capacity += dstr->unit_len * add_units;
    dstr->str = realloc(dstr->str, dstr->capacity);
}
