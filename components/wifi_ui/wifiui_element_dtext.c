#include <string.h>
#include <stdio.h>
#include "wifiui_element_dtext.h"
#include "wifiui_server.h"

static char* create_partial_html(const wifiui_element_t* self);
static char* change_text_impl(const wifiui_element_dtext_t* self, const char* new_text);
void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len);

const wifiui_element_dtext_t * wifiui_element_dynamic_text(const char* text)
{
    wifiui_element_dtext_t* handler = (wifiui_element_dtext_t*)malloc(sizeof(wifiui_element_dtext_t));
    set_default_common(&handler->common, WIFIUI_DYNAMIC_TEXT, create_partial_html);
    handler->common.system.use_websocket = true;
    handler->common.system.on_recv_data = on_recv_data;

    handler->text = strdup(text);
    handler->change_text = change_text_impl;
    
    return handler;
}

char* create_partial_html(const wifiui_element_t* self_)
{
    wifiui_element_dtext_t* self = (wifiui_element_dtext_t*)self_;
    size_t buf_size = strlen(self->text) + 1024; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size - 1, 
        "<p id='%s'>%s</p>"
        "<script>"
            "ws_actions[%d]=function(data){"
                "document.getElementById('%s').innerText = cstr2str(data);"
                "ws_send_with_eid(%d, str2cstr(document.getElementById('%s').innerText))"
            "}"
        "</script>", 
        self->common.id_str, self->text, 
        self->common.id, 
        self->common.id_str,
        self->common.id, self->common.id_str);
    return buf;
}

char* change_text_impl(const wifiui_element_dtext_t* self, const char* new_text)
{
    size_t data_len = strlen(new_text) + 1;
    wifiui_element_send_data(&self->common, new_text, data_len);
    return ""; // TODO
}

void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len)
{
    wifiui_element_dtext_t* self_dtext = (wifiui_element_dtext_t*)self;
    char* old_text = (char*)self_dtext->text;
    self_dtext->text = strndup((const char*)data, len);
    free(old_text);
}