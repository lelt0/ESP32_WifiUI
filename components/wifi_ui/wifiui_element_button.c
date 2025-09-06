#include <memory.h>
#include <stdio.h>
#include "wifiui_element_button.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void on_clicked(wifiui_element_t * self, httpd_req_t * req);

const wifiui_element_button_t * wifiui_element_button(const char* label, onclick_callback_f onclick_callback, void* onclick_callback_param)
{
    wifiui_element_button_t* self = (wifiui_element_button_t*)malloc(sizeof(wifiui_element_button_t));
    set_default_common(&self->common, WIFIUI_BUTTON, create_partial_html);
    self->common.system.on_post_from_this_element = on_clicked;

    self->label = strdup(label);
    self->onclick_callback = onclick_callback;
    self->onclick_callback_param = onclick_callback_param;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self_)
{
    dstring_t* html = dstring_create(128);
    wifiui_element_button_t* self = (wifiui_element_button_t*)self_;
    dstring_appendf(html, "<p><button onclick='fetch(location.origin + location.pathname + \"?eid=%s\", {method:\"POST\"})' id=\"%s\">%s</button></p>", 
        self->common.id_str, self->common.id_str, self->label);
    return html;
}

void on_clicked(wifiui_element_t * self, httpd_req_t * req)
{
    if(self != NULL)
    {
        wifiui_element_button_t * self_button = (wifiui_element_button_t *) self;
        self_button->onclick_callback(self_button, self_button->onclick_callback_param);
    }
}