#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../wifiui_page.h"

void wifiui_start(const wifiui_page_t* top_page);

void wifiui_ws_send_data_async(const char* data, size_t len);

#ifdef __cplusplus
}
#endif
