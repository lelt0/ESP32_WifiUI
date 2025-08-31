#include <string.h>
#include <stdio.h>
#include "wifiui_element_stext.h"

static char* create_partial_html(const wifiui_element_t* self);

const wifiui_element_stext_t * wifiui_element_static_text(const char* text)
{
    wifiui_element_stext_t* handler = (wifiui_element_stext_t*)malloc(sizeof(wifiui_element_stext_t));
    set_default_common(&handler->common, WIFIUI_STATIC_TEXT, create_partial_html);

    handler->text = strdup(text);

    return handler;
}

char* create_partial_html(const wifiui_element_t* self_)
{
    char* buf = (char*)malloc(1024); // TODO
    wifiui_element_stext_t* self = (wifiui_element_stext_t*)self_;
    snprintf(buf, 1024, "<p>%s</p>", self->text);
    return buf;
}