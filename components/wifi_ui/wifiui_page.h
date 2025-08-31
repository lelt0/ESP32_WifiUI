#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "wifiui_element_base_.h"

typedef struct {
    const char* title;
    const char* uri;
    void ** element_handlers;
    size_t element_count;
    bool has_websocket;
} wifiui_page_t;

const wifiui_page_t * wifiui_create_page(const char * title, void * elements[], size_t element_count);
wifiui_page_t ** wifiui_get_pages(uint16_t* pages_count);
wifiui_element_t * wifiui_find_element(const wifiui_page_t * page, const wifiui_element_id id);

char* wifiui_generate_page_html(const wifiui_page_t* page); // return pointer must be `free()` by calling side!

#ifdef __cplusplus
}
#endif
