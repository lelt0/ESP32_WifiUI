#include <memory.h>
#include <stdio.h>
#include "wifiui_element_button.h"

static char* create_partial_html(const wifiui_element_t* self);
static void on_clicked(wifiui_element_t * self, httpd_req_t * req);

const wifiui_element_button_t * wifiui_element_button(const char* label, onclick_callback_f onclick_callback, void* onclick_callback_param)
{
    wifiui_element_button_t* handler = (wifiui_element_button_t*)malloc(sizeof(wifiui_element_button_t));
    set_default_common(&handler->common, WIFIUI_BUTTON, create_partial_html);
    handler->common.system.on_post_from_this_element = on_clicked;

    handler->label = strdup(label);
    handler->onclick_callback = onclick_callback;
    handler->onclick_callback_param = onclick_callback_param;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self_)
{
    char* buf = (char*)malloc(1024); // TODO
    wifiui_element_button_t* self = (wifiui_element_button_t*)self_;
    snprintf(buf, 1024, "<button onclick='fetch(location.origin + \"?eid=%s\", {method:\"POST\"})' id=\"%s\">%s</button>", 
        self->common.id_str, self->common.id_str, self->label);
    return buf;
}

void on_clicked(wifiui_element_t * self, httpd_req_t * req)
{
    if(self != NULL)
    {
        wifiui_element_button_t * self_button = (wifiui_element_button_t *) self;
        self_button->onclick_callback(self_button, self_button->onclick_callback_param);
    }
}