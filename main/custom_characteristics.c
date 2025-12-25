#include "custom_characteristics.h"

homekit_characteristic_t voltage_characteristic =
    HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE_(0.0f);

homekit_characteristic_t current_characteristic =
    HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT_(0.0f);

homekit_characteristic_t power_characteristic =
    HOMEKIT_CHARACTERISTIC_CUSTOM_POWER_(0.0f);

homekit_characteristic_t total_consumption_characteristic =
    HOMEKIT_CHARACTERISTIC_CUSTOM_TOTAL_CONSUMPTION_(0.0f);
