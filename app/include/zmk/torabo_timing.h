/*
 * Copyright (c) 2026 tak-2025
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * @file
 * @brief Runtime override points for hold-tap and kscan debounce timing.
 *
 * WHY THIS EXISTS
 * Both sets of numbers are baked into `static const` devicetree structs, so the
 * only way to change them is a reflash. The torabo firmware wants them editable
 * live from the app (docs/DESIGN-timing.md in torabo-tsuki_ext_FW), which needs
 * a handful of seams in this tree — and nothing else.
 *
 * Every one of them is __weak here and defaults to STOCK BEHAVIOR: no override,
 * the devicetree debounce config passed straight back, and nothing sent across
 * the split link. A build without the torabo timing module therefore behaves
 * bit-for-bit as it did before, and this fork still builds and runs on its own.
 *
 * The strong definitions live in the out-of-tree module
 * (torabo-tsuki_ext_FW/timing/src/config_state.c on the central,
 * src/split_peripheral.c on the peripheral) — except
 * zmk_torabo_debounce_split_push, which the BLE split central defines here.
 * Zephyr links its libraries
 * with --whole-archive, so the strong definition wins whenever that module is
 * compiled in.
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Positional-hold slots carried by the wire (DESIGN-timing.md §"Wire v1"). */
#define ZMK_TORABO_HT_MAX_POSITIONS 32

/** `flags` bits, mirroring the wire's ht-block byte +7. */
#define ZMK_TORABO_HT_FLAG_RETRO_TAP 0x01
#define ZMK_TORABO_HT_FLAG_HOLD_TRIGGER_ON_RELEASE 0x02
#define ZMK_TORABO_HT_FLAG_HOLD_WHILE_UNDECIDED 0x04

/**
 * @brief One hold-tap node's tunable parameters.
 *
 * Same content as a wire ht block, in native form: the wire's 0xFFFF "disabled"
 * sentinel for the two quick-tap style timeouts is already decoded to -1 here,
 * which is what `struct behavior_hold_tap_config` uses.
 *
 * `hold-while-undecided-linger` is deliberately absent: it is out of scope for
 * wire v1 and always keeps its devicetree value.
 */
struct zmk_torabo_ht_params {
    /** `tapping-term-ms`. */
    uint16_t tapping_term_ms;
    /** `quick-tap-ms`; negative = disabled. */
    int32_t quick_tap_ms;
    /** `require-prior-idle-ms`; negative = disabled. */
    int32_t require_prior_idle_ms;
    /** 0 hold-preferred, 1 balanced, 2 tap-preferred, 3 tap-unless-interrupted. */
    uint8_t flavor;
    /** ZMK_TORABO_HT_FLAG_* */
    uint8_t flags;
    /** Entries used in @ref positions; 0 disables positional hold. */
    uint8_t pos_count;
    /** `hold-trigger-key-positions`; only the first @ref pos_count are valid. */
    uint8_t positions[ZMK_TORABO_HT_MAX_POSITIONS];
};

/**
 * @brief Ask for the live parameters of one hold-tap node.
 *
 * Called by behavior_hold_tap.c ONCE per key press, at the moment the hold-tap
 * is captured, and the result is latched for the whole undecided window — a
 * setting written mid-decision can never change the rules half way through.
 *
 * @param dev The hold-tap behavior instance (`dev->name` is the devicetree node
 *            name, e.g. "mod_tap" / "layer_tap").
 * @param out Filled with the live parameters when true is returned.
 *
 * @retval true  @p out is valid and should replace the devicetree config.
 * @retval false no override for this node — use the devicetree config as-is.
 *               This is the __weak default.
 */
bool zmk_torabo_ht_override(const struct device *dev, struct zmk_torabo_ht_params *out);

/**
 * @brief Publish a hold-tap node's devicetree defaults.
 *
 * Called once per instance from behavior_hold_tap_init(). It is how the timing
 * module learns the built-in values so a READ can answer with them before the
 * user has ever written a config — without `struct behavior_hold_tap_config`
 * having to become public.
 *
 * @param dev_name Devicetree node name of the instance.
 * @param dt       Its devicetree values. Borrowed; copy what you keep.
 */
void zmk_torabo_ht_report_dt(const char *dev_name, const struct zmk_torabo_ht_params *dt);

/* Forward declaration only: <zmk/debounce.h> lives in the kscan driver module,
 * and nothing here needs the layout. Includers that dereference the pointer
 * include that header themselves. */
struct zmk_debounce_config;

/**
 * @brief Pick the debounce config a kscan scan should use.
 *
 * Called on every matrix scan, so a write takes effect on the very next scan
 * with no re-init. The returned pointer must stay valid until at least the next
 * call.
 *
 * @param dt The driver's devicetree config.
 * @return @p dt itself (the __weak default), or a live replacement.
 */
const struct zmk_debounce_config *
zmk_torabo_debounce_effective(const struct zmk_debounce_config *dt);

/* ---- split propagation of the debounce windows ---------------------------
 *
 * Each half scans its own matrix, so the numbers the app writes to the central
 * would otherwise stop there. These three seams carry them across the split
 * link, gated by CONFIG_ZMK_SPLIT_BLE_DEBOUNCE_SYNC.
 *
 * The peripheral stores nothing: the central re-sends on every connect, so the
 * only window where the two halves disagree is the second or two before the
 * link is up, where the peripheral runs on its devicetree values.
 */

/**
 * @brief Read the debounce windows the central should hand its peripherals.
 *
 * Implemented by the torabo timing module (central side); the __weak default
 * returns false.
 *
 * @retval true  something has been written; @p press_ms / @p release_ms are set.
 * @retval false nothing written yet — the peripherals keep their devicetree
 *               values and the central must not push anything.
 */
bool zmk_torabo_debounce_split_values(uint8_t *press_ms, uint8_t *release_ms);

/**
 * @brief Ask the split central to re-send the current debounce windows.
 *
 * Called by the timing module whenever a write changes them. Implemented by the
 * BLE split central (which also pushes on its own when a peripheral's
 * characteristic turns up during discovery); the __weak default is a no-op, so
 * a non-split or wired-only build simply does nothing.
 */
void zmk_torabo_debounce_split_push(void);

/**
 * @brief Apply debounce windows pushed down the split link.
 *
 * Called on the peripheral from the split GATT write. Implemented by the torabo
 * timing module's peripheral half (which is also what supplies
 * zmk_torabo_debounce_effective there); the __weak default is a no-op.
 */
void zmk_torabo_debounce_split_apply(uint8_t press_ms, uint8_t release_ms);

#ifdef __cplusplus
}
#endif
