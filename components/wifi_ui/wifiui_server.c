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
#include "esp_spiffs.h"

#include "dns_server.h"
#include "wifiui_page.h"

#define ESP_AP_PASS ""
#define MAX_AP_CONN 2

static const char * TAG = "wifiui_server";

typedef struct {
    int fd;
    esp_ip4_addr_t ip_addr;
} websocket_client_t;
static websocket_client_t ws_cilents[MAX_AP_CONN];

static httpd_handle_t server = NULL;
static const char * top_page_uri = NULL;
static void (*on_scan_completed_callback)(void*) = NULL;
static void * on_scan_completed_callback_arg = NULL;
static void (*on_ap_connected_callback)(void*, uint32_t) = NULL;
static void * on_ap_connected_callback_arg = NULL;
static void (*on_ap_disconnected_callback)(void*, uint8_t) = NULL;
static void * on_ap_disconnected_callback_arg = NULL;

static void wifi_init_softap(void);
static httpd_handle_t start_webserver(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t page_access_handler(httpd_req_t *req);
static esp_err_t websocket_handler(httpd_req_t *req);
static esp_err_t redirect_handler(httpd_req_t *req);
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err); // HTTP Error (404) Handler - Redirects all requests to the root page

static void create_ssid(char * dst, unsigned int dst_len);
static esp_ip4_addr_t get_client_ip_addr(httpd_req_t *req, int sockfd);
static esp_err_t mount_spiffs(void);
static void websoket_close(int fd);
static esp_err_t get_current_sta_ip(esp_netif_ip_info_t* dst);
static esp_err_t get_current_ap_ip(esp_netif_ip_info_t* dst);

void wifiui_start(const wifiui_page_t* top_page)
{
    if(server != NULL){
        ESP_LOGW(TAG, "server already started");
        return;
    }

    for(int i = 0; i < MAX_AP_CONN; i++){ ws_cilents[i].fd = -1; ws_cilents[i].ip_addr.addr = 0; }
    top_page_uri = top_page->uri;

    mount_spiffs();

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
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);


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

void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_dhcps_stop(netif);
    {
        // AP IP を 4.3.2.1 に設定
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        // IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
        // IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
        IP4_ADDR(&ip_info.ip, 198, 18, 0, 1);
        IP4_ADDR(&ip_info.gw, 198, 18, 0, 1);
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
            .password = ESP_AP_PASS,
            //.ssid_len = strlen(ap_ssid),
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = MAX_AP_CONN
        },
    };
    create_ssid((char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    if (strlen(ESP_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info_;
    get_current_ap_ip(&ip_info_);
    ESP_LOGI(TAG, "Set up softAP with IP: " IPSTR, IP2STR(&ip_info_.ip));
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             wifi_config.ap.ssid, ESP_AP_PASS);
}

httpd_handle_t start_webserver(void)
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
        {
            uint16_t page_count = 0;
            wifiui_page_t** pages = wifiui_get_pages(&page_count);
            for(int page_i = 0; page_i < page_count; page_i++)
            {
                wifiui_page_t* page = pages[page_i];
                const httpd_uri_t page_uri = {
                    .uri = page->uri,
                    .method = HTTP_ANY,
                    .handler = page_access_handler,
                    .user_ctx = NULL,
                    .is_websocket = false,
                };
                httpd_register_uri_handler(server, &page_uri);

                if(page->has_websocket)
                {
                    size_t uri_len = strlen(page->uri);
                    char * uri_ws = (char*)malloc(uri_len + 3 + 1);
                    strncpy(uri_ws, page->uri, uri_len + 1);
                    strcat(uri_ws, "/ws");
                    const httpd_uri_t websocket_uri = {
                        .uri = uri_ws,
                        .method = HTTP_GET,
                        .handler = websocket_handler,
                        .user_ctx = NULL,
                        .is_websocket = true
                    };
                    httpd_register_uri_handler(server, &websocket_uri);
                    free(uri_ws);
                }
            }
        }
        const httpd_uri_t redirect_uri = {
            .uri = "*",
            .method = HTTP_GET,
            .handler = redirect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &redirect_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

esp_err_t wifiui_connect_to_ap(const char* ssid, const char* password, wifi_auth_mode_t auth_mode)
{
    esp_wifi_disconnect();

    if(auth_mode == WIFI_AUTH_MAX) auth_mode = WIFI_AUTH_WPA_WPA2_PSK;

    wifi_config_t sta_config = {
        .sta = {
            // .ssid = ssid,
            // .password = password,
            .threshold.authmode = auth_mode,
        },
    };
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strncpy((char*)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    esp_err_t ret;
    ESP_ERROR_CHECK(ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(ret= esp_wifi_connect());
    return ret;
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        if(on_scan_completed_callback != NULL) {
            on_scan_completed_callback(on_scan_completed_callback_arg);
        }
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        if(on_ap_connected_callback != NULL) {
            on_ap_connected_callback(on_ap_connected_callback_arg, (uint32_t)event->ip_info.ip.addr);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        if(on_ap_disconnected_callback != NULL) {
            on_ap_disconnected_callback(on_ap_disconnected_callback_arg, disconn->reason);
        }
    }
}

esp_err_t page_access_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ACCESS: %s %s", req->uri, http_method_str(req->method));

    uint16_t pages_count = 0;
    wifiui_page_t ** pages = wifiui_get_pages(&pages_count);
    size_t uri_len_without_query = strcspn(req->uri, "?");

    for(int page_i = 0; page_i < pages_count; page_i++)
    {
        wifiui_page_t * page = pages[page_i];
        if(strncmp(req->uri, page->uri, uri_len_without_query) == 0)
        {
            switch (req->method)
            {
                case HTTP_GET:
                {
                    ESP_LOGI(TAG, "Serve page: %s \"%s\"", page->uri, page->title);
                    httpd_resp_set_type(req, "text/html");
                    dstring_t* html = wifiui_generate_page_html(page);
                    httpd_resp_send(req, html->str, HTTPD_RESP_USE_STRLEN);
                    dstring_free(html);
                    return ESP_OK;
                }
                break;
                case HTTP_POST:
                {
                    char query[32];
                    char param[16];
                    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
                        ESP_LOGI(TAG, "Found URL query => %s", query);
                        if (httpd_query_key_value(query, "eid", param, sizeof(param)) == ESP_OK) {
                            ESP_LOGI(TAG, "Found URL query parameter => eid=%s", param);
                            wifiui_element_id eid = (wifiui_element_id)strtol(param, NULL, 16);
                            wifiui_element_t* element = wifiui_find_element(page, eid);
                            if(element != NULL && element->system.on_post_from_this_element != NULL)
                            {
                                element->system.on_post_from_this_element(element, req);
                            }
                        }
                    }
                    httpd_resp_sendstr(req, "OK");
                    return ESP_OK;
                }
                break;
                default:
                {

                }
                break;
            }
            break;
        }
    }
    return ESP_FAIL;
}

esp_err_t websocket_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "ACCESS: %s %s", req->uri, http_method_str(req->method));

    int sock_fd = httpd_req_to_sockfd(req);
    
    if (req->method == HTTP_GET)
    {
        // WebSocket connection establish
        esp_ip4_addr_t connected_ip_addr = get_client_ip_addr(req, sock_fd);
        bool override = false;
        for(int exist_cli_i = 0; exist_cli_i < MAX_AP_CONN; exist_cli_i++) {
            if (ws_cilents[exist_cli_i].ip_addr.addr == connected_ip_addr.addr) {
                websoket_close(ws_cilents[exist_cli_i].fd);
                ws_cilents[exist_cli_i].fd = sock_fd;
                override = true;
                ESP_LOGW(TAG, "[WebSocket] Update Websocket of device (" IPSTR ")", IP2STR(&connected_ip_addr));
                break;
            }
        }
        if(!override) {
            int exist_cli_i;
            for(exist_cli_i = 0; exist_cli_i < MAX_AP_CONN; exist_cli_i++) {
                if (ws_cilents[exist_cli_i].fd < 0) {
                    ws_cilents[exist_cli_i].fd = sock_fd;
                    ws_cilents[exist_cli_i].ip_addr = connected_ip_addr;
                    break;
                }
            }
            if(exist_cli_i == MAX_AP_CONN) {
                ESP_LOGW(TAG, "[WebSocket] Max WebSocket clients reached (fd: %d)", MAX_AP_CONN);
            }
        }
        ESP_LOGI(TAG, "[WebSocket] WebSocket connection established from " IPSTR " (%d)", IP2STR(&connected_ip_addr), sock_fd);
        return ESP_OK;
    }

    // Websocket message receiving

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // to get frame length
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[WebSocket] httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    if (ws_pkt.len)
    {
        uint8_t *buf = (uint8_t*)malloc(ws_pkt.len + 1);        
        if (buf == NULL) {
            ESP_LOGE(TAG, "[WebSocket] Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        buf[ws_pkt.len] = 0;

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len); // to receive frame payload
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[WebSocket] httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        wifiui_element_id element_id = *((wifiui_element_id*)(ws_pkt.payload));
        ESP_LOGD(TAG, "[WebSocket] data recved from eid:%u", element_id);
        wifiui_element_t* sent_element = wifiui_find_element(NULL, element_id);
        if(sent_element->system.on_recv_data != NULL)
        {
            sent_element->system.on_recv_data(sent_element, ws_pkt.payload + sizeof(wifiui_element_id), ws_pkt.len - sizeof(wifiui_element_id));
        }
        free(buf);
    }
    return ESP_OK;
}

void wifiui_ws_send_data_async(const char* data, size_t len)
{
    if(server == NULL) return;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = len;
    for(int cli_i = 0; cli_i < MAX_AP_CONN; cli_i++)
    {
        if(ws_cilents[cli_i].fd >= 0)
        {
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;
            esp_err_t ret = httpd_ws_send_frame_async(server, ws_cilents[cli_i].fd, &ws_pkt);
            if (ret != ESP_OK) {
                ws_cilents[cli_i].fd = -1;
                ws_cilents[cli_i].ip_addr.addr = 0;
            }
        }
    }
}

esp_err_t redirect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Redirect else request: %s", req->uri);
    
    char redirect_url[64];
    snprintf(redirect_url, sizeof(redirect_url), "%s", top_page_uri);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", top_page_uri);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the top page", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to top page");
    return ESP_OK;
}

void create_ssid(char * dst, unsigned int dst_len)
{
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(dst, dst_len, "esp32-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

esp_ip4_addr_t get_client_ip_addr(httpd_req_t *req, int sockfd)
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

esp_err_t mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage", // partitions.csvのラベルに合わせる
        .max_files = 5,
        .format_if_mount_failed = false  // 既存イメージを消さない
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
        return err;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(conf.partition_label, &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted. total=%u, used=%u", (unsigned int)total, (unsigned int)used);
    return ESP_OK;
}

void websoket_close(int fd)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_CLOSE;
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;

    esp_err_t ret = httpd_ws_send_data(server, fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[WebSocket] Failed to send close frame: %s", esp_err_to_name(ret));
    }
}

void wifiui_start_ssid_scan()
{
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE
    };
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
}
void wifiui_set_ssid_scan_callback(void (*callback)(void*), void* arg)
{
    if(on_scan_completed_callback != NULL){
        ESP_LOGW(TAG, "scan_callback already exists. wifiui system can only have one scan_callback.");
        return;
    }
    on_scan_completed_callback = callback;
    on_scan_completed_callback_arg = arg;
}

void wifiui_set_ap_connected_callback(void (*callback)(void* arg, uint32_t ip_addr), void* arg)
{
    if(on_ap_connected_callback != NULL){
        ESP_LOGW(TAG, "connected_callback already exists. wifiui system can only have one connected_callback.");
        return;
    }
    on_ap_connected_callback = callback;
    on_ap_connected_callback_arg = arg;
}

void wifiui_set_ap_disconnected_callback(void (*callback)(void* arg, uint8_t reason), void* arg)
{
    if(on_ap_disconnected_callback != NULL){
        ESP_LOGW(TAG, "disconnected_callback already exists. wifiui system can only have one disconnected_callback.");
        return;
    }
    on_ap_disconnected_callback = callback;
    on_ap_disconnected_callback_arg = arg;
}

esp_err_t get_current_ap_ip(esp_netif_ip_info_t* dst)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if(!sta_netif) return ESP_FAIL;
    return esp_netif_get_ip_info(sta_netif, dst);
}

esp_err_t get_current_sta_ip(esp_netif_ip_info_t* dst)
{
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(!ap_netif) return ESP_FAIL;
    return esp_netif_get_ip_info(ap_netif, dst);
}

void wifiui_print_server_status()
{
    if(server == NULL) {
        ESP_LOGI(TAG, "HTTP server: not started");
    } else {
        esp_netif_ip_info_t ap_ip = {0};
        get_current_ap_ip(&ap_ip);
        ESP_LOGI(TAG, "HTTP server: " IPSTR, IP2STR(&ap_ip.ip));
    }
    char buf[64];
    int buff_off = 0;
    for(int cli_i = 0; cli_i < MAX_AP_CONN; cli_i++)
    {
        buff_off += snprintf(buf + buff_off, sizeof(buf) - buff_off, IPSTR "(%d), ", IP2STR(&ws_cilents[cli_i].ip_addr), ws_cilents[cli_i].fd);    
    }
    ESP_LOGI(TAG, "websocket clients: %s", buf);

    esp_netif_ip_info_t sta_ip = {0};
    get_current_sta_ip(&sta_ip);
    ESP_LOGI(TAG, "sta:: ip: " IPSTR, IP2STR(&sta_ip.ip));
}
