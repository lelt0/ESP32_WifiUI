#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_scatter3dplot wifiui_element_scatter3dplot_t;
struct wifiui_element_scatter3dplot {
    wifiui_element_t common;
    const char* plot_title;
    float x_min;
    float x_max;
    float y_min;
    float y_max;
    float z_min;
    float z_max;
    void (*update_plot)(const wifiui_element_scatter3dplot_t* self, uint16_t point_count, const float* x, const float* y, const float* z, const uint32_t* rgb);
};

const wifiui_element_scatter3dplot_t * wifiui_element_scatter3d_plot(
    const char* plot_title, 
    float x_min, float x_max,
    float y_min, float y_max,
    float z_min, float z_max);

#ifdef __cplusplus
}
#endif
