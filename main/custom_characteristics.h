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

#pragma once

#include <stddef.h>
#include <stdbool.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#ifndef HOMEKIT_CUSTOM_UUID
#define HOMEKIT_CUSTOM_UUID(value) (value "-0e36-4a42-ad11-745a73b84f2b")
#endif

#define HOMEKIT_SERVICE_CUSTOM_POWER_METER HOMEKIT_CUSTOM_UUID("000000A1")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE HOMEKIT_CUSTOM_UUID("F0000101")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTAGE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTAGE, \
    .description = "Voltage (V)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){260}, \
    .min_step = (float[]){0.1f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT HOMEKIT_CUSTOM_UUID("F0000102")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CURRENT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CURRENT, \
    .description = "Current (A)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){32}, \
    .min_step = (float[]){0.01f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWER HOMEKIT_CUSTOM_UUID("F0000103")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWER, \
    .description = "Power (W)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){4000}, \
    .min_step = (float[]){0.1f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY HOMEKIT_CUSTOM_UUID("F0000104")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_ENERGY(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_ENERGY, \
    .description = "Energy (kWh)", \
    .format = homekit_format_float, \
    .unit = homekit_unit_none, \
    .permissions = homekit_permissions_paired_read \
        | homekit_permissions_notify, \
    .min_value = (float[]){0}, \
    .max_value = (float[]){99999}, \
    .min_step = (float[]){0.01f}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#ifdef __cplusplus
extern "C" {
#endif

void custom_characteristic_update_float(homekit_characteristic_t *characteristic,
                                        float value,
                                        bool notify);

#ifdef __cplusplus
}
#endif
