#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_serialLog wifiui_element_serialLog_t;
struct wifiui_element_serialLog {
    wifiui_element_t common;
    void (*print)(const wifiui_element_serialLog_t* self, const char* message);
};

const wifiui_element_serialLog_t * wifiui_element_serial_log(bool mirror_log_mode);

#ifdef __cplusplus
}
#endif
