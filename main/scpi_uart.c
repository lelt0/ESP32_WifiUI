#include "scpi_uart.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCPI_UART_PORT       UART_NUM_1
#define SCPI_UART_TX_GPIO    GPIO_NUM_17
#define SCPI_UART_RX_GPIO    GPIO_NUM_18
#define SCPI_UART_BAUD_RATE  115200
#define SCPI_UART_BUF_SIZE   256
#define SCPI_UART_LINE_SIZE  128

static const char *TAG = "scpi_uart";
static scpi_uart_step_cb_t s_step_callback = NULL;
static void *s_step_user_data = NULL;
static bool s_uart_configured = false;
static bool s_rx_task_started = false;

static void scpi_uart_rx_task(void *arg);

static esp_err_t scpi_uart_start_rx_task(void)
{
    if (!s_uart_configured || s_rx_task_started || s_step_callback == NULL) {
        return ESP_OK;
    }

    if (xTaskCreate(scpi_uart_rx_task,
                    "scpi_uart_rx",
                    3072,
                    NULL,
                    5,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART RX task");
        return ESP_ERR_NO_MEM;
    }

    s_rx_task_started = true;
    ESP_LOGI(TAG, "UART SCPI receiver task started");
    return ESP_OK;
}

esp_err_t scpi_uart_set_step_callback(scpi_uart_step_cb_t callback,
                                      void *user_data)
{
    s_step_callback = callback;
    s_step_user_data = user_data;
    return scpi_uart_start_rx_task();
}

static bool scpi_uart_parse_step(const char *line, int8_t *step)
{
    static const char prefix[] = ":VOLUME:STEP ";
    if (strncmp(line, prefix, sizeof(prefix) - 1) != 0) {
        return false;
    }

    const char *argument = line + sizeof(prefix) - 1;
    if (strcmp(argument, "+1") == 0) {
        *step = 1;
        return true;
    }
    if (strcmp(argument, "-1") == 0) {
        *step = -1;
        return true;
    }

    ESP_LOGW(TAG, "Invalid volume step command: %s", line);
    return false;
}

static void scpi_uart_process_line(const char *line)
{
    int8_t step;
    if (scpi_uart_parse_step(line, &step)) {
        ESP_LOGI(TAG, "RX: %s", line);
        if (s_step_callback != NULL) {
            s_step_callback(step, s_step_user_data);
        }
        return;
    }

    /* Preserve the text-console echo for non-SCPI diagnostic input only. */
    ESP_LOGI(TAG, "RX (console): %s", line);
    size_t length = strlen(line);
    if (length > 0 && uart_write_bytes(SCPI_UART_PORT, line, length) < 0) {
        ESP_LOGE(TAG, "UART console echo data TX failed");
    }
    if (uart_write_bytes(SCPI_UART_PORT, "\n", 1) < 0) {
        ESP_LOGE(TAG, "UART console echo terminator TX failed");
    }
}

static void scpi_uart_rx_task(void *arg)
{
    (void)arg;

    uint8_t rx_buffer[64];
    char line_buffer[SCPI_UART_LINE_SIZE];
    size_t line_length = 0;

    while (true) {
        int rx_len = uart_read_bytes(SCPI_UART_PORT,
                                     rx_buffer,
                                     sizeof(rx_buffer),
                                     pdMS_TO_TICKS(100));
        if (rx_len <= 0) {
            continue;
        }

        for (int i = 0; i < rx_len; ++i) {
            const uint8_t ch = rx_buffer[i];
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                line_buffer[line_length] = '\0';
                if (line_length > 0) {
                    scpi_uart_process_line(line_buffer);
                }
                line_length = 0;
                continue;
            }
            if (line_length < sizeof(line_buffer) - 1) {
                line_buffer[line_length++] = (char)ch;
            } else {
                ESP_LOGW(TAG,
                         "Received line too long (max %u bytes); discarding",
                         (unsigned)(sizeof(line_buffer) - 1));
                line_length = 0;
            }
        }
    }
}

esp_err_t scpi_uart_send_volume_step(int8_t step)
{
    if (step != 1 && step != -1) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *command = (step > 0) ? ":VOLUME:STEP +1\n"
                                    : ":VOLUME:STEP -1\n";
    const size_t length = strlen(command);
    int written = uart_write_bytes(SCPI_UART_PORT, command, length);
    if (written != (int)length) {
        ESP_LOGE(TAG, "Volume step TX failed (written=%d, expected=%u)",
                 written, (unsigned)length);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TX: %.*s", (int)length - 1, command);
    return ESP_OK;
}

esp_err_t scpi_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = SCPI_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(SCPI_UART_PORT,
                                        SCPI_UART_BUF_SIZE,
                                        SCPI_UART_BUF_SIZE,
                                        0,
                                        NULL,
                                        0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(SCPI_UART_PORT, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        goto error;
    }

    err = uart_set_pin(SCPI_UART_PORT,
                       SCPI_UART_TX_GPIO,
                       SCPI_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        goto error;
    }

    s_uart_configured = true;

    ESP_LOGI(TAG,
             "UART1 configured: TX=GPIO%d, RX=GPIO%d, baud=%d",
             SCPI_UART_TX_GPIO,
             SCPI_UART_RX_GPIO,
             SCPI_UART_BAUD_RATE);
    /* RX starts when the rotary-encoder event queue/callback is ready. */
    err = scpi_uart_start_rx_task();
    if (err != ESP_OK) {
        goto error;
    }
    return ESP_OK;

error:
    s_uart_configured = false;
    uart_driver_delete(SCPI_UART_PORT);
    return err;
}
