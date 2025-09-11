#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_plot wifiui_element_plot_t;
struct wifiui_element_plot {
    wifiui_element_t common;
    const char* plot_title;
    uint8_t series_count;
    char** series_names;
    char** series_colors;
    const char* y_label;
    bool y_auto_scale;
    float y_min;
    float y_max;
    float time_window_sec;
    void (*update_plot)(const wifiui_element_plot_t* self, const char* series_name, float value);
    void (*update_plots)(const wifiui_element_plot_t* self, const float* values);
};

const wifiui_element_plot_t * wifiui_element_plot(
    const char* plot_title, uint8_t series_count, char** series_names, 
    const char* y_label, float y_min, float y_max, 
    float time_window_sec);

#ifdef __cplusplus
}
#endif
