#include <string.h>
#include <stdio.h>
#include "wifiui_element_link.h"

static char* create_partial_html(const wifiui_element_t* self);

const wifiui_element_link_t * wifiui_element_link(const char* text, const wifiui_page_t * destination)
{
    wifiui_element_link_t* handler = (wifiui_element_link_t*)malloc(sizeof(wifiui_element_link_t));
    set_default_common(&handler->common, WIFIUI_LINK, create_partial_html);

    handler->text = strdup(text);
    handler->url = destination->uri;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_link_t* self_link = (wifiui_element_link_t*)self;
    size_t buf_size = strlen(self_link->text) + 32; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, "<p><a href='%s'>%s</a></p>", self_link->url, self_link->text);
    return buf;
}