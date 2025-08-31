#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_button wifiui_element_button_t;
typedef void (*onclick_callback_f)(const wifiui_element_button_t *, void*);
struct wifiui_element_button {
    wifiui_element_t common;
    char* label;
    onclick_callback_f onclick_callback;
    void* onclick_callback_param;
};

const wifiui_element_button_t * wifiui_element_button(const char* label, onclick_callback_f onclick_callback, void* onclick_callback_param);

#ifdef __cplusplus
}
#endif
