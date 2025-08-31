#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

#include "wifiui_server.h"
#include "wifiui_element_stext.h"
#include "wifiui_element_button.h"
#include "wifiui_element_dtext.h"

static const char *TAG = "example";

// static void print_memory(void)
// {
//     UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
//     ESP_LOGD(TAG, "Stack high water mark: %u words\n", (unsigned int)high_water_mark);
//     size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
//     size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
//     ESP_LOGD(TAG, "Current free heap: %u bytes\n", (unsigned int)free_heap);
//     ESP_LOGD(TAG, "Minimum free heap ever: %u bytes\n", (unsigned int)min_free_heap);
// }

static vprintf_like_t s_orig_vprintf = NULL;  // オリジナルのログ関数ポインタ
static bool s_in_my_log = false;              // ログ関数再帰防止フラグ
static int my_log_vprintf(const char *fmt, va_list args)
{
//     if (s_in_my_log) {
        // すでに自分の中なら、オリジナルログ関数だけ呼んで終わり
        return s_orig_vprintf(fmt, args);
//     }

//     s_in_my_log = true;

//     static char buf[256];
//     va_list args_copy;
//     va_copy(args_copy, args);
//     int len = vsnprintf(buf, sizeof(buf), fmt, args_copy);
//     va_end(args_copy);

//     if (len > 0) {
//         send_log(buf, len);
//     }

//     // 元のログ関数でUART出力
//     int ret = s_orig_vprintf(fmt, args);

//     s_in_my_log = false;
//     return ret;
}

#define LED_GPIO 19
static bool led_status = true;
void toggle_led(const wifiui_element_button_t * dummy, void* arg) {
    led_status = !led_status;
    gpio_set_level(LED_GPIO, led_status);
    ESP_LOGI(TAG, "LED toggled to %s", led_status ? "ON" : "OFF");
}

// void status_send_task(void *arg) {
//     int count = 0;
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(1000));

//         // 起動時間取得
//         double time = esp_timer_get_time() / 1000000.0;

//         // STA IP取得
//         esp_netif_ip_info_t ip_info = {0};
//         get_current_sta_ip(&ip_info);

//         // 出力
//         static char buf[64];
//         snprintf(buf, sizeof(buf), "time: %.3lfsec\nLED: %s\nSTA IP: " IPSTR, time, (led_status?"ON":"OFF"), IP2STR(&ip_info.ip));
//         send_status(buf, strlen(buf));
//         ESP_LOGI(TAG, "sent: %.3lfs %d " IPSTR, time, led_status, IP2STR(&ip_info.ip));

//         if(count%10==0) print_connections(TAG);

//         count++;
//     }
// }

const wifiui_element_dtext_t* dtext1 = NULL;
void status_send_task(void *arg) {
    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if(dtext1 != NULL)
        {
            char update_text[64];
            snprintf(update_text, 64, "This is dynamic text.\nChanged!%d", count);
            dtext1->change_text(dtext1, update_text);
        }
        printf("[yatadebug] %p %d\n", dtext1, count);

        count++;
    }
}

void app_main(void)
{
    s_orig_vprintf = esp_log_set_vprintf(my_log_vprintf); // シリアル出力を盗んでWebSocketでも送信するようにする

    const wifiui_element_t* elements[3];
    elements[0] = (const wifiui_element_t*) wifiui_element_static_text("Hello, World!");
    elements[1] = (const wifiui_element_t*) wifiui_element_button("CLICK", toggle_led, NULL);
    elements[2] = (const wifiui_element_t*) (dtext1 = wifiui_element_dynamic_text("This is dynamic text.\nABCDEFG"));
    const wifiui_page_t* index_page = wifiui_create_page("index", (void**)elements, 3);
    char * html = wifiui_generate_page_html(index_page);
    printf("HTML: %s\n", html);
    free(html);

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, led_status);

    wifiui_start();

    xTaskCreate(status_send_task, "status_send_task", 4096, NULL, 5, NULL);
}