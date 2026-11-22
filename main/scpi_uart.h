#ifndef SCPI_UART_H
#define SCPI_UART_H

#include <stdint.h>
#include "esp_err.h"

typedef void (*scpi_uart_step_cb_t)(int8_t step, void *user_data);

/** Register the step callback; starts RX when UART is configured. */
esp_err_t scpi_uart_set_step_callback(scpi_uart_step_cb_t callback,
                                      void *user_data);

/** Send one relative volume operation (+1 or -1) to the NANO. */
esp_err_t scpi_uart_send_volume_step(int8_t step);

/** Initialize UART1 and start the SCPI line receiver. */
esp_err_t scpi_uart_init(void);

#endif /* SCPI_UART_H */
