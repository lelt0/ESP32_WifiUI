#ifndef SCPI_UART_H
#define SCPI_UART_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef void (*scpi_uart_step_cb_t)(int8_t step, void *user_data);
typedef void (*scpi_uart_switch_cb_t)(bool on, void *user_data);

esp_err_t scpi_uart_set_step_callback(scpi_uart_step_cb_t callback, void *user_data);
esp_err_t scpi_uart_set_switch_callback(scpi_uart_switch_cb_t callback, void *user_data);
esp_err_t scpi_uart_send_volume_step(int8_t step);
esp_err_t scpi_uart_send_switch_state(bool on);
esp_err_t scpi_uart_init(void);

#endif
