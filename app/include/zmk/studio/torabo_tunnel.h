/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/sys/iterable_sections.h>
#include <zephyr/types.h>

/**
 * @file
 * @brief Registration API for the torabo settings tunnel.
 *
 * The tunnel carries opaque wire blobs for out-of-tree features over the Studio
 * RPC transport, so a feature never has to touch protobuf/nanopb (whose
 * generated headers are private to the `app` target). Implementations live in
 * out-of-tree modules and only include this header.
 */

/**
 * @brief A single tunnel-addressable feature.
 *
 * @param feature_id Stable identifier, shared with the equivalent BLE GATT
 *                   characteristic so both transports address the same feature.
 * @param read       Encode the current settings into @p buf (at most @p cap
 *                   bytes) and set @p out_len. Returns 0 or a negative errno.
 *                   May be NULL if the feature is write-only.
 * @param write      Validate and apply a complete wire blob. Returns 0 or a
 *                   negative errno. May be NULL if the feature is read-only.
 */
struct torabo_tunnel_feature {
    uint16_t feature_id;
    int (*read)(uint8_t *buf, uint16_t cap, uint16_t *out_len);
    int (*write)(const uint8_t *buf, uint16_t len);
};

/**
 * @brief Register a feature with the tunnel subsystem.
 * @param name A unique identifier for the registration, e.g. `trackball`.
 * @param id The feature identifier, @see struct torabo_tunnel_feature.
 * @param read_cb The read callback, or NULL.
 * @param write_cb The write callback, or NULL.
 */
#define TORABO_TUNNEL_FEATURE(name, id, read_cb, write_cb)                                         \
    STRUCT_SECTION_ITERABLE(torabo_tunnel_feature, torabo_tunnel_feature_##name) = {               \
        .feature_id = (id),                                                                        \
        .read = (read_cb),                                                                         \
        .write = (write_cb),                                                                       \
    };

/**
 * @brief Push an unsolicited blob for the given feature to the connected client.
 * @retval -ENOTSUP if no client has subscribed to @p feature_id.
 */
int torabo_tunnel_notify(uint16_t feature_id, const uint8_t *buf, uint16_t len);
