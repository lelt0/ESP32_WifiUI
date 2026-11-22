#ifndef MAIN_ROT_ENCODER_H
#define MAIN_ROT_ENCODER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rot_encoder_callback_t)(void);

void rot_encoder_init(rot_encoder_callback_t on_right,
                      rot_encoder_callback_t on_left,
                      rot_encoder_callback_t on_click);

/** Queue one remote +1/-1 operation through the same callback path as the
 * physical rotary encoder, without transmitting the operation back. */
esp_err_t rot_encoder_apply_remote_step(int8_t step);

#ifdef __cplusplus
}
#endif

#endif // MAIN_ROT_ENCODER_H
