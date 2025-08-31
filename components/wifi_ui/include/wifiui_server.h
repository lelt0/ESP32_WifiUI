#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../wifiui_page.h"

void wifiui_start();

void wifiui_ws_send_data_async(const char* data, size_t len);

#ifdef __cplusplus
}
#endif
