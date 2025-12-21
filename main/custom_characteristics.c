#include "custom_characteristics.h"

#include <homekit/homekit.h>

static void update_characteristic(homekit_characteristic_t *characteristic,
                                  homekit_value_t value,
                                  bool notify) {
    if (characteristic == NULL) {
        return;
    }

    characteristic->value = value;
    if (notify) {
        homekit_characteristic_notify(characteristic, characteristic->value);
    }
}

void bl0937_characteristics_update(const bl0937_characteristics_t *characteristics,
                                   float voltage,
                                   float current,
                                   float power,
                                   float energy_wh,
                                   float power_factor,
                                   bool notify) {
    if (characteristics == NULL) {
        return;
    }

    update_characteristic(characteristics->voltage, HOMEKIT_FLOAT(voltage), notify);
    update_characteristic(characteristics->current, HOMEKIT_FLOAT(current), notify);
    update_characteristic(characteristics->power, HOMEKIT_FLOAT(power), notify);
    update_characteristic(characteristics->energy, HOMEKIT_FLOAT(energy_wh), notify);
    update_characteristic(characteristics->power_factor, HOMEKIT_FLOAT(power_factor), notify);
}
