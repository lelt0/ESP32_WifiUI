#include <string.h>
#include <stdio.h>
#include "wifiui_element_heading.h"

static char* create_partial_html(const wifiui_element_t* self);

const wifiui_element_heading_t * wifiui_element_heading(const char* text, uint8_t heading_level)
{
    wifiui_element_heading_t* handler = (wifiui_element_heading_t*)malloc(sizeof(wifiui_element_heading_t));
    set_default_common(&handler->common, WIFIUI_HEADING, create_partial_html);

    handler->text = strdup(text);
    handler->heading_level = (heading_level>6)? 6 : heading_level;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_heading_t* self_heading = (wifiui_element_heading_t*)self;
    size_t buf_size = strlen(self_heading->text) + 16; // TODO
    char* buf = (char*)malloc(buf_size); // TODO
    if(self_heading->heading_level == 0){
        snprintf(buf, buf_size, "<p>%s</p>", self_heading->text);
    }else{
        snprintf(buf, buf_size, "<h%u>%s</h%u>", self_heading->heading_level, self_heading->text, self_heading->heading_level);
    }
    return buf;
}