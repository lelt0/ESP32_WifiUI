#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_dtext wifiui_element_dtext_t;
struct wifiui_element_dtext {
    wifiui_element_t common;
    const char * text;
    char * (*change_text)(const wifiui_element_dtext_t* self, const char* new_text);
};

const wifiui_element_dtext_t * wifiui_element_dynamic_text(const char* init_text);

#ifdef __cplusplus
}
#endif
