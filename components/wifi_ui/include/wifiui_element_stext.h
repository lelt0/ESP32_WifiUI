#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct {
    wifiui_element_t common;
    const char* text;
} wifiui_element_stext_t;

const wifiui_element_stext_t * wifiui_element_static_text(const char* text);

#ifdef __cplusplus
}
#endif
