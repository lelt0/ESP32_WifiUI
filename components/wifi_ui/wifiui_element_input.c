#include <string.h>
#include <stdio.h>
#include "wifiui_element_input.h"

static char* create_partial_html(const wifiui_element_t* self);
static void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len);

const wifiui_element_input_t * wifiui_element_input(const char* button_label, void (*on_input_callback)(char*, void*), void* on_input_callback_param, bool clear_after_sent)
{
    wifiui_element_input_t* handler = (wifiui_element_input_t*)malloc(sizeof(wifiui_element_input_t));
    set_default_common(&handler->common, WIFIUI_INPUT, create_partial_html);
    handler->common.system.use_websocket = true;
    handler->common.system.on_recv_data = on_recv_data;

    handler->button_label = strdup(button_label);
    handler->on_input = on_input_callback;
    handler->on_input_param = on_input_callback_param;
    handler->clear_after_sent = clear_after_sent;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_input_t* self_input = (wifiui_element_input_t*)self;
    size_t buf_size = strlen(self_input->button_label) + 1024; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, 
        "<textarea id='%s' rows='1' placeholder='...'></textarea>"
        "<button onclick='eid=\"%s\"; t = document.getElementById(eid); ws_send_with_eid(%u, str2cstr(t.value)); %s'>%s</button>"
        "<script>"
        "document.getElementById('%s').addEventListener('input', function(){ fit_textarea_height('%s'); });"
        "window.addEventListener('load', function(){ fit_textarea_height('%s'); });"
        "window.addEventListener('resize', function(){ fit_textarea_height('%s'); });"
        "</script>",
        self_input->common.id_str, 
        self_input->common.id_str, self_input->common.id, (self_input->clear_after_sent?"t.value = \"\"; fit_textarea_height(eid);":""), self_input->button_label,
        self_input->common.id_str, self_input->common.id_str, self_input->common.id_str , self_input->common.id_str
    );
    return buf;
}

void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len)
{
    wifiui_element_input_t* self_input = (wifiui_element_input_t*)self;
    if(self_input->on_input != NULL)
    {
        self_input->on_input((char*)data, self_input->on_input_param);
    }
}