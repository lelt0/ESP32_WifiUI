#include <string.h>
#include <stdio.h>
#include "wifiui_element_ap_connect_form.h"
#include "wifiui_server.h"
#include "esp_wifi.h"

static char* create_partial_html(const wifiui_element_t* self);
static void posted(wifiui_element_t * self, httpd_req_t * req);
static void on_scan_completed(void* arg);

typedef struct {
    char ssid[33];
    wifi_auth_mode_t authmode;
} ssid_info_t;

static ssid_info_t* available_ssid = NULL;
static uint16_t available_ssid_count = 0;

const wifiui_element_apConnectForm_t * wifiui_element_ap_connect_form(void (*on_connect_callback)(bool))
{
    wifiui_element_apConnectForm_t* handler = (wifiui_element_apConnectForm_t*)malloc(sizeof(wifiui_element_apConnectForm_t));
    set_default_common(&handler->common, WIFIUI_AP_CONNECT_FORM, create_partial_html);
    handler->common.system.on_post_from_this_element = posted;

    handler->on_connect = on_connect_callback;
    handler->ssid_scanning = false;
    wifiui_set_ssid_scan_callback(on_scan_completed, (void*)handler);

    return handler;
}

char* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_apConnectForm_t* self_apform = (wifiui_element_apConnectForm_t*)self;
    size_t buf_size = 2048; // TODO
    char* buf = (char*)malloc(buf_size);
    snprintf(buf, buf_size, 
        "<button id='%s_scan'>scan available SSID</button><br>"
        "<input class='single_input combo_input' id='%s_ssid' type='text' placeholder='SSID' autocomplete='off'/>"
            "<div class='combo_list' id='%s_ssid_list'></div>"
        "<input class='single_input' id='%s_password' type='password' placeholder='password' autocomplete='off'/>"
        "<button id='%s_connect'>connect</button>"
        "<script>"
        "const scan_button = document.getElementById('%s_scan');"
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
        "scan_button.addEventListener('click', function() {"
            "fetch('?eid=%s&func=scan', {method:'POST'});"
            "scan_button.disabled = true;"
            "scan_button.textContent = 'scanning...';"
        "});"
        "ssid_input.addEventListener('focus', () => {"
            "if (ssid_list.children.length > 0) ssid_list.style.display = 'block';"
        "});"
        "ssid_input.addEventListener('blur', () => {"
            "setTimeout(() => ssid_list.style.display = 'none', 200);"
        "});"
        "ws_actions[%d]=function(data){"
            "console.log('[yatadebug] ' + cstr2str(data));"
            "updateList(JSON.parse(cstr2str(data)));"
            "scan_button.disabled = false;"
            "scan_button.textContent = 'scan available SSID';"
        "};"
        "document.getElementById('%s_connect').addEventListener('click', function() {"
            "fetch('?eid=%s&func=connect', { method: 'POST', body: document.getElementById('%s_ssid').value + '/' + document.getElementById('%s_password').value });"
        "});"
        "</script>",
        self_apform->common.id_str, self_apform->common.id_str, 
        self_apform->common.id_str, self_apform->common.id_str,
        self_apform->common.id_str, self_apform->common.id_str, 

        self_apform->common.id_str, self_apform->common.id_str, self_apform->common.id_str, 

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
                    if(!self_apform->ssid_scanning) {
                        printf("[yatadebug] SCAN\n");
                        self_apform->ssid_scanning = true;
                        wifiui_start_ssid_scan();
                    }
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

void on_scan_completed(void* arg)
{
    wifiui_element_apConnectForm_t *self_apform = (wifiui_element_apConnectForm_t*) arg;

    esp_wifi_scan_get_ap_num(&available_ssid_count);
    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * available_ssid_count);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&available_ssid_count, ap_info));

    free(available_ssid);
    available_ssid = NULL;
    if(available_ssid_count > 0)
    {
        available_ssid = (ssid_info_t*) malloc(sizeof(ssid_info_t) * available_ssid_count);
        for(uint16_t i = 0; i < available_ssid_count; i++) {
            strncpy(available_ssid[i].ssid, (char*)ap_info[i].ssid, sizeof(available_ssid[i].ssid));
            available_ssid[i].authmode = ap_info[i].authmode;
        }
    }
    free(ap_info);

    size_t total_len = sizeof(char) * (33+3) * available_ssid_count + 2;
    char* ssid_list_json = (char*) malloc(total_len);
    size_t off = 0;
    off += snprintf(ssid_list_json, 2, "[");
    for (uint16_t i = 0; i < available_ssid_count; i++) {
        if(i < available_ssid_count - 1)
            off += snprintf(ssid_list_json + off, total_len - off, "\"%s\",", available_ssid[i].ssid);
        else
            off += snprintf(ssid_list_json + off, total_len - off, "\"%s\"", available_ssid[i].ssid);
    }
    off += snprintf(ssid_list_json + off, total_len - off, "]");
    wifiui_element_send_data(&self_apform->common, ssid_list_json, strlen(ssid_list_json));
    free(ssid_list_json);

    self_apform->ssid_scanning = false;
}
