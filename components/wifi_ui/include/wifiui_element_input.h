#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct {
    wifiui_element_t common;
    const char* button_label;
    void (*on_input)(char * input_str, void* param);
    void* on_input_param;
    bool clear_after_sent;
} wifiui_element_input_t;

const wifiui_element_input_t * wifiui_element_input(const char* button_label, void (*on_input_callback)(char*, void*), void* on_input_callback_param, bool clear_after_sent);

#ifdef __cplusplus
}
#endif
