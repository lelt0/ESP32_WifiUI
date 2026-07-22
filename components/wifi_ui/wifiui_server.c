#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "dns_server.h"
#include "wifiui_page.h"
#include "wifiui_server.h"

#define MAX_AP_CONN 2
#define MAX_CLIENTS 2 // AP clients + LAN access
#define MAX_SOCKS 7

static const char * TAG = "wifiui_server";
extern const uint8_t ploty_min_js_gz_start[] asm("_binary_ploty_min_js_gz_start");
extern const uint8_t ploty_min_js_gz_end[]   asm("_binary_ploty_min_js_gz_end");

// 接続中のclient管理
typedef struct {
    bool active;
    esp_ip4_addr_t ip;
    const wifiui_page_t* active_page;
    int http_fd;
    int ws_fd;
    uint64_t last_ping_time_us;
} client_info_t;
static client_info_t clients_info[MAX_CLIENTS];
static void clear_client_info(client_info_t* cinfo){ cinfo->active = false; cinfo->ip.addr = 0; cinfo->active_page = NULL; cinfo->http_fd=-1; cinfo->ws_fd=-1; cinfo->last_ping_time_us = 0; }
static client_info_t* find_free_client(){ for(int i=0;i<MAX_CLIENTS;i++){ if(!clients_info[i].active) return (clients_info+i); } return NULL; }
static client_info_t* find_client(esp_ip4_addr_t ip){ for(int i=0;i<MAX_CLIENTS;i++){ if(clients_info[i].active && clients_info[i].ip.addr == ip.addr) return &clients_info[i]; } return NULL; }
static void close_client(client_info_t* client);
static void close_session(int fd);
static void close_unused_clients();
static void print_client_info(client_info_t* cinfo){ if(!cinfo) return; printf("active:%c " IPSTR " page:%s(H%d/W%d) tim:%llums\n", (cinfo->active?'T':'F'), IP2STR(&cinfo->ip), (cinfo->active_page?cinfo->active_page->uri:"NULL"), cinfo->http_fd, cinfo->ws_fd, cinfo->last_ping_time_us/1000); }
static void wifiui_ws_send_pong(int fd);
static esp_ip4_addr_t get_client_ip_addr(httpd_req_t *req, int sockfd);

static httpd_handle_t server = NULL;
static const char * top_page_uri = NULL;
static void (*on_scan_completed_callback)(void*) = NULL;
static void * on_scan_completed_callback_arg = NULL;
static void (*on_ap_connected_callback)(void*, uint32_t) = NULL;
static void * on_ap_connected_callback_arg = NULL;
static void (*on_ap_disconnected_callback)(void*, uint8_t) = NULL;
static void * on_ap_disconnected_callback_arg = NULL;

static void wifi_init_softap(const char* ap_ssid, const char* ap_password);
static httpd_handle_t start_webserver(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t page_access_handler(httpd_req_t *req);
static esp_err_t websocket_handler(httpd_req_t *req);
static esp_err_t ploty_js_get_handler(httpd_req_t *req);
static esp_err_t redirect_handler(httpd_req_t *req);
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err); // HTTP Error (404) Handler - Redirects all requests to the root page

static void create_ssid(char * dst, unsigned int dst_len);
static esp_err_t get_current_sta_ip(esp_netif_ip_info_t* dst);
static esp_err_t get_current_ap_ip(esp_netif_ip_info_t* dst);

void wifiui_start(const char* ap_ssid, const char* ap_password, const wifiui_page_t* top_page)
{
    if(server != NULL){
        ESP_LOGW(TAG, "server already started");
        return;
    }

    for(int i = 0; i < MAX_CLIENTS; i++) clear_client_info(&clients_info[i]);
    top_page_uri = top_page->uri;

    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);

    // Initialize NVS needed by Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // フラッシュを消去してから再初期化
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &wifi_event_handler, NULL, NULL);


    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap(ap_ssid, ap_password);

    // Start the server for the first time
    server = start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    ESP_LOGI(TAG, "wifiui started!");
}

void wifi_init_softap(const char* ap_ssid, const char* ap_password)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_dhcps_stop(netif);
    {
        // AP IP を 198.18.0.1 に設定（RFC 2544で定義されたベンチマーク用アドレスのため、一般的なPublicIPとの衝突はないはず）
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
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
            //.ssid_len = strlen(ap_ssid),
            //.password = ap_password,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = MAX_AP_CONN
        },
    };
    if(strlen(ap_ssid)==0) {
        create_ssid((char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    }else{
        strncpy((char*)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    }
    if (strlen(ap_password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }else{
        if(strlen(ap_password) > sizeof(wifi_config.ap.password) - 1) ESP_LOGW(TAG, "AP Password is too long!");
        strncpy((char*)wifi_config.ap.password, ap_password, sizeof(wifi_config.ap.password));
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info_;
    get_current_ap_ip(&ip_info_);
    ESP_LOGI(TAG, "Set up softAP with IP:" IPSTR " SSID:'%s' password:'%s'", IP2STR(&ip_info_.ip), wifi_config.ap.ssid, wifi_config.ap.password);
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = tskIDLE_PRIORITY+5;
    config.uri_match_fn = httpd_uri_match_wildcard; // "/*" を使う
    config.lru_purge_enable = true; // 古い接続を追い出す
    config.max_open_sockets = MAX_SOCKS;
    config.recv_wait_timeout = 3;
    config.send_wait_timeout = 3;
    uint16_t page_count = 0;
    wifiui_page_t** pages = wifiui_get_pages(&page_count);
    config.max_uri_handlers = page_count * 2 + 2;

    // Start the httpd server
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        {
            for(int page_i = 0; page_i < page_count; page_i++)
            {
                wifiui_page_t* page = pages[page_i];
                const httpd_uri_t page_uri = {
                    .uri = page->uri,
                    .method = HTTP_ANY,
                    .handler = page_access_handler,
                    .user_ctx = page,
                    .is_websocket = false,
                };
                httpd_register_uri_handler(server, &page_uri);

                if(page->use_websocket)
                {
                    size_t uri_len = strlen(page->uri);
                    char * uri_ws = (char*)malloc(uri_len + 3 + 1);
                    strncpy(uri_ws, page->uri, uri_len + 1);
                    strcat(uri_ws, "/ws");
                    const httpd_uri_t websocket_uri = {
                        .uri = uri_ws,
                        .method = HTTP_GET,
                        .handler = websocket_handler,
                        .user_ctx = page,
                        .is_websocket = true
                    };
                    httpd_register_uri_handler(server, &websocket_uri);
                    free(uri_ws);
                }
            }
        }
        static const httpd_uri_t ploty_js_uri = {
            .uri = "/ploty.js",
            .method = HTTP_GET,
            .handler = ploty_js_get_handler
        };
        httpd_register_uri_handler(server, &ploty_js_uri);
        const httpd_uri_t redirect_uri = {
            .uri = "*",
            .method = HTTP_GET,
            .handler = redirect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &redirect_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    ESP_LOGI(TAG, "HTTP server started. port:%u", config.server_port);
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

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGD(TAG, "New AP client connected. mac:" MACSTR, MAC2STR(event->mac));
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT)
    {
        ip_event_assigned_ip_to_client_t *event = (ip_event_assigned_ip_to_client_t*) event_data;
        ESP_LOGI(TAG, "IP assigned. mac:" MACSTR " ip:" IPSTR, MAC2STR(event->mac), IP2STR(&event->ip));
        close_unused_clients();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = event_data;
        ESP_LOGI(TAG, "AP client disconnected. mac:" MACSTR, MAC2STR(event->mac));
    }
}

esp_err_t page_access_handler(httpd_req_t *req)
{
    int sock_fd = httpd_req_to_sockfd(req);
    ESP_LOGD(TAG, "ACCESS: %s %s (fd%d)", req->uri, http_method_str(req->method), sock_fd);

    uint16_t pages_count = 0;
    wifiui_page_t ** pages = wifiui_get_pages(&pages_count);
    size_t uri_len_without_query = strcspn(req->uri, "?");

    for(int page_i = 0; page_i < pages_count; page_i++)
    {
        wifiui_page_t * page = pages[page_i];
        if(strncmp(req->uri, page->uri, uri_len_without_query) == 0)
        {
            esp_ip4_addr_t ip = get_client_ip_addr(req, sock_fd);
            client_info_t* client = find_client(ip);
            if(!client) client = find_free_client();
            if(client)
            {
                client->active = true;
                client->ip = ip;
                client->http_fd = sock_fd;
            }

            switch (req->method)
            {
                case HTTP_GET:
                {
                    httpd_resp_set_type(req, "text/html");
                    dstring_t* html = wifiui_generate_page_html(page);
                    httpd_resp_send(req, html->str, HTTPD_RESP_USE_STRLEN);
                    dstring_free(html);
                    
                    ESP_LOGI(TAG, "HTTP %s served. (fd%d)", req->uri, sock_fd);
                    return ESP_OK;
                }
                break;
                case HTTP_POST:
                {
                    char query[32];
                    char param[16];
                    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
                        if (httpd_query_key_value(query, "eid", param, sizeof(param)) == ESP_OK) {
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
    int sock_fd = httpd_req_to_sockfd(req);
    ESP_LOGD(TAG, "ACCESS(WS): %s %s (fd%d)", req->uri, http_method_str(req->method), sock_fd);

    if (req->method == HTTP_GET) // WebSocket connection establish
    {
        esp_ip4_addr_t ip = get_client_ip_addr(req, sock_fd);
        client_info_t* client = find_client(ip);
        if(!client) client = find_free_client();
        if(client)
        {
            wifiui_page_t* page = (wifiui_page_t*)req->user_ctx;
            client->active = true;
            client->ip = ip;
            client->ws_fd = sock_fd;
            client->active_page = page;
        }
        else
        {
            ESP_LOGW(TAG, "Reached MAX Clients! (MAX:%d)", MAX_CLIENTS);
        }

        ESP_LOGI(TAG, "WebSocket connection established by fd%d", sock_fd);
        return ESP_OK;
    }

    // Websocket message receiving
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // to get frame length
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get ws-frame len. %s.", esp_err_to_name(ret));
        return ret;
    }
    if (ws_pkt.len)
    {
        uint8_t *buf = (uint8_t*)malloc(ws_pkt.len + 1);        
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for received ws buf. %s.", esp_err_to_name(ret));
            return ESP_ERR_NO_MEM;
        }
        buf[ws_pkt.len] = 0;

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len); // to receive frame payload
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get ws payload. %s.", esp_err_to_name(ret));
            free(buf);
            return ret;
        }
        wifiui_element_id element_id = *((wifiui_element_id*)(ws_pkt.payload));
        ESP_LOGD(TAG, "ws data recved from eid:%u", element_id);
        uint8_t* payload_data = ws_pkt.payload + sizeof(wifiui_element_id);
        size_t payload_len = ws_pkt.len - sizeof(wifiui_element_id);
        if(element_id == 0) // id0はシステム疎通確認
        {
            esp_ip4_addr_t ip = get_client_ip_addr(req, sock_fd);
            client_info_t* client = find_client(ip);
            if(client) client->last_ping_time_us = esp_timer_get_time();
            wifiui_ws_send_pong(sock_fd);
        }
        else
        {
            wifiui_element_t* sent_element = wifiui_find_element(NULL, element_id);
            if(sent_element->system.on_recv_data != NULL)
            {
                sent_element->system.on_recv_data(sent_element, payload_data, payload_len);
            }
        }
        free(buf);
    }
    return ESP_OK;
}

esp_err_t ploty_js_get_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "ACCESS(ploty.js): %s %s (fd%d)", req->uri, http_method_str(req->method), httpd_req_to_sockfd(req));

    const size_t len = ploty_min_js_gz_end - ploty_min_js_gz_start;
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char *)ploty_min_js_gz_start, len);
    return ESP_OK;
}

void wifiui_ws_send_data_async(const char* data, size_t len, const wifiui_element_t* element_info)
{
    if(server == NULL) return;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = len;
    for(int cli_i = 0; cli_i < MAX_CLIENTS; cli_i++)
    {
        // そのelementがあるページがアクティブなクライアントにだけ送信する
        bool element_is_active = false;
        client_info_t* client = &clients_info[cli_i];
        if(!client->active) continue;
        if(client->active_page == NULL) continue;
        for(int element_i = 0; element_i < client->active_page->element_count; element_i++)
        {
            if(client->active_page->elements[element_i]->id == element_info->id)
            {
                element_is_active = true;
                break;
            }
        }

        if(client->ws_fd >= 0 && element_is_active)
        {
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;
            esp_err_t ret = httpd_ws_send_frame_async(server, client->ws_fd, &ws_pkt);
            if (ret != ESP_OK) { /* failed */ }
        }
    }
}

void wifiui_ws_send_pong(int fd)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;

    esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ws_pkt);
    if (ret != ESP_OK) { /* failed */ }
}

esp_err_t redirect_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "ACCESS(redirect): %s %s (fd%d)", req->uri, http_method_str(req->method), httpd_req_to_sockfd(req));
    
    char redirect_url[128];
    snprintf(redirect_url, sizeof(redirect_url), "%s?redirect=1", top_page_uri);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    ESP_LOGD(TAG, "ACCESS(404): %s %s (%d)", req->uri, http_method_str(req->method), httpd_req_to_sockfd(req));
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", top_page_uri);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the top page", HTTPD_RESP_USE_STRLEN);

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

void close_session(int fd)
{
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
    httpd_sess_trigger_close(server, fd);
}

void close_client(client_info_t* client)
{
    if(!client) return;
    close_session(client->http_fd);
    close_session(client->ws_fd);
    clear_client_info(client);
}

void close_unused_clients()
{
    // 古いclientをclose
    uint64_t now_us = esp_timer_get_time();
    for(int cli_i = 0; cli_i < MAX_CLIENTS; cli_i++)
    {
        client_info_t* client = &clients_info[cli_i];
        if(!client->active) continue;
        if(now_us - client->last_ping_time_us > 3000000) close_client(client);
    }

    // 管理していないのにhttpdに残っているws_fdをclose
    size_t fds = MAX_SOCKS;
    int alive_fds[MAX_SOCKS] = {0};
    ESP_ERROR_CHECK(httpd_get_client_list(server, &fds, alive_fds));
    for(int fd_i = 0; fd_i < fds; fd_i++)
    {
        int fd = alive_fds[fd_i];
        httpd_ws_client_info_t fd_protocol = httpd_ws_get_fd_info(server, fd);
        if(fd_protocol == HTTPD_WS_CLIENT_INVALID)
        {
            close_session(fd);
        }
        else
        {
            bool is_client = false;
            for(int cli_i = 0; cli_i < MAX_CLIENTS; cli_i++)
            {
                client_info_t* client = &clients_info[cli_i];
                if(!client->active) continue;
                if(fd == client->http_fd) { is_client = true; break; }
                if(fd == client->ws_fd) { is_client = true; break; }
            }
            if(!is_client) close_session(fd);
        }
    }

    // 管理しているけどhttpdにないws_fdをclose
    fds = MAX_SOCKS;
    ESP_ERROR_CHECK(httpd_get_client_list(server, &fds, alive_fds));
    for(int cli_i = 0; cli_i < MAX_CLIENTS; cli_i++)
    {
        client_info_t* client = &clients_info[cli_i];
        if(!client->active) continue;

        bool alive_client = false;
        for(int fd_i = 0; fd_i < fds; fd_i++)
        {
            if(client->http_fd == alive_fds[fd_i]){ alive_client = true; break; }
            if(client->ws_fd == alive_fds[fd_i]){ alive_client = true; break; }
        }
        if(!alive_client) close_client(client);
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
        ESP_LOGW(TAG, "HTTP server: not started");
    } else {
        esp_netif_ip_info_t ap_ip = {0};
        get_current_ap_ip(&ap_ip);
        printf("--------------------------------\n");
        printf("HTTP server: " IPSTR "\n", IP2STR(&ap_ip.ip));
    }
    size_t fds = MAX_SOCKS;
    int clinet_fds[MAX_SOCKS] = {0};
    ESP_ERROR_CHECK(httpd_get_client_list(server, &fds, clinet_fds));
    printf("  %u clinet sockets:", fds);
    for(size_t i = 0U; i < fds; i++)
    {
        httpd_ws_client_info_t fd_protocol = httpd_ws_get_fd_info(server, clinet_fds[i]);
        char fd_protocol_char = (fd_protocol==HTTPD_WS_CLIENT_HTTP? 'H': (fd_protocol==HTTPD_WS_CLIENT_WEBSOCKET? 'W':'x'));
        printf(" %d%c", clinet_fds[i], fd_protocol_char);
    }
    printf("\n");

    esp_netif_ip_info_t sta_ip = {0};
    get_current_sta_ip(&sta_ip);
    printf("IP as STA: " IPSTR "\n", IP2STR(&sta_ip.ip));

    puts("clients:");
    for(int cli_i = 0; cli_i < MAX_CLIENTS; cli_i++)
    {
        if(!clients_info[cli_i].active) continue;
        printf("  " IPSTR " %s(H%d/W%d) tim:%llums\n",
            IP2STR(&clients_info[cli_i].ip), ((clients_info[cli_i].active_page==NULL)? "--" : clients_info[cli_i].active_page->uri),
            clients_info[cli_i].http_fd, clients_info[cli_i].ws_fd,
            clients_info[cli_i].last_ping_time_us / 1000
        );
    }

    printf("--------------------------------\n");
}
