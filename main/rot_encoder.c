#include "rot_encoder.h"

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"

#include "scpi_uart.h"

#define ROT_ENCODER_GPIO_A GPIO_NUM_45
#define ROT_ENCODER_GPIO_B GPIO_NUM_46
#define ROT_ENCODER_GPIO_SW GPIO_NUM_47

#define ROT_ENCODER_QUEUE_SIZE 32
#define ROT_ENCODER_BUTTON_DEBOUNCE_US 50000

typedef enum {
    ROT_ENCODER_EVENT_RIGHT,
    ROT_ENCODER_EVENT_LEFT,
    ROT_ENCODER_EVENT_REMOTE_RIGHT,
    ROT_ENCODER_EVENT_REMOTE_LEFT,
    ROT_ENCODER_EVENT_BUTTON_EDGE,
} rot_encoder_event_type_t;

typedef struct {
    rot_encoder_event_type_t type;
    uint8_t level;
} rot_encoder_event_t;

static const char *TAG = "rot_encoder";
static QueueHandle_t s_rot_encoder_queue = NULL;
static bool s_rot_encoder_initialized = false;
static int8_t s_current_a_level = 0;
static bool s_rot_event_dead_time = false; // チャタリング防止のためAエッジでイベント発生後、Bエッジまでは不感とする
static void (*s_rot_encoder_on_right)(void) = NULL;
static void (*s_rot_encoder_on_left)(void) = NULL;
static void (*s_rot_encoder_on_click)(void) = NULL;

static void rot_encoder_scpi_step_received(int8_t step, void *user_data)
{
    (void)user_data;
    esp_err_t err = rot_encoder_apply_remote_step(step);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not queue remote volume step (%d): %s",
                 (int)step, esp_err_to_name(err));
    }
}

static void IRAM_ATTR rot_encoder_gpio_isr(void *arg)
{
    const uint32_t gpio = (uint32_t) arg;
    rot_encoder_event_t event;
    BaseType_t woken = pdFALSE;

    if (gpio == ROT_ENCODER_GPIO_A) {
        if (s_rot_event_dead_time) return;
        s_rot_event_dead_time = true;

        bool a_raising = (s_current_a_level == 0); // Aエッジ方向はBエッジ時のAレベルで判定する
        int b = gpio_get_level(ROT_ENCODER_GPIO_B);
        if ((a_raising && !b) || (!a_raising && b)) {
            event.type = ROT_ENCODER_EVENT_RIGHT;
        } else {
            event.type = ROT_ENCODER_EVENT_LEFT;
        }
        event.level = 0;
    } else if (gpio == ROT_ENCODER_GPIO_B) {
        s_current_a_level = gpio_get_level(ROT_ENCODER_GPIO_A);
        s_rot_event_dead_time = false;
        return;
    } else if (gpio == ROT_ENCODER_GPIO_SW) {
        event.type = ROT_ENCODER_EVENT_BUTTON_EDGE;
        event.level = (uint8_t) gpio_get_level(ROT_ENCODER_GPIO_SW);
    } else {
        return;
    }

    if (s_rot_encoder_queue != NULL) {
        xQueueSendFromISR(s_rot_encoder_queue, &event, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void rot_encoder_task(void *arg)
{
    (void)arg;
    rot_encoder_event_t event;
    int64_t last_button_time = 0;
    bool button_down = false;

    s_current_a_level = gpio_get_level(ROT_ENCODER_GPIO_A);
    while (true) {
        if (xQueueReceive(s_rot_encoder_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const int64_t now = esp_timer_get_time();

        if (event.type == ROT_ENCODER_EVENT_BUTTON_EDGE) {
            if (now - last_button_time < ROT_ENCODER_BUTTON_DEBOUNCE_US) {
                continue;
            }
            last_button_time = now;

            if (event.level == 0) {
                button_down = true;
            } else if (button_down) {
                button_down = false;
                if (s_rot_encoder_on_click != NULL) {
                    s_rot_encoder_on_click();
                }
            }
            continue;
        }

        if (event.type == ROT_ENCODER_EVENT_RIGHT ||
            event.type == ROT_ENCODER_EVENT_REMOTE_RIGHT) {
            if (event.type == ROT_ENCODER_EVENT_RIGHT) {
                (void)scpi_uart_send_volume_step(1);
            }
            if (s_rot_encoder_on_right != NULL) {
                s_rot_encoder_on_right();
            }
        } else if (event.type == ROT_ENCODER_EVENT_LEFT ||
                   event.type == ROT_ENCODER_EVENT_REMOTE_LEFT) {
            if (event.type == ROT_ENCODER_EVENT_LEFT) {
                (void)scpi_uart_send_volume_step(-1);
            }
            if (s_rot_encoder_on_left != NULL) {
                s_rot_encoder_on_left();
            }
        }
    }
}

void rot_encoder_init(rot_encoder_callback_t on_right,
                      rot_encoder_callback_t on_left,
                      rot_encoder_callback_t on_click)
{
    s_rot_encoder_on_right = on_right;
    s_rot_encoder_on_left = on_left;
    s_rot_encoder_on_click = on_click;
    if (s_rot_encoder_initialized) {
        return;
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ROT_ENCODER_GPIO_A) | (1ULL << ROT_ENCODER_GPIO_B) | (1ULL << ROT_ENCODER_GPIO_SW),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);

    s_rot_encoder_queue = xQueueCreate(ROT_ENCODER_QUEUE_SIZE, sizeof(rot_encoder_event_t));
    if (s_rot_encoder_queue == NULL) {
        return;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return;
    }

    gpio_isr_handler_add(ROT_ENCODER_GPIO_A, rot_encoder_gpio_isr, (void *) ROT_ENCODER_GPIO_A);
    gpio_isr_handler_add(ROT_ENCODER_GPIO_B, rot_encoder_gpio_isr, (void *) ROT_ENCODER_GPIO_B);
    gpio_isr_handler_add(ROT_ENCODER_GPIO_SW, rot_encoder_gpio_isr, (void *) ROT_ENCODER_GPIO_SW);

    if (xTaskCreate(rot_encoder_task, "rot_encoder", 2048, NULL, 5, NULL) != pdPASS) {
        return;
    }

    s_rot_encoder_initialized = true;
    err = scpi_uart_set_step_callback(rot_encoder_scpi_step_received, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SCPI receiver: %s", esp_err_to_name(err));
    }
}

esp_err_t rot_encoder_apply_remote_step(int8_t step)
{
    if (step != 1 && step != -1) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_rot_encoder_initialized || s_rot_encoder_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    rot_encoder_event_t event = {
        .type = (step > 0) ? ROT_ENCODER_EVENT_REMOTE_RIGHT
                           : ROT_ENCODER_EVENT_REMOTE_LEFT,
        .level = 0,
    };
    if (xQueueSend(s_rot_encoder_queue, &event, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
