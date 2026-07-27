file(GLOB_RECURSE WIFI_UI_SRCS
    "*.c"
)
idf_component_register(SRCS ${WIFI_UI_SRCS}
                       INCLUDE_DIRS include
                       PRIV_REQUIRES esp_http_server esp_netif nvs_flash esp_wifi vfs esp_timer)

option(USE_PLOTY "use embedded Ploty.js for graph rendering" ON)
if(USE_PLOTY)
    target_add_binary_data(${COMPONENT_LIB} "./assets/ploty.min.js.gz" BINARY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFIUI_USE_PLOTY=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFIUI_USE_PLOTY=0)
endif()
