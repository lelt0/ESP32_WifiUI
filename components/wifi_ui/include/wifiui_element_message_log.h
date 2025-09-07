#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct wifiui_element_msglog wifiui_element_msglog_t;
struct wifiui_element_msglog {
    wifiui_element_t common;
    void (*print_message)(const wifiui_element_msglog_t* self, const char* message);
};

const wifiui_element_msglog_t * wifiui_element_message_log(bool mirror_log_mode);

#ifdef __cplusplus
}
#endif
