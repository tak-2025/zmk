/*
 * Copyright (c) 2026 tak-2025
 *
 * SPDX-License-Identifier: MIT
 *
 * Stock-behavior defaults for the torabo timing override points.
 *
 * Keeping them here (rather than inline in the header) means every translation
 * unit sees one declaration and the linker resolves to whichever definition is
 * present: these when the torabo timing module is not in the build, the
 * module's strong ones when it is.
 */

#include <zmk/torabo_timing.h>

__weak bool zmk_torabo_ht_override(const struct device *dev, struct zmk_torabo_ht_params *out) {
    ARG_UNUSED(dev);
    ARG_UNUSED(out);
    return false;
}

__weak void zmk_torabo_ht_report_dt(const char *dev_name, const struct zmk_torabo_ht_params *dt) {
    ARG_UNUSED(dev_name);
    ARG_UNUSED(dt);
}

__weak const struct zmk_debounce_config *
zmk_torabo_debounce_effective(const struct zmk_debounce_config *dt) {
    return dt;
}

__weak bool zmk_torabo_debounce_split_values(uint8_t *press_ms, uint8_t *release_ms) {
    ARG_UNUSED(press_ms);
    ARG_UNUSED(release_ms);
    return false;
}

/* Strong on a BLE split central built with CONFIG_ZMK_SPLIT_BLE_DEBOUNCE_SYNC
 * (src/split/bluetooth/central.c); nothing to do anywhere else. */
__weak void zmk_torabo_debounce_split_push(void) {}

__weak void zmk_torabo_debounce_split_apply(uint8_t press_ms, uint8_t release_ms) {
    ARG_UNUSED(press_ms);
    ARG_UNUSED(release_ms);
}
