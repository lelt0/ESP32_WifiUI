#include <string.h>
#include <stdio.h>
#include "wifiui_element_ap_connect_form.h"
#include "wifiui_server.h"

static char* create_partial_html(const wifiui_element_t* self);

const wifiui_element_apConnectForm_t * wifiui_element_ap_connect_form(void (*on_connect_callback)(bool))
{
    wifiui_element_apConnectForm_t* handler = (wifiui_element_apConnectForm_t*)malloc(sizeof(wifiui_element_apConnectForm_t));
    set_default_common(&handler->common, WIFIUI_AP_CONNECT_FORM, create_partial_html);

    handler->on_connect = on_connect_callback;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_apConnectForm_t* self_apform = (wifiui_element_apConnectForm_t*)self;
    size_t buf_size = 2048; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, 
        "<button id='%s_scan' onclick='fetch(location.origin + location.pathname + \"?eid=%s&func=scan\", {method:\"POST\"})'>scan available SSID</button>"
        "<form action=location.origin+location.pathname+'/?eid=%s&func=connect' method='post'>"
            "<input id='%s_ssid' name='%s_ssid' type='text' list='ssid_suggestions' placeholder='input or select SSID' autocomplete='off' required/>"
            "<datalist id='%s_ssid_options'></datalist>"
            "<input id='%s_password' name='%s_password' type='password' placeholder='password' required/>"
            "<button type='submit'>connect</button>"
        "</form>"
        "<script>"
        "const ssid_list = document.getElementById('%s_ssid_options');"
        "ws_actions[%d]=function(data){"
            "const options = JSON.parse(cstr2str(data));"
            "const frag = document.createDocumentFragment();"
            "for (const v of options) {"
                "const opt = document.createElement('option');"
                "opt.value = v;"
                "frag.appendChild(opt);"
            "}"
            "ssid_list.replaceChildren(frag);"
        "};"
        "</script>",
        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str, 

        self_apform->common.id_str, 
        self_apform->common.id
    );
    return buf;
}