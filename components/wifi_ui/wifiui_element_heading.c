#include <string.h>
#include <stdio.h>
#include "wifiui_element_heading.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);

const wifiui_element_heading_t * wifiui_element_heading(const char* text, uint8_t heading_level)
{
    wifiui_element_heading_t* self = (wifiui_element_heading_t*)malloc(sizeof(wifiui_element_heading_t));
    set_default_common(&self->common, WIFIUI_HEADING, create_partial_html);

    self->text = strdup(text);
    self->heading_level = (heading_level>6)? 6 : heading_level;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_heading_t* self_heading = (wifiui_element_heading_t*)self;
    dstring_t* html = dstring_create(64);
    if(self_heading->heading_level == 0){
        dstring_appendf(html, "<p class='wrap_text'>%s</p>", self_heading->text);
    }else{
        dstring_appendf(html, "<h%u class='wrap_text'>%s</h%u>", self_heading->heading_level, self_heading->text, self_heading->heading_level);
    }
    return html;
}