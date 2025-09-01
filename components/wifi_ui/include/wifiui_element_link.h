#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"
#include "../wifiui_page.h"

typedef struct {
    wifiui_element_t common;
    const char* text;
    const char* url;
} wifiui_element_link_t;

const wifiui_element_link_t * wifiui_element_link(const char* text, const wifiui_page_t * destination);

#ifdef __cplusplus
}
#endif
