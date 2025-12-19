#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bl0937_overcurrent_cb_t)(void *ctx, float current_a);

void bl0937_init(void);
void bl0937_start(void);
void bl0937_stop(void);
void bl0937_set_overcurrent_callback(bl0937_overcurrent_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
