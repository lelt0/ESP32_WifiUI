#include <string.h>
#include <stdio.h>
#include "wifiui_element_dtext.h"
#include "wifiui_server.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static char* change_text_impl(const wifiui_element_dtext_t* self, const char* new_text);
void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len);

const wifiui_element_dtext_t * wifiui_element_dynamic_text(const char* text)
{
    wifiui_element_dtext_t* self = (wifiui_element_dtext_t*)malloc(sizeof(wifiui_element_dtext_t));
    set_default_common(&self->common, WIFIUI_DYNAMIC_TEXT, create_partial_html);
    self->common.system.use_websocket = true;
    self->common.system.on_recv_data = on_recv_data;

    self->text = strdup(text);
    self->change_text = change_text_impl;
    
    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self_)
{
    wifiui_element_dtext_t* self = (wifiui_element_dtext_t*)self_;
    dstring_t* html = dstring_create(256);
    dstring_appendf(html, 
        "<p class='wrap_text' id='%s'>%s</p>"
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
    return html;
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