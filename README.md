# ESP32-P4-NANO / ESP32-S3 UART volume control

This archive contains the ESP32-S3-side files to add or replace under `main/`.
Extract the archive into the **ESP32-S3 project root**. Only the listed files
are included; other files in `main/` remain untouched. The existing
`main/CMakeLists.txt` is intentionally unchanged. This README replaces the
older root-level `README_UART_ECHO.txt`; remove that old file from the S3
repository after extracting this archive if it is still present.

## Wiring

Connect the boards using crossed TX/RX lines and a common ground:

| ESP32-S3 Expand Board | ESP32-P4-NANO |
| --- | --- |
| GPIO17 (UART1 TX) | GPIO23 (UART1 RX) |
| GPIO18 (UART1 RX) | GPIO22 (UART1 TX) |
| GND | GND |

Both interfaces use 3.3 V TTL. Do not connect 5 V UART signals.

## UART settings

- UART: UART1
- Baud rate: 115200 bit/s
- Format: 8 data bits, no parity, 1 stop bit (8N1)
- Flow control: disabled
- Line ending: LF (`\n`); CRLF is also accepted

## SCPI command list

Commands are sent as one LF-terminated line:

| Command | Effect |
| --- | --- |
| `:VOLUME:STEP +1` | Apply one positive volume step |
| `:VOLUME:STEP -1` | Apply one negative volume step |

The command represents an operation, not an absolute value. The receiver
applies the same direction of operation as a right/left turn of the S3 rotary
encoder. The sender does not transmit its current dial value.

## Behavior

- Turning the S3 rotary encoder performs its existing local menu/LED action
  and sends the corresponding step to the NANO GUI dial.
- A step received from the NANO is placed on the rotary-encoder event queue
  and runs the same right/left callback path as a physical turn. It is not
  transmitted back to the NANO.
- Turning the NANO GUI dial sends one command for each value step to the S3.
- Holding the center of the NANO dial resets only the NANO dial and its `Value`
  label to 0; it does not reset the S3 or send a reset command.
- The two devices exchange relative `+1` / `-1` operations, not a shared
  absolute value.

## Build and test

Build and flash this project and the companion NANO project. Monitor each
board's ESP-IDF log to see `TX` and `RX` records from `scpi_uart`. Test both
ends of the dial/encoder and confirm that valid SCPI commands do not bounce
back and forth.

The S3 UART RX task starts after the rotary-encoder event queue and callback
are ready, so early NANO commands remain buffered by the UART driver during
S3 application startup. The S3 still echoes non-SCPI diagnostic text lines.
Valid `:VOLUME:STEP` commands are consumed and are not echoed.

## Source branches used

- NANO: [lelt0/ESP32P4-LVGL-touch-UI](https://github.com/lelt0/ESP32P4-LVGL-touch-UI/tree/feature/scpi)
- S3: [lelt0/ESP32_WifiUI](https://github.com/lelt0/ESP32_WifiUI/tree/feature/expand_board_scpi)

The archive is based on NANO commit `1a7d9e7a3df70500d85866df92f94db68cf34aa6` and S3 commit `573dbb5a9b70e675b2b2e0e0122a9b2857ed73d3`.
The sources were reviewed, but no ESP-IDF build, flash, or physical-board test
was performed while preparing this archive.
