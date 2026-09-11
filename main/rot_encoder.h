#ifndef MAIN_ROT_ENCODER_H
#define MAIN_ROT_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rot_encoder_callback_t)(void);

void rot_encoder_init(rot_encoder_callback_t on_right,
                      rot_encoder_callback_t on_left,
                      rot_encoder_callback_t on_click);

#ifdef __cplusplus
}
#endif

#endif // MAIN_ROT_ENCODER_H
