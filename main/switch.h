#ifndef __SWITCH_H__
#define __SWITCH_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void switch_init(void (*on_click)(void));
void switch_set_remote_state(bool on);

#ifdef __cplusplus
}
#endif

#endif
