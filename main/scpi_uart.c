#include "scpi_uart.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
                ESP_LOGI(TAG, "RX: %s", line_buffer);

                /* Keep the original UART echo behavior for diagnostics. */
                if (line_length > 0) {
                    int tx_len = uart_write_bytes(SCPI_UART_PORT,
                                                  line_buffer,
                                                  line_length);
                    if (tx_len < 0) {
                        ESP_LOGE(TAG, "UART echo data TX failed");
                    }
                }
                if (uart_write_bytes(SCPI_UART_PORT, "\n", 1) < 0) {
                    ESP_LOGE(TAG, "UART echo terminator TX failed");
                } else {
                    ESP_LOGI(TAG, "TX (echo): %s", line_buffer);
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

esp_err_t scpi_uart_send_volume(int32_t value)
{
    char command[32];
    int length = snprintf(command, sizeof(command), ":VOLUME %ld\n", (long)value);
    if (length < 0 || length >= (int)sizeof(command)) {
        return ESP_ERR_INVALID_SIZE;
    }

    int written = uart_write_bytes(SCPI_UART_PORT, command, (size_t)length);
    if (written != length) {
        ESP_LOGE(TAG, "Volume TX failed (written=%d, expected=%d)",
                 written, length);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TX: %.*s", length - 1, command);
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

    if (xTaskCreate(scpi_uart_rx_task,
                    "scpi_uart_rx",
                    3072,
                    NULL,
                    5,
                    NULL) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "Failed to create UART RX task");
        goto error;
    }

    ESP_LOGI(TAG,
             "UART1 ready: TX=GPIO%d, RX=GPIO%d, baud=%d",
             SCPI_UART_TX_GPIO,
             SCPI_UART_RX_GPIO,
             SCPI_UART_BAUD_RATE);
    return ESP_OK;

error:
    uart_driver_delete(SCPI_UART_PORT);
    return err;
}
