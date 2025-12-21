/**
   Copyright 2026 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   for more information visit https://www.studiopieters.nl
 **/

#include "custom_characteristics.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Custom UUIDs (keep these stable once shipped)
// -----------------------------------------------------------------------------
#define UUID_VOLTAGE "E863F10A-079E-48FF-8F27-9C2605A29F52"
#define UUID_CURRENT "E863F126-079E-48FF-8F27-9C2605A29F52"
#define UUID_POWER   "E863F10D-079E-48FF-8F27-9C2605A29F52"
#define UUID_ENERGY  "E863F10C-079E-48FF-8F27-9C2605A29F52"

// -----------------------------------------------------------------------------
// Characteristics (manual init — no macros, no override-init warnings)
// -----------------------------------------------------------------------------
static homekit_value_t hk_get_voltage(void);
static homekit_value_t hk_get_current(void);
static homekit_value_t hk_get_power(void);
static homekit_value_t hk_get_energy(void);

homekit_characteristic_t ch_voltage = {
    .type = UUID_VOLTAGE,
    .description = "Voltage (V)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
    .getter = hk_get_voltage,
};

homekit_characteristic_t ch_current = {
    .type = UUID_CURRENT,
    .description = "Current (A)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
    .getter = hk_get_current,
};

homekit_characteristic_t ch_power = {
    .type = UUID_POWER,
    .description = "Power (W)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
    .getter = hk_get_power,
};

homekit_characteristic_t ch_energy = {
    .type = UUID_ENERGY,
    .description = "Energy (Wh)",
    .format = homekit_format_float,
    .permissions = homekit_permissions_paired_read | homekit_permissions_notify,
    .value = HOMEKIT_FLOAT(0.0f),
    .getter = hk_get_energy,
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline float sane_float(float x) {
    if (isnan(x) || isinf(x)) return 0.0f;
    return x;
}

typedef struct {
    homekit_characteristic_t *characteristic;
    const char *label;
    float last_notified_value;
    int64_t last_notified_ms;
    float pending_value;
    bool pending;
    float threshold;
    int64_t min_interval_ms;
} hk_notify_state_t;

static const char *NOTIFY_TAG = "HAP_NOTIFY";

static bool notifications_ready = false;
static esp_timer_handle_t notify_timer;

#define NOTIFY_SCHEDULER_INTERVAL_MS 250
#define NOTIFY_INTERVAL_MS_DEFAULT 1000
#define NOTIFY_INTERVAL_MS_ENERGY 30000
#define NOTIFY_THRESHOLD_VOLTAGE 0.5f
#define NOTIFY_THRESHOLD_CURRENT 0.05f
#define NOTIFY_THRESHOLD_POWER 1.0f
#define NOTIFY_THRESHOLD_ENERGY 0.5f

static hk_notify_state_t notify_voltage = {
    .characteristic = &ch_voltage,
    .label = "Voltage",
    .threshold = NOTIFY_THRESHOLD_VOLTAGE,
    .min_interval_ms = NOTIFY_INTERVAL_MS_DEFAULT,
};

static hk_notify_state_t notify_current = {
    .characteristic = &ch_current,
    .label = "Current",
    .threshold = NOTIFY_THRESHOLD_CURRENT,
    .min_interval_ms = NOTIFY_INTERVAL_MS_DEFAULT,
};

static hk_notify_state_t notify_power = {
    .characteristic = &ch_power,
    .label = "Power",
    .threshold = NOTIFY_THRESHOLD_POWER,
    .min_interval_ms = NOTIFY_INTERVAL_MS_DEFAULT,
};

static hk_notify_state_t notify_energy = {
    .characteristic = &ch_energy,
    .label = "Energy",
    .threshold = NOTIFY_THRESHOLD_ENERGY,
    .min_interval_ms = NOTIFY_INTERVAL_MS_ENERGY,
};

static homekit_value_t hk_get_voltage(void) {
    return ch_voltage.value;
}

static homekit_value_t hk_get_current(void) {
    return ch_current.value;
}

static homekit_value_t hk_get_power(void) {
    return ch_power.value;
}

static homekit_value_t hk_get_energy(void) {
    return ch_energy.value;
}

static void notify_mark_pending(hk_notify_state_t *state, float value) {
    const float delta = fabsf(value - state->last_notified_value);
    if (delta >= state->threshold) {
        state->pending_value = value;
        state->pending = true;
    } else if (state->pending) {
        state->pending_value = value;
    }
}

static void notify_try_send(hk_notify_state_t *state, int64_t now_ms) {
    if (!state->pending) {
        return;
    }

    if (now_ms - state->last_notified_ms < state->min_interval_ms) {
        return;
    }

    if (fabsf(state->pending_value - state->last_notified_value) <
        state->threshold) {
        state->pending = false;
        return;
    }

    state->last_notified_value = state->pending_value;
    state->last_notified_ms = now_ms;
    state->pending = false;

    state->characteristic->value = HOMEKIT_FLOAT(state->last_notified_value);
    homekit_characteristic_notify(state->characteristic,
                                  state->characteristic->value);

    uint16_t aid = 0;
    uint16_t iid = state->characteristic->id;
    if (state->characteristic->service &&
        state->characteristic->service->accessory) {
        aid = state->characteristic->service->accessory->id;
    }

    ESP_LOGI(NOTIFY_TAG, "Notified %s=%.3f (aid=%u iid=%u)",
             state->label,
             state->last_notified_value,
             aid,
             iid);
}

static void notify_timer_cb(void *arg) {
    (void)arg;

    if (!notifications_ready) {
        return;
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;

    notify_try_send(&notify_voltage, now_ms);
    notify_try_send(&notify_current, now_ms);
    notify_try_send(&notify_power, now_ms);
    notify_try_send(&notify_energy, now_ms);
}

void custom_characteristics_set_notify_ready(bool ready) {
    if (notifications_ready == ready) {
        return;
    }

    notifications_ready = ready;
    if (ready) {
        const int64_t now_ms = esp_timer_get_time() / 1000;
        notify_voltage.last_notified_ms = now_ms;
        notify_current.last_notified_ms = now_ms;
        notify_power.last_notified_ms = now_ms;
        notify_energy.last_notified_ms = now_ms;
    }
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
void custom_characteristics_init(void) {
    ch_voltage.value = HOMEKIT_FLOAT(0.0f);
    ch_current.value = HOMEKIT_FLOAT(0.0f);
    ch_power.value   = HOMEKIT_FLOAT(0.0f);
    ch_energy.value  = HOMEKIT_FLOAT(0.0f);

    notify_voltage.last_notified_value = 0.0f;
    notify_current.last_notified_value = 0.0f;
    notify_power.last_notified_value = 0.0f;
    notify_energy.last_notified_value = 0.0f;

    notifications_ready = false;

    if (!notify_timer) {
        esp_timer_create_args_t timer_args = {
            .callback = &notify_timer_cb,
            .name = "hk_notify_sched",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &notify_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(
            notify_timer, NOTIFY_SCHEDULER_INTERVAL_MS * 1000));
    }
}

// -----------------------------------------------------------------------------
// Update + notify helpers
// -----------------------------------------------------------------------------
void hk_update_voltage(float v) {
    v = sane_float(v);
    ch_voltage.value = HOMEKIT_FLOAT(v);
    notify_mark_pending(&notify_voltage, v);
}

void hk_update_current(float a) {
    a = sane_float(a);
    ch_current.value = HOMEKIT_FLOAT(a);
    notify_mark_pending(&notify_current, a);
}

void hk_update_power(float w) {
    w = sane_float(w);
    ch_power.value = HOMEKIT_FLOAT(w);
    notify_mark_pending(&notify_power, w);
}

void hk_update_energy(float wh) {
    wh = sane_float(wh);
    if (wh < 0.0f) wh = 0.0f;
    ch_energy.value = HOMEKIT_FLOAT(wh);
    notify_mark_pending(&notify_energy, wh);
}
