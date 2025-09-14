#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_scatterplot wifiui_element_scatterplot_t;
struct wifiui_element_scatterplot {
    wifiui_element_t common;
    const char* plot_title;
    const char* x_label;
    float x_min;
    float x_max;
    bool x_auto_scale;
    const char* y_label;
    float y_min;
    float y_max;
    bool y_auto_scale;
    void (*add_plot)(const wifiui_element_scatterplot_t* self, 
        const char* series_name, const uint32_t point_count, const float* x, const float* y, bool draw_line);
};

const wifiui_element_scatterplot_t * wifiui_element_scatterplot(
    const char* plot_title, 
    const char* x_label, float x_min, float x_max, 
    const char* y_label, float y_min, float y_max);

#ifdef __cplusplus
}
#endif
