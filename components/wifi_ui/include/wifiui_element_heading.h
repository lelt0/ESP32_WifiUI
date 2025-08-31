#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct {
    wifiui_element_t common;
    const char* text;
    uint8_t heading_level;
} wifiui_element_heading_t;

const wifiui_element_heading_t * wifiui_element_heading(const char* text, uint8_t heading_level); // heading_level: 1 to 6 (if you want <h1>, put '1'), or 0 for normal text <p>

#ifdef __cplusplus
}
#endif
