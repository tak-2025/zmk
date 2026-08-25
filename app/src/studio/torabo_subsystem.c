/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <pb_encode.h>
#include <zmk/studio/rpc.h>
#include <zmk/studio/torabo_tunnel.h>

ZMK_RPC_SUBSYSTEM(torabo)

#define TORABO_RESPONSE(type, ...) ZMK_RPC_RESPONSE(torabo, type, __VA_ARGS__)

#define TUNNEL_STATUS(name) zmk_torabo_TunnelStatus_ZMK_TORABO_TUNNEL_STATUS_##name
#define TUNNEL_OP(name) zmk_torabo_TunnelOp_ZMK_TORABO_TUNNEL_OP_##name

/* Staging area for a read response. Only ever touched from the single RPC
 * thread, between building the response and encoding it in send_response(). */
static uint8_t tunnel_blob[CONFIG_ZMK_STUDIO_TORABO_TUNNEL_BLOB_MAX_SIZE];
static uint16_t tunnel_blob_len;

static bool encode_tunnel_blob(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
    if (tunnel_blob_len == 0) {
        return true;
    }

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    return pb_encode_string(stream, tunnel_blob, tunnel_blob_len);
}

static const struct torabo_tunnel_feature *find_feature(uint16_t feature_id) {
    STRUCT_SECTION_FOREACH(torabo_tunnel_feature, feature) {
        if (feature->feature_id == feature_id) {
            return feature;
        }
    }

    return NULL;
}

static zmk_studio_Response tunnel_status(zmk_torabo_TunnelStatus status) {
    zmk_torabo_TunnelResponse resp = zmk_torabo_TunnelResponse_init_zero;
    resp.status = status;

    return TORABO_RESPONSE(tunnel, resp);
}

zmk_studio_Response tunnel(const zmk_studio_Request *req) {
    const zmk_torabo_TunnelRequest *tunnel_req = &req->subsystem.torabo.request_type.tunnel;
    const uint16_t feature_id = (uint16_t)tunnel_req->feature_id;

    LOG_DBG("Tunnel op %d for feature 0x%02x", tunnel_req->op, feature_id);

    const struct torabo_tunnel_feature *feature = find_feature(feature_id);
    if (!feature) {
        LOG_WRN("No tunnel feature registered for 0x%02x", feature_id);
        return tunnel_status(TUNNEL_STATUS(UNSUPPORTED_FEATURE));
    }

    tunnel_blob_len = 0;

    switch (tunnel_req->op) {
    case TUNNEL_OP(READ): {
        if (!feature->read) {
            return tunnel_status(TUNNEL_STATUS(UNSUPPORTED_FEATURE));
        }

        int ret = feature->read(tunnel_blob, sizeof(tunnel_blob), &tunnel_blob_len);
        if (ret < 0) {
            LOG_ERR("Failed to read feature 0x%02x: %d", feature_id, ret);
            tunnel_blob_len = 0;
            return tunnel_status(TUNNEL_STATUS(ERROR));
        }
        break;
    }
    case TUNNEL_OP(WRITE): {
        if (!feature->write) {
            return tunnel_status(TUNNEL_STATUS(UNSUPPORTED_FEATURE));
        }

        int ret = feature->write(tunnel_req->blob.bytes, tunnel_req->blob.size);
        if (ret < 0) {
            LOG_ERR("Failed to write feature 0x%02x: %d", feature_id, ret);
            return tunnel_status(TUNNEL_STATUS(INVALID));
        }
        break;
    }
    default:
        /* SUBSCRIBE/UNSUBSCRIBE are added along with torabo_tunnel_notify(). */
        return tunnel_status(TUNNEL_STATUS(UNSUPPORTED_FEATURE));
    }

    zmk_torabo_TunnelResponse resp = zmk_torabo_TunnelResponse_init_zero;
    resp.status = TUNNEL_STATUS(OK);
    resp.blob.funcs.encode = encode_tunnel_blob;

    return TORABO_RESPONSE(tunnel, resp);
}

ZMK_RPC_SUBSYSTEM_HANDLER(torabo, tunnel, ZMK_STUDIO_RPC_HANDLER_UNSECURED);

int torabo_tunnel_notify(uint16_t feature_id, const uint8_t *buf, uint16_t len) {
    ARG_UNUSED(feature_id);
    ARG_UNUSED(buf);
    ARG_UNUSED(len);

    /* Filled in together with the SUBSCRIBE/UNSUBSCRIBE ops. */
    return -ENOTSUP;
}
