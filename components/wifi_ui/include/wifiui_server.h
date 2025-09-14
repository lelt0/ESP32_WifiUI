#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../wifiui_page.h"
#include "esp_wifi.h"

void wifiui_start(const wifiui_page_t* top_page);

void wifiui_ws_send_data_async(const char* data, size_t len, const wifiui_element_t* element_info);

void wifiui_start_ssid_scan();
void wifiui_set_ssid_scan_callback(void (*callback)(void*), void* arg);

esp_err_t wifiui_connect_to_ap(const char* ssid, const char* password, wifi_auth_mode_t auth_mode);
void wifiui_set_ap_connected_callback(void (*callback)(void* arg, uint32_t ip_addr), void* arg);
void wifiui_set_ap_disconnected_callback(void (*callback)(void* arg, uint8_t reason), void* arg);

void wifiui_print_server_status();

#ifdef __cplusplus
}
#endif
