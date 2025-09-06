#include <string.h>
#include <stdio.h>
#include "wifiui_element_link.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);

const wifiui_element_link_t * wifiui_element_link(const char* text, const wifiui_page_t * destination)
{
    wifiui_element_link_t* self = (wifiui_element_link_t*)malloc(sizeof(wifiui_element_link_t));
    set_default_common(&self->common, WIFIUI_LINK, create_partial_html);

    self->text = strdup(text);
    self->url = destination->uri;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_link_t* self_link = (wifiui_element_link_t*)self;
    dstring_t* html = dstring_create(128);
    dstring_appendf(html, "<p class='wrap_text'><a href='%s'>%s</a></p>", self_link->url, self_link->text);
    return html;
}