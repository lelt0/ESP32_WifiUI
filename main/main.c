#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "wifi_ui.h"
#include "string.h"

static const char *TAG = "example";

void time_task(void *arg) {
    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        char buf[64];
        snprintf(buf, sizeof(buf), "t%02d", count++);
        ESP_LOGI(TAG, "send: %s", buf);
        send_all(buf, strlen(buf));
    }
}

void app_main(void)
{
    wifiui_config_t config;
    wifiui_start(&config);

    xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
}