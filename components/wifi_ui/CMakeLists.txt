file(GLOB_RECURSE WIFI_UI_SRCS
    "*.c"
)
idf_component_register(SRCS ${WIFI_UI_SRCS}
                       INCLUDE_DIRS include
                       PRIV_REQUIRES esp_http_server esp_netif nvs_flash esp_wifi spiffs)
