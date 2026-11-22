UART Echo test - Step 1
======================

The two archives contain only the files to add/replace under each project's
main/ directory. They are overlay archives: keep the other existing files in
main/ unchanged.

Wiring (cross TX/RX; GND common):
  ESP32-S3 GPIO17 (TX)  -> ESP32-P4-NANO GPIO23 (RX)
  ESP32-S3 GPIO18 (RX)  <- ESP32-P4-NANO GPIO22 (TX)
  ESP32-S3 GND          -- ESP32-P4-NANO GND

UART settings:
  UART1, 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.

Apply to each project:
  1. Extract the corresponding archive into the project root (so files land
     in main/ and the existing main/ files are not deleted).
  2. From the project root, apply the small main.c integration patch:
       git apply main/main.c.patch
     Alternatively, make the two indicated edits manually if the local main.c
     has changed since the base branch snapshot.
  3. Build and flash both boards.

Expected behavior:
  - NANO sends "Hello from NANO!" (LF terminated) every 2 seconds.
  - S3 prints each received line to its standard ESP-IDF log and echoes it.
  - NANO prints the echoed line to its standard ESP-IDF log.

Use each board's existing USB debug connection / monitor to view ESP_LOGI
output. The UART link is separate from the USB debug serial interface.

Note: The archives are based on these branch snapshots:
  NANO: lelt0/ESP32P4-LVGL-touch-UI @ feature/scpi
  S3:   lelt0/ESP32_WifiUI @ feature/expand_board_scpi

The code was reviewed against the repository source and ESP-IDF 6.x UART API,
but a board-specific ESP-IDF build/flash test has not been run in this session.
