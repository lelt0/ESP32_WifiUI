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
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "dns_server.h"
#include "wifi_ui.h"

#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_MAX_STA_CONN 2

static const char *TAG = "wifi_ui";

typedef struct {
    int fd;
    esp_ip4_addr_t ip_addr;
} websocket_client_t;

static websocket_client_t ws_status_cilents[EXAMPLE_MAX_STA_CONN];
static websocket_client_t ws_log_cilents[EXAMPLE_MAX_STA_CONN];
void (*led_callback_func)(void*) = NULL;
static httpd_handle_t server = NULL;

static const char *PORTAL_HTML =
#include "sample.html"
;

void create_ssid(char * dst, unsigned int dst_len)
{
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(dst, dst_len, "esp32-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

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
            //.ssid = ap_ssid,
            .password = EXAMPLE_ESP_WIFI_PASS,
            //.ssid_len = strlen(ap_ssid),
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = EXAMPLE_MAX_STA_CONN
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    create_ssid((char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info_;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info_);
    ESP_LOGI(TAG, "Set up softAP with IP: " IPSTR, IP2STR(&ip_info_.ip));
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             wifi_config.ap.ssid, EXAMPLE_ESP_WIFI_PASS);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    if(req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Serve root");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);

        return ESP_OK;
    }else if(req->method == HTTP_POST) {
        ESP_LOGI(TAG, "POST");

        char query[32];
        char param[16];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", query);
            if (httpd_query_key_value(query, "id", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => id=%s", param);
                if(strcmp(param, "0001") == 0)
                {
                    if (led_callback_func) led_callback_func(NULL); // Call the LED callback function
                }
                else if(strcmp(param, "0002") == 0)
                {
                    char buf[100];
                    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
                    if (ret > 0) {
                        buf[ret] = 0;
                        ESP_LOGI("WEB", "POST body: %s", buf); // username=xxx&password=yyy

                        wifi_config_t sta_config = {
                            .sta = {
                                //.ssid = ssid,
                                //.password = password,
                                .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
                            },
                        };
                        httpd_query_key_value(buf, "ssid", (char*)sta_config.sta.ssid, sizeof(sta_config.sta.ssid));
                        httpd_query_key_value(buf, "password", (char*)sta_config.sta.password, sizeof(sta_config.sta.password));
                        ESP_LOGI(TAG, "ssid:%s pass:%s", sta_config.sta.ssid, sta_config.sta.password);
                        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
                        ESP_ERROR_CHECK(esp_wifi_connect());

                        //httpd_resp_send(req, "received", HTTPD_RESP_USE_STRLEN);
                    }
                }
            }
        }

        return ESP_OK;
    }
    return ESP_OK;
}

esp_err_t get_current_sta_ip(esp_netif_ip_info_t* dst)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(!sta_netif) return ESP_FAIL;
    return esp_netif_get_ip_info(sta_netif, dst);
}

static esp_ip4_addr_t get_client_ip_addr(httpd_req_t *req, int sockfd)
{
    struct sockaddr_in6 addr;
    socklen_t addr_len = sizeof(addr);
    int peer_ret = getpeername(sockfd, (struct sockaddr *)&addr, &addr_len);

    if (peer_ret == 0) {
        if (addr.sin6_family == AF_INET6) {
            esp_ip4_addr_t ip_addr;
            ip_addr.addr = (uint32_t)addr.sin6_addr.un.u32_addr[3]; // IPv6 mapped IPv4 address
            return ip_addr;
        } else if (addr.sin6_family == AF_INET) {
            esp_ip4_addr_t ip_addr;
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
            ip_addr.addr = (uint32_t)addr4->sin_addr.s_addr;
            return ip_addr;
        } else {
            return (esp_ip4_addr_t){0};
        }
    } else {
        return (esp_ip4_addr_t){0};
    }
}

// WS Handler
static esp_err_t ws_status_handler(httpd_req_t *req) {
    int sock_fd = httpd_req_to_sockfd(req);
    
    // WebSocket connection establish
    if (req->method == HTTP_GET)
    {
        esp_ip4_addr_t connected_ip_addr = get_client_ip_addr(req, sock_fd);
        bool override = false;
        for(int exist_cli_i = 0; exist_cli_i < EXAMPLE_MAX_STA_CONN; exist_cli_i++) {
            if (ws_status_cilents[exist_cli_i].ip_addr.addr == connected_ip_addr.addr) {
                ws_status_cilents[exist_cli_i].fd = sock_fd;
                override = true;
                ESP_LOGW(TAG, "[STS_WS] Only last connect is vaid per device (" IPSTR ")", IP2STR(&connected_ip_addr));
                break;
            }
        }
        if(!override) {
            int exist_cli_i;
            for(exist_cli_i = 0; exist_cli_i < EXAMPLE_MAX_STA_CONN; exist_cli_i++) {
                if (ws_status_cilents[exist_cli_i].fd < 0) {
                    ws_status_cilents[exist_cli_i].fd = sock_fd;
                    ws_status_cilents[exist_cli_i].ip_addr = connected_ip_addr;
                    break;
                }
            }
            if(exist_cli_i == EXAMPLE_MAX_STA_CONN) {
                ESP_LOGW(TAG, "[STS_WS] Max WebSocket clients reached (fd: %d)", EXAMPLE_MAX_STA_CONN);
            }
        }
        ESP_LOGI(TAG, "[STS_WS] WebSocket connection established from " IPSTR " (%d)", IP2STR(&connected_ip_addr), sock_fd);
        return ESP_OK;
    }

    // Websocket message receiving

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // to get frame length
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[STS_WS] httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    if (ws_pkt.len)
    {
        uint8_t *buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "[STS_WS] Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len); // to receive frame payload
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[STS_WS] httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "[STS_WS] Packet type: %d", ws_pkt.type);
        ESP_LOGI(TAG, "[STS_WS] Got packet with message: %s", ws_pkt.payload);
        free(buf);
    }
    return ESP_OK;
}

static esp_err_t ws_log_handler(httpd_req_t *req) {
    int sock_fd = httpd_req_to_sockfd(req);
    
    // WebSocket connection establish
    if (req->method == HTTP_GET)
    {
        esp_ip4_addr_t connected_ip_addr = get_client_ip_addr(req, sock_fd);
        bool override = false;
        for(int exist_cli_i = 0; exist_cli_i < EXAMPLE_MAX_STA_CONN; exist_cli_i++) {
            if (ws_log_cilents[exist_cli_i].ip_addr.addr == connected_ip_addr.addr) {
                ws_log_cilents[exist_cli_i].fd = sock_fd;
                override = true;
                ESP_LOGW(TAG, "[LOG_WS] Only last connect is vaid per device (" IPSTR ")", IP2STR(&connected_ip_addr));
                break;
            }
        }
        if(!override) {
            int exist_cli_i;
            for(exist_cli_i = 0; exist_cli_i < EXAMPLE_MAX_STA_CONN; exist_cli_i++) {
                if (ws_log_cilents[exist_cli_i].fd < 0) {
                    ws_log_cilents[exist_cli_i].fd = sock_fd;
                    ws_log_cilents[exist_cli_i].ip_addr = connected_ip_addr;
                    break;
                }
            }
            if(exist_cli_i == EXAMPLE_MAX_STA_CONN) {
                ESP_LOGW(TAG, "[LOG_WS] Max WebSocket clients reached (fd: %d)", EXAMPLE_MAX_STA_CONN);
            }
        }
        ESP_LOGI(TAG, "[LOG_WS] WebSocket connection established from " IPSTR " (%d)", IP2STR(&connected_ip_addr), sock_fd);
        return ESP_OK;
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
    .method = HTTP_ANY,
    .handler = root_get_handler
};

static const httpd_uri_t ws_status_uri = {
    .uri = "/ws_status",
    .method = HTTP_GET,
    .handler = ws_status_handler,
    .user_ctx = NULL,
    .is_websocket = true
};

static const httpd_uri_t ws_log_uri = {
    .uri = "/ws_log",
    .method = HTTP_GET,
    .handler = ws_log_handler,
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
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
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
        httpd_register_uri_handler(server, &ws_status_uri);
        httpd_register_uri_handler(server, &ws_log_uri);
        httpd_register_uri_handler(server, &redirect_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

bool send_status(const char *message, size_t len)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = len;
    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++)
    {
        if(ws_status_cilents[i].fd >= 0)
        {
            // char buf[64];
            // snprintf(buf, 64, "%s (your fd is %d)", message, ws_status_cilents[i].fd);
            // ws_pkt.payload = (uint8_t *)buf;
            // ws_pkt.len = strlen(buf);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            esp_err_t ret = httpd_ws_send_frame_async(server, ws_status_cilents[i].fd, &ws_pkt);
            //esp_err_t ret = httpd_ws_send_data(server, ws_status_cilents[i].fd, &ws_pkt);
            if (ret != ESP_OK) {
                ws_status_cilents[i].fd = -1;
                ws_status_cilents[i].ip_addr.addr = 0;
            }
        }
    }
    return true;
}

bool send_log(const char *message, size_t len)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = len;
    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++)
    {
        if(ws_log_cilents[i].fd >= 0)
        {
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            esp_err_t ret = httpd_ws_send_frame_async(server, ws_log_cilents[i].fd, &ws_pkt);
            //esp_err_t ret = httpd_ws_send_data(server, ws_log_cilents[i].fd, &ws_pkt);
            if (ret != ESP_OK) {
                ws_log_cilents[i].fd = -1;
                ws_log_cilents[i].ip_addr.addr = 0;
            }
        }
    }
    return true;
}

void wifiui_start(wifiui_config_t *config, void (*led_callback)(void*))
{
    if(server != NULL){
        ESP_LOGW(TAG, "server already started");
        return;
    }

    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++){ ws_status_cilents[i].fd = -1; ws_status_cilents[i].ip_addr.addr = 0; }
    for(int i = 0; i < EXAMPLE_MAX_STA_CONN; i++){ ws_log_cilents[i].fd = -1; ws_log_cilents[i].ip_addr.addr = 0; }
    led_callback_func = led_callback;

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
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    server = start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);
}

void print_connections(const char* tag)
{
    char buf[32];
    int buf_off = snprintf(buf, sizeof(buf), "ws_fd: ");
    for(int cli_i = 0; cli_i < EXAMPLE_MAX_STA_CONN; cli_i++)
    {
        buf_off += snprintf(buf + buf_off, sizeof(buf) - buf_off, "(%d,%d)", ws_status_cilents[cli_i].fd, ws_log_cilents[cli_i].fd);
    }
    ESP_LOGI(tag, "%s", buf);

    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    ESP_LOGI(tag, "MAC(AP IF): %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(tag, "MAC(STA IF): %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}