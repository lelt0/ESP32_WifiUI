#include <string.h>
#include <stdio.h>
#include "wifiui_element_stext.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);

const wifiui_element_stext_t * wifiui_element_static_text(const char* text)
{
    wifiui_element_stext_t* self = (wifiui_element_stext_t*)malloc(sizeof(wifiui_element_stext_t));
    set_default_common(&self->common, WIFIUI_STATIC_TEXT, create_partial_html);

    self->text = strdup(text);

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_stext_t* self_stext = (wifiui_element_stext_t*)self;
    dstring_t* html = dstring_create(128);
    dstring_appendf(html, "<p class='wrap_text'>%s</p>", self_stext->text);
    return html;
}