#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void* arg;
} wifiui_config_t;

void wifiui_start(wifiui_config_t *config);
bool send_all(const char *message, size_t len);

#ifdef __cplusplus
}
#endif
