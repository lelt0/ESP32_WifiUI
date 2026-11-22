#ifndef SCPI_UART_H
#define SCPI_UART_H

#include <stdint.h>
#include "esp_err.h"

/** Initialize UART1 and start the line-oriented UART echo task. */
esp_err_t scpi_uart_init(void);

/** Send the current physical rotary-encoder volume value to the NANO. */
esp_err_t scpi_uart_send_volume(int32_t value);

#endif /* SCPI_UART_H */
