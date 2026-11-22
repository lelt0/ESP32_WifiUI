#ifndef SCPI_UART_H
#define SCPI_UART_H

#include "esp_err.h"

/** Initialize UART1 and start the line-oriented UART echo task. */
esp_err_t scpi_uart_init(void);

#endif /* SCPI_UART_H */
