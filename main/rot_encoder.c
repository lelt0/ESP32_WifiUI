#include "rot_encoder.h"

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"

#define ROT_ENCODER_GPIO_A GPIO_NUM_45
#define ROT_ENCODER_GPIO_B GPIO_NUM_46
#define ROT_ENCODER_GPIO_SW GPIO_NUM_47

#define ROT_ENCODER_QUEUE_SIZE 8
#define ROT_ENCODER_BUTTON_DEBOUNCE_US 50000

typedef enum {
    ROT_ENCODER_EVENT_RIGHT,
    ROT_ENCODER_EVENT_LEFT,
    ROT_ENCODER_EVENT_BUTTON_EDGE,
} rot_encoder_event_type_t;

typedef struct {
    rot_encoder_event_type_t type;
    uint8_t level;
} rot_encoder_event_t;

static QueueHandle_t s_rot_encoder_queue = NULL;
static bool s_rot_encoder_initialized = false;
static int8_t s_current_a_level = 0;
static bool s_rot_event_dead_time = false; // チャタリング防止のためAエッジでボリュームイベント発生後、Bエッジまでは不感とする
static void (*s_rot_encoder_on_right)(void) = NULL;
static void (*s_rot_encoder_on_left)(void) = NULL;
static void (*s_rot_encoder_on_click)(void) = NULL;

static void IRAM_ATTR rot_encoder_gpio_isr(void *arg)
{
    const uint32_t gpio = (uint32_t) arg;
    rot_encoder_event_t event;
    BaseType_t woken = pdFALSE;

    if (gpio == ROT_ENCODER_GPIO_A) {
        if(s_rot_event_dead_time) return;
        s_rot_event_dead_time = true;
        
        bool a_raising = (s_current_a_level == 0); // Aエッジが立ち上がりかどうかはBエッジのときのAレベルを用いて判定する. チャタリングによってエッジ付近はレベルが揺れるのでここでAレベルは読まない
        int b = gpio_get_level(ROT_ENCODER_GPIO_B);
        if((a_raising && !b) || (!a_raising && b)) {
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
    (void) arg;
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
                s_rot_encoder_on_click();
            }
            continue;
        }

        if (event.type == ROT_ENCODER_EVENT_RIGHT) {
            if (s_rot_encoder_on_right) {
                s_rot_encoder_on_right();
            }
        } else if (event.type == ROT_ENCODER_EVENT_LEFT) {
            if (s_rot_encoder_on_left) {
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

    xTaskCreate(rot_encoder_task, "rot_encoder", 2048, NULL, 5, NULL);
    s_rot_encoder_initialized = true;
}
