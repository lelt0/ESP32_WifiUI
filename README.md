# ESP32-S3 Expand Board / ESP32-P4-NANO UART control

This project exchanges rotary-encoder operations and switch state between an
ESP32-S3 Expand Board and an ESP32-P4-NANO touch UI over UART1.

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
- Line ending: LF (`\n`); CR is ignored by the receiver

## SCPI commands

Each command is sent as one LF-terminated line.

| Command | Meaning |
| --- | --- |
| `:VOLUME:STEP +1` | Apply one positive volume step |
| `:VOLUME:STEP -1` | Apply one negative volume step |
| `:SWITCH:STATE 0` | Set the switch state to OFF |
| `:SWITCH:STATE 1` | Set the switch state to ON |

Volume commands are relative operations. They do not transfer an absolute
volume value. Switch commands transfer an explicit state (`0` or `1`).

## Behavior

### Volume

- Turning the S3 rotary encoder performs its existing local menu/LED action
  and sends the corresponding `:VOLUME:STEP` command to the NANO.
- A volume step received from the NANO is placed on the rotary-encoder event
  queue and follows the same right/left callback path as a physical turn.
  Remote steps are not transmitted back to the NANO.
- The two devices exchange relative `+1` / `-1` operations, not a shared
  absolute value.
- Holding the center of the NANO dial resets only the NANO dial and its
  `Value` label to 0; it does not reset the S3 or send a reset command.

### Switch

- Pressing the S3 board's tact switch toggles the S3-side switch state, invokes
  the existing application callback, and sends the resulting state to the
  NANO using `:SWITCH:STATE 0` or `:SWITCH:STATE 1`.
- A state received from the NANO is applied to the S3-side state. When the
  state changes, the existing application callback is invoked so the board's
  LED follows the NANO switch.
- A state received from the NANO is not transmitted back. Likewise, the NANO
  does not retransmit a state it received from the S3.
- Both sides are initialized to OFF. There is no separate startup
  state-negotiation or handshake command.

## Build and test

Build and flash this project together with the companion ESP32-P4-NANO project.
Monitor the ESP-IDF log and check the `scpi_uart` `TX` / `RX` records.

1. Turn the S3 rotary encoder in both directions and confirm that the NANO
   dial follows.
2. Turn the NANO dial in both directions and confirm that the S3 follows.
3. Change the NANO GUI switch to ON and OFF and confirm that the S3 LED follows.
4. Press the S3 tact switch and confirm that the NANO GUI switch follows.
5. Confirm that received commands are not echoed back as new commands.

The source changes are supplied separately as files for the S3 project's
`main/` directory. The existing `main/CMakeLists.txt` must remain unchanged;
in particular, keep this registration line:

```cmake
idf_component_register(SRCS main.c rot_encoder.c switch.c lcd_char2x8.c scpi_uart.c)
```

## Companion project

- NANO: [lelt0/ESP32P4-LVGL-touch-UI](https://github.com/lelt0/ESP32P4-LVGL-touch-UI/tree/feature/scpi)
- S3: [lelt0/ESP32_WifiUI](https://github.com/lelt0/ESP32_WifiUI/tree/feature/expand_board_scpi)

The Step 4 source changes have not been built or tested on physical hardware
as part of this README update.
