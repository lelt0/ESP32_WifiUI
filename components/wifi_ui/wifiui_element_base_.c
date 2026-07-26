#include "wifiui_element_base_.h"
#include "wifiui_server.h"

#include <stdio.h>
#include <assert.h>

static wifiui_element_id next_id = 1; // id=0はシステム疎通確認用
static wifiui_element_id new_id();

void set_default_common(wifiui_element_t* dst, wifiui_element_type type, create_partial_html_f create_html_func)
{
    dst->type = type;
    dst->id = new_id();
    snprintf(dst->id_str, sizeof(dst->id_str), "%04X", dst->id);
    dst->system.create_partial_html = create_html_func;
    dst->system.on_post_from_this_element = NULL;
    dst->system.on_recv_data = NULL;
    dst->system.use_ploty = false;
}

void wifiui_element_send_data(const wifiui_element_t* element, const char* data, size_t len)
{
    wifiui_ws_send_element_data_async(element->id, data, len);
}

wifiui_element_id new_id()
{
    assert(next_id < UINT16_MAX);
    return next_id++;
}