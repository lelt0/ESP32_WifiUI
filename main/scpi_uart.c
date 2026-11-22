#include "scpi_uart.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCPI_UART_PORT UART_NUM_1
#define SCPI_UART_TX_GPIO GPIO_NUM_17
#define SCPI_UART_RX_GPIO GPIO_NUM_18
#define SCPI_UART_BAUD_RATE 115200
#define SCPI_UART_BUF_SIZE 256
#define SCPI_UART_LINE_SIZE 128

static const char *TAG = "scpi_uart";
static scpi_uart_step_cb_t s_step_callback;
static void *s_step_user_data;
static scpi_uart_switch_cb_t s_switch_callback;
static void *s_switch_user_data;
static bool s_uart_configured;
static bool s_rx_task_started;

static void scpi_uart_rx_task(void *arg);

static esp_err_t scpi_uart_start_rx_task(void)
{
    if (!s_uart_configured || s_rx_task_started || (!s_step_callback && !s_switch_callback)) return ESP_OK;
    if (xTaskCreate(scpi_uart_rx_task, "scpi_uart_rx", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART RX task");
        return ESP_ERR_NO_MEM;
    }
    s_rx_task_started = true;
    return ESP_OK;
}

esp_err_t scpi_uart_set_step_callback(scpi_uart_step_cb_t callback, void *user_data)
{
    s_step_callback = callback;
    s_step_user_data = user_data;
    return scpi_uart_start_rx_task();
}

esp_err_t scpi_uart_set_switch_callback(scpi_uart_switch_cb_t callback, void *user_data)
{
    s_switch_callback = callback;
    s_switch_user_data = user_data;
    return scpi_uart_start_rx_task();
}

static void scpi_uart_process_line(const char *line)
{
    static const char step_prefix[] = ":VOLUME:STEP ";
    static const char state_prefix[] = ":SWITCH:STATE ";
    if (strncmp(line, step_prefix, sizeof(step_prefix) - 1) == 0) {
        const char *arg = line + sizeof(step_prefix) - 1;
        int8_t step;
        if (strcmp(arg, "+1") == 0) step = 1;
        else if (strcmp(arg, "-1") == 0) step = -1;
        else { ESP_LOGW(TAG, "Invalid volume step: %s", line); return; }
        ESP_LOGI(TAG, "RX: %s", line);
        if (s_step_callback) s_step_callback(step, s_step_user_data);
        return;
    }
    if (strncmp(line, state_prefix, sizeof(state_prefix) - 1) == 0) {
        const char *arg = line + sizeof(state_prefix) - 1;
        if (strcmp(arg, "0") != 0 && strcmp(arg, "1") != 0) {
            ESP_LOGW(TAG, "Invalid switch state: %s", line);
            return;
        }
        ESP_LOGI(TAG, "RX: %s", line);
        if (s_switch_callback) s_switch_callback(arg[0] == '1', s_switch_user_data);
        return;
    }
    ESP_LOGW(TAG, "Unknown command: %s", line);
}

static void scpi_uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t rx[64];
    char line[SCPI_UART_LINE_SIZE];
    size_t used = 0;
    while (true) {
        int n = uart_read_bytes(SCPI_UART_PORT, rx, sizeof(rx), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; ++i) {
            char ch = (char)rx[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                line[used] = '\0';
                if (used) scpi_uart_process_line(line);
                used = 0;
            } else if (used < sizeof(line) - 1) line[used++] = ch;
            else { ESP_LOGW(TAG, "Command too long; discarded"); used = 0; }
        }
    }
}

static esp_err_t scpi_uart_write(const char *command)
{
    size_t len = strlen(command);
    int written = uart_write_bytes(SCPI_UART_PORT, command, len);
    if (written != (int)len) return ESP_FAIL;
    ESP_LOGI(TAG, "TX: %.*s", (int)len - 1, command);
    return ESP_OK;
}

esp_err_t scpi_uart_send_volume_step(int8_t step)
{
    if (step != 1 && step != -1) return ESP_ERR_INVALID_ARG;
    return scpi_uart_write(step > 0 ? ":VOLUME:STEP +1\n" : ":VOLUME:STEP -1\n");
}

esp_err_t scpi_uart_send_switch_state(bool on)
{
    return scpi_uart_write(on ? ":SWITCH:STATE 1\n" : ":SWITCH:STATE 0\n");
}

esp_err_t scpi_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = SCPI_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(SCPI_UART_PORT, SCPI_UART_BUF_SIZE, SCPI_UART_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(SCPI_UART_PORT, &config);
    if (err != ESP_OK) goto fail;
    err = uart_set_pin(SCPI_UART_PORT, SCPI_UART_TX_GPIO, SCPI_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) goto fail;
    s_uart_configured = true;
    ESP_LOGI(TAG, "UART1 configured: TX=GPIO%d RX=GPIO%d baud=%d",
             SCPI_UART_TX_GPIO, SCPI_UART_RX_GPIO, SCPI_UART_BAUD_RATE);
    err = scpi_uart_start_rx_task();
    if (err != ESP_OK) goto fail;
    return ESP_OK;
fail:
    s_uart_configured = false;
    uart_driver_delete(SCPI_UART_PORT);
    return err;
}
