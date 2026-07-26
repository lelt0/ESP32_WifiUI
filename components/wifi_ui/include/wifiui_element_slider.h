#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_slider wifiui_element_slider_t;
struct wifiui_element_slider {
    wifiui_element_t common;
    const char* label;
    float min;
    float max;
    float step;
    float current_value;
    const char* css_color;
    void (*on_changed)(float value);
    void (*set_value)(const wifiui_element_slider_t*, float value);
};

const wifiui_element_slider_t * wifiui_element_slider(
    const char* label,
    float min,
    float max,
    float step,
    float init_value,
    const char* color, // NULL:デフォルト色
    void (*on_changed_callback)(float) // NULL:UI操作不可
);
const wifiui_element_slider_t * wifiui_element_switch(
    const char* label,
    bool init_value,
    const char* color, // NULL:デフォルト色
    void (*on_changed_callback)(float) // NULL:UI操作不可
);

#ifdef __cplusplus
}
#endif
