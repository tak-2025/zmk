/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

#ifndef BT_UUID_NUM_OF_DIGITALS
#define BT_UUID_NUM_OF_DIGITALS BT_UUID_DECLARE_16(0x2909)
#endif

#define ZMK_BT_SPLIT_UUID(num) BT_UUID_128_ENCODE(num, 0x0096, 0x7107, 0xc967, 0xc5cfb1c2482a)
#define ZMK_SPLIT_BT_SERVICE_UUID ZMK_BT_SPLIT_UUID(0x00000000)
#define ZMK_SPLIT_BT_CHAR_POSITION_STATE_UUID ZMK_BT_SPLIT_UUID(0x00000001)
#define ZMK_SPLIT_BT_CHAR_RUN_BEHAVIOR_UUID ZMK_BT_SPLIT_UUID(0x00000002)
#define ZMK_SPLIT_BT_CHAR_SENSOR_STATE_UUID ZMK_BT_SPLIT_UUID(0x00000003)
#define ZMK_SPLIT_BT_UPDATE_HID_INDICATORS_UUID ZMK_BT_SPLIT_UUID(0x00000004)
#define ZMK_SPLIT_BT_SELECT_PHYS_LAYOUT_UUID ZMK_BT_SPLIT_UUID(0x00000005)
#define ZMK_SPLIT_BT_INPUT_EVENT_UUID ZMK_BT_SPLIT_UUID(0x00000006)

/* Torabo additions start at 0xa0 so upstream can keep extending its own run of
 * numbers without ever colliding with this fork.
 * 0xa0: the central pushes the two kscan debounce windows it was given over
 * BLE, so the peripheral scans with the same numbers (DESIGN-timing.md). */
#define ZMK_SPLIT_BT_UPDATE_DEBOUNCE_UUID ZMK_BT_SPLIT_UUID(0x000000a0)
