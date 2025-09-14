#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <esp_http_server.h>
#include "dstring.h"

typedef uint16_t wifiui_element_id;

typedef enum {
    WIFIUI_STATIC_TEXT,
    WIFIUI_HEADING,
    WIFIUI_BUTTON,
    WIFIUI_DYNAMIC_TEXT,
    WIFIUI_LINK,
    WIFIUI_INPUT,
    WIFIUI_AP_CONNECT_FORM,
    WIFIUI_MESSAGE_LOG,
    WIFIUI_TIMEPLOT,
    WIFIUI_SCATTERPLOT,
    WIFIUI_SCALLTER3D_PLOT
} wifiui_element_type;

typedef struct wifiui_element wifiui_element_t;
typedef dstring_t* (*create_partial_html_f)(const wifiui_element_t*);
struct wifiui_element {
    wifiui_element_type type;
    wifiui_element_id id;
    char id_str[5];
    struct {
        create_partial_html_f create_partial_html; // return pointer must be `free` by calling side.
        void (*on_post_from_this_element)(wifiui_element_t*, httpd_req_t*);
        bool use_websocket;
        void (*on_recv_data)(wifiui_element_t* self, const uint8_t* data, size_t len);
        bool use_plotly;
    } system;
};

void set_default_common(wifiui_element_t* dst, wifiui_element_type type, create_partial_html_f create_html_func);
void wifiui_element_send_data(const wifiui_element_t* dst, const char* data, size_t len);

#ifdef __cplusplus
}
#endif
