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
        "<button id='%s_scan' onclick='fetch(location.origin + location.pathname + \"?eid=%s&func=scan\", {method:\"POST\"})'>scan available SSID</button><br>"
        "<input class='single_input combo_input' id='%s_ssid' type='text' placeholder='SSID' autocomplete='off'/>"
            "<div class='combo_list' id='%s_ssid_list'></div>"
        "<input class='single_input' id='%s_password' type='password' placeholder='password' autocomplete='off'/>"
        "<button id='%s_connect'>connect</button>"
        "<script>"
        "const ssid_input = document.getElementById('%s_ssid');"
        "const ssid_list = document.getElementById('%s_ssid_list');"
        "function updateList(items) {"
            "ssid_list.innerHTML = '';"
            "items.forEach(v => {"
                "const div = document.createElement('div');"
                "div.className = 'combo_item';"
                "div.textContent = v;"
                "div.addEventListener('click', () => {"
                "ssid_input.value = v;"
                "ssid_list.style.display = 'none';"
                "});"
                "ssid_list.appendChild(div);"
            "});"
        "}"
        "ssid_input.addEventListener('focus', () => {"
            "if (ssid_list.children.length > 0) ssid_list.style.display = 'block';"
        "});"
        "ssid_input.addEventListener('blur', () => {"
            "setTimeout(() => ssid_list.style.display = 'none', 200);"
        "});"
        "ws_actions[%d]=function(data){"
            "updateList(JSON.parse(cstr2str(data)));"
        "};"
        "document.getElementById('%s_connect').addEventListener('click', function() {"
            "fetch('?eid=%s&func=connect', { method: 'POST', body: document.getElementById('%s_ssid').value + '/' + document.getElementById('%s_password').value });"
        "});"
        "</script>",
        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str,
        self_apform->common.id_str, 
        self_apform->common.id_str, 

        self_apform->common.id_str, self_apform->common.id_str, 

        self_apform->common.id, 

        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str
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