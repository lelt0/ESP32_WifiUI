#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../wifiui_element_base_.h"

typedef struct {
    wifiui_element_t common;
    void (*on_connect)(bool);
    bool ssid_scanning;
} wifiui_element_apConnectForm_t;

const wifiui_element_apConnectForm_t * wifiui_element_ap_connect_form(void (*on_connect_callback)(bool));

#ifdef __cplusplus
}
#endif
