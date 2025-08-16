/* Captive Portal Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

static const char *TAG = "example";

int ws_fd[EXAMPLE_MAX_STA_CONN] = { -1 };
httpd_handle_t server = NULL;
const char *PORTAL_HTML =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>ESP32 WS</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style type='text/css'>"
        "body { font-family: system-ui, sans-serif; margin: 0; padding: 1rem; line-height: 1.6; max-width: 900px; margin-left: auto; margin-right: auto; }" 
        "h1, h2, h3 { line-height: 1.2; } "
        "button { padding: 0.6em 1.2em; font-size: 1rem; border: none; border-radius: 0.5em; background: #0078ff; color: white; cursor: pointer; } button:hover { background: #005fcc; } "
        "img, video { max-width: 100%; height: auto; display: block; margin: 1rem 0; } "
        "@media (max-width: 600px) { body { padding: 0.8rem; font-size: 0.95rem; } button { width: 100%; } } "
        "@media (min-width: 601px) { body { font-size: 1.05rem; } }"
        "</style>"
        "</head>"
        "<body>"
        "<h2>ESP32 WebSocket Demo</h2>"
        "<button onclick='toggleLED()'>Toggle LED</button>"
        "<p id='time'>--</p>"
        "<script>"
        "let ws = new WebSocket('ws://' + location.host + '/ws');"
        "ws.onmessage = function(evt) { document.getElementById('time').innerText = evt.data; };"
        "function toggleLED() { ws.send('toggle'); }"
        "</script>"
        "</body>"
        "</html>";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_dhcps_stop(netif);
    {
        // AP IP を 4.3.2.1 に設定
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
        IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_set_ip_info(netif, &ip_info);

        // Captive Portal URL を設定
        char captiveportal_uri[32];
        snprintf(captiveportal_uri, sizeof(captiveportal_uri), "http://" IPSTR, IP2STR(&ip_info.ip));
        ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri)));
    }
    esp_netif_dhcps_start(netif);

    // Wifi AP 起動
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = EXAMPLE_MAX_STA_CONN
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info_;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info_);
    ESP_LOGI(TAG, "Set up softAP with IP: " IPSTR, IP2STR(&ip_info_.ip));
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    //const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    //httpd_resp_send(req, root_start, root_len);
    httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// WS Handler
static esp_err_t ws_handler(httpd_req_t *req) {
    int sock_fd = httpd_req_to_sockfd(req);
    
    // WebSocket connection establish
    if (req->method == HTTP_GET)
    {
        // // すでに他のクライアントが接続中なら切断
        // if(ws_fd[0] >= 0)
        // {
        //     httpd_sess_trigger_close(req->handle, ws_fd[0]);
        //     ws_fd[0] = -1;
        // }
        for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++) {
            if (ws_fd[i] == -1) {
                ws_fd[i] = sock_fd;
                break;
            }
        }
        ESP_LOGI(TAG, "WebSocket connection established (%d)", sock_fd);
        return ESP_OK;
    }

    // Websocket message receiving

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // to get frame length
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    if (ws_pkt.len)
    {
        uint8_t *buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len); // to receive frame payload
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
        free(buf);
    }
    return ESP_OK;
}

static esp_err_t redirect_handler(httpd_req_t *req) {
    ESP_LOGD(TAG, "Redirect else request: %s", req->uri);
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    char redirect_url[64];
    snprintf(redirect_url, sizeof(redirect_url), "http://" IPSTR "/", IP2STR(&ip_info.ip));

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

static const httpd_uri_t ws_uri = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
};

static const httpd_uri_t redirect_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = redirect_handler,
    .user_ctx = NULL
};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // "/*" を使う
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &ws_uri);
        httpd_register_uri_handler(server, &redirect_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}
bool send_all(const char *message, size_t len)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    // ws_pkt.payload = (uint8_t *)message;
    // ws_pkt.len = len;
    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++)
    {
        if(ws_fd[i] >= 0)
        {
            char buf[64];
            snprintf(buf, 64, "%s (your fd is %d)", message, ws_fd[i]);
            ws_pkt.payload = (uint8_t *)buf;
            ws_pkt.len = strlen(buf);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            esp_err_t ret = httpd_ws_send_frame_async(server, ws_fd[i], &ws_pkt);
            //esp_err_t ret = httpd_ws_send_data(server, ws_fd[i], &ws_pkt);
            ESP_LOGI(TAG, "send %s", buf);
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "send failed with %d", ret);
                ws_fd[i] = -1;
            }
        }
    }
    ESP_LOGI(TAG, "ws_fd: %d %d %d %d", ws_fd[0], ws_fd[1], ws_fd[2], ws_fd[3]);
    return true;
}

void time_task(void *arg) {
    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        char buf[64];
        snprintf(buf, sizeof(buf), "t%02d", count++);
        ESP_LOGI(TAG, "send: %s", buf);
        send_all(buf, strlen(buf));
    }
}

void app_main(void)
{
    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++) ws_fd[i] = -1;

    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    server = start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&config);

    xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);

}