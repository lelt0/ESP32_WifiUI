#include <string.h>
#include <stdio.h>
#include "wifiui_element_ap_connect_form.h"
#include "wifiui_server.h"

static char* create_partial_html(const wifiui_element_t* self);
static void posted(wifiui_element_t * self, httpd_req_t * req);

const wifiui_element_apConnectForm_t * wifiui_element_ap_connect_form(void (*on_connect_callback)(bool))
{
    wifiui_element_apConnectForm_t* handler = (wifiui_element_apConnectForm_t*)malloc(sizeof(wifiui_element_apConnectForm_t));
    set_default_common(&handler->common, WIFIUI_AP_CONNECT_FORM, create_partial_html);
    handler->common.system.on_post_from_this_element = posted;

    handler->on_connect = on_connect_callback;

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_apConnectForm_t* self_apform = (wifiui_element_apConnectForm_t*)self;
    size_t buf_size = 2048; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, 
        "<p>"
        "<button id='%s_scan' onclick='fetch(location.origin + location.pathname + \"?eid=%s&func=scan\", {method:\"POST\"})'>scan available SSID</button><br>"
        "<input id='%s_ssid' type='text' list='%s_ssid_options' placeholder='SSID' autocomplete='off' required>"
            "<datalist id='%s_ssid_options'><option value='aaa'><option value='bbb'><option value='ccc'></datalist><br>"
        "<input id='%s_password' type='password' placeholder='password' required><br>"
        "<button id='%s_connect'>connect</button>"
        "</p>"
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
        "document.getElementById('%s_connect').addEventListener('click', function() {"
            "fetch('?eid=%s&func=connect', { method: 'POST', body: document.getElementById('%s_ssid').value + '/' + document.getElementById('%s_password').value });"
        "});"
        "</script>",
        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str,
        self_apform->common.id_str, 
        self_apform->common.id_str, 
        self_apform->common.id_str, 

        self_apform->common.id_str, self_apform->common.id, 

        self_apform->common.id_str, self_apform->common.id_str, self_apform->common.id_str, self_apform->common.id_str
    );
    return buf;
}

void posted(wifiui_element_t * self, httpd_req_t * req)
{
    if(self != NULL)
    {
        wifiui_element_apConnectForm_t* self_apform = (wifiui_element_apConnectForm_t*)self;

        char query[32];
        char param[16];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            if (httpd_query_key_value(query, "func", param, sizeof(param)) == ESP_OK) {
                if(strncmp(param, "scan", sizeof(param))==0)
                {
                    printf("[yatadebug] SCAN\n");
                    const char* dummy_data = "[\"apple\",\"banana\",\"orange\"]";
                    wifiui_element_send_data(&self_apform->common, dummy_data, strlen(dummy_data));
                }
                else if(strncmp(param, "connect", sizeof(param))==0)
                {
                    printf("[yatadebug] CONNECT\n");
                    char buf[100];
                    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
                    if (ret > 0) {
                        buf[ret] = 0;
                        printf("[yatadebug] POST body: %s\n", buf); // username/password
                    }
                }
            }
        }
    }
}