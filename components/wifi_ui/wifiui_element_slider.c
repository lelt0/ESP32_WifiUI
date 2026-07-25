#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wifiui_element_slider.h"
#include "wifiui_server.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void set_value_impl(const wifiui_element_slider_t* self, float value);
static void on_recv_data(wifiui_element_t* self, const uint8_t* data, size_t len);

const wifiui_element_slider_t * wifiui_element_slider(
    const char* label,
    float min,
    float max,
    float step,
    float init_value,
    const char* color,
    void (*on_changed_callback)(float) // NULL:操作不可
)
{
    wifiui_element_slider_t* self = (wifiui_element_slider_t*)malloc(sizeof(wifiui_element_slider_t));
    if(self == NULL) return NULL;

    set_default_common(&self->common, WIFIUI_SLIDER, create_partial_html);
    self->common.system.on_recv_data = on_recv_data;

    self->label = strdup(label);
    self->min = min;
    self->max = max;
    self->step = step;
    self->current_value = init_value;
    self->css_color = strdup(color);
    self->on_changed = on_changed_callback;
    self->set_value = set_value_impl;

    return self;
}

static dstring_t* create_partial_html(const wifiui_element_t* self_)
{
    wifiui_element_slider_t* self = (wifiui_element_slider_t*)self_;

    dstring_t* html = dstring_create(512);
    dstring_appendf(html,
        "<div style='display:flex'>"
            "<div id='%s_label' class='fixed_width' style='width:30%%'>%s</div>"
            "<input type='range' id='%s_slider' min='%f' max='%f' step='%f' value='%f' style='accent-color:%s; width:70%%; %s'>"
        "</div>"
        "<script>"
            "const _%s_label = document.getElementById('%s_label');"
            "const _%s_slider = document.getElementById('%s_slider');"
            "const update_%s_label = ()=>{ _%s_label.textContent = `%s ${_%s_slider.value}`; };"
            "update_%s_label();"
            "_%s_slider.addEventListener('input', ()=>{"
                "update_%s_label();"
                "ws_send_with_eid(%d, floats2bytes([_%s_slider.value]))"
            "});"
            
            "ws_actions[%d]=function(array){"
                "_%s_slider.value = bytes2floats(array)[0];"
                "update_%s_label();"
            "}"
        "</script>",
        self->common.id_str, self->label, 
        self->common.id_str, self->min, self->max, self->step, self->current_value, self->css_color, (self->on_changed==NULL?"pointer-events:none":""),

        self->common.id_str, self->common.id_str,
        self->common.id_str, self->common.id_str,
        self->common.id_str, self->common.id_str, self->label, self->common.id_str,
        self->common.id_str,
        self->common.id_str,
            self->common.id_str,
            self->common.id, self->common.id_str,
        
        self->common.id, 
            self->common.id_str,
            self->common.id_str
    );

    return html;
}

static void set_value_impl(const wifiui_element_slider_t* self_, float value)
{
    wifiui_element_slider_t* self = (wifiui_element_slider_t*)self_;
    
    if(value < self->min) value = self->min;
    if(value > self->max) value = self->max;

    self->current_value = value;
    wifiui_element_send_data(&self->common, (const char*)&self->current_value, sizeof(self->current_value));
}


static void on_recv_data(wifiui_element_t* self_, const uint8_t* data, size_t len)
{
    wifiui_element_slider_t* self = (wifiui_element_slider_t*)self_;
    if(self == NULL || data == NULL || len < sizeof(float)) return;

    float value = *(const float *)data;
    self->current_value = value;
    if(self->on_changed != NULL)
    {
        self->on_changed(value);
    }
}