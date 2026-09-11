#include "switch.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"

#define SWITCH_GPIO GPIO_NUM_0
#define SWITCH_DEBOUNCE_US 20000

typedef struct {
    uint32_t level;
} switch_event_t;

static QueueHandle_t s_switch_queue = NULL;
static bool s_switch_initialized = false;
static void (*s_switch_on_click)(void) = NULL;

static void IRAM_ATTR switch_gpio_isr(void *arg)
{
    switch_event_t event = { .level = gpio_get_level(SWITCH_GPIO) };
    BaseType_t woken = pdFALSE;

    if (s_switch_queue != NULL) {
        xQueueSendFromISR(s_switch_queue, &event, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void switch_task(void *arg)
{
    (void)arg;
    switch_event_t event;
    int64_t last_event_time = 0;
    int last_level = 1;  // プルアップなので初期値は HIGH

    while (true) {
        if (xQueueReceive(s_switch_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const int64_t now = esp_timer_get_time();
        if (now - last_event_time < SWITCH_DEBOUNCE_US) {
            continue;
        }
        last_event_time = now;

        // 立ち上がりエッジ（前がLOW、今がHIGH）のみでコールバック
        if (last_level == 0 && event.level == 1) {
            if (s_switch_on_click != NULL) {
                s_switch_on_click();
            }
        }

        last_level = event.level;
    }
}

void switch_init(void (*on_click)(void))
{
    if (s_switch_initialized) {
        return;
    }

    s_switch_on_click = on_click;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << SWITCH_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_pull_mode(SWITCH_GPIO, GPIO_PULLUP_ONLY);
    gpio_hold_dis(SWITCH_GPIO);

    s_switch_queue = xQueueCreate(8, sizeof(switch_event_t));
    if (s_switch_queue == NULL) {
        return;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return;
    }

    gpio_isr_handler_add(SWITCH_GPIO, switch_gpio_isr, NULL);
    xTaskCreate(switch_task, "switch_task", 2048, NULL, 5, NULL);
    s_switch_initialized = true;
}
