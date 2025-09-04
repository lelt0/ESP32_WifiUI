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

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_stext_t* self_stext = (wifiui_element_stext_t*)self;
    size_t buf_size = strlen(self_stext->text) + 32; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, "<p class='wrap_text'>%s</p>", self_stext->text);
    return buf;
}