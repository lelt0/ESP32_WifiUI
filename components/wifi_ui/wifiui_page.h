#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "wifiui_element_base_.h"
#include "dstring.h"

typedef struct {
    const char* title;
    const char* uri;
    const wifiui_element_t ** elements;
    size_t element_count;
    bool has_websocket;
} wifiui_page_t;

wifiui_page_t * wifiui_create_page(const char * title);
size_t wifiui_add_element(wifiui_page_t* page, const wifiui_element_t* element);
size_t wifiui_add_elements(wifiui_page_t* page, const wifiui_element_t* elements[], size_t element_count);
wifiui_page_t ** wifiui_get_pages(uint16_t* pages_count);
wifiui_element_t * wifiui_find_element(const wifiui_page_t * page, const wifiui_element_id id);

dstring_t* wifiui_generate_page_html(const wifiui_page_t* page); // return pointer must be `free()` by calling side!

#ifdef __cplusplus
}
#endif
