#include "wifiui_element_base_.h"
#include "wifiui_server.h"

#include <stdio.h>
#include <assert.h>

static wifiui_element_id next_id = 0;

static void default_send_data(wifiui_element_t* target_element, const char* data, size_t len);

wifiui_element_id new_id()
{
    assert(next_id < UINT16_MAX);
    return next_id++;
}

void set_default_common(wifiui_element_t* dst, wifiui_element_type type, create_partial_html_f create_html_func)
{
    dst->type = type;
    dst->id = new_id();
    snprintf(dst->id_str, sizeof(dst->id_str), "%04X", dst->id);
    dst->system.create_partial_html = create_html_func;
    dst->system.on_post_from_this_element = NULL;
    dst->system.use_websocket = false;
    dst->system.on_recv_data = NULL;
}

void wifiui_element_send_data(const wifiui_element_t* dst, const char* data, size_t len)
{
    char * element_data = (char*)malloc(sizeof(wifiui_element_id) + len);
    if(element_data == NULL) return;

    *((wifiui_element_id*)element_data) = dst->id;
    memcpy(element_data + sizeof(wifiui_element_id), data, len);
    wifiui_ws_send_data_async(element_data, sizeof(wifiui_element_id) + len);
    free(element_data);
}