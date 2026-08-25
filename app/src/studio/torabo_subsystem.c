/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <errno.h>
#include <string.h>

#include <zephyr/sys/atomic.h>

#include <pb_encode.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/studio/rpc.h>
#include <zmk/studio/torabo_tunnel.h>

#if IS_ENABLED(CONFIG_ZMK_STUDIO_TRANSPORT_BLE)
#include <zephyr/bluetooth/conn.h>
#endif

ZMK_RPC_SUBSYSTEM(torabo)

#define TORABO_RESPONSE(type, ...) ZMK_RPC_RESPONSE(torabo, type, __VA_ARGS__)

#define TUNNEL_STATUS(name) zmk_torabo_TunnelStatus_ZMK_TORABO_TUNNEL_STATUS_##name
#define TUNNEL_OP(name) zmk_torabo_TunnelOp_ZMK_TORABO_TUNNEL_OP_##name

/* Feature ids index a bitmask, so they have to stay inside one atomic word. */
#define TUNNEL_MAX_FEATURE_ID 31

/* Staging area for a read response. Only ever touched from the single RPC
 * thread, between building the response and encoding it in send_response(). */
static uint8_t tunnel_blob[CONFIG_ZMK_STUDIO_TORABO_TUNNEL_BLOB_MAX_SIZE];
static uint16_t tunnel_blob_len;

/* Bit N set = a client asked feature N to push. Written from the RPC thread and
 * from the disconnect paths below, read from whichever thread produces a record. */
static atomic_t subscriptions;

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

/* Tell every subscribed feature its client is gone, and forget the whole set.
 * Called when the client can no longer be reached, so a stale subscription can
 * never keep a producer pushing into a transport nobody is draining. */
static void drop_all_subscriptions(void) {
    atomic_val_t was = atomic_clear(&subscriptions);
    if (was == 0) {
        return;
    }

    LOG_DBG("Dropping tunnel subscriptions 0x%08x", (uint32_t)was);

    STRUCT_SECTION_FOREACH(torabo_tunnel_feature, feature) {
        if ((was & BIT(feature->feature_id)) && feature->subscribe) {
            feature->subscribe(false);
        }
    }
}

bool torabo_tunnel_subscribed(uint16_t feature_id) {
    if (feature_id > TUNNEL_MAX_FEATURE_ID) {
        return false;
    }

    return (atomic_get(&subscriptions) & BIT(feature_id)) != 0;
}

static zmk_studio_Response set_subscribed(const struct torabo_tunnel_feature *feature,
                                          bool enabled) {
    if (!feature->subscribe || feature->feature_id > TUNNEL_MAX_FEATURE_ID) {
        return tunnel_status(TUNNEL_STATUS(UNSUPPORTED_FEATURE));
    }

    if (enabled) {
        atomic_or(&subscriptions, BIT(feature->feature_id));
    } else {
        atomic_and(&subscriptions, ~BIT(feature->feature_id));
    }

    int ret = feature->subscribe(enabled);
    if (ret < 0) {
        atomic_and(&subscriptions, ~BIT(feature->feature_id));
        LOG_ERR("Feature 0x%02x refused to subscribe: %d", feature->feature_id, ret);
        return tunnel_status(TUNNEL_STATUS(ERROR));
    }

    return tunnel_status(TUNNEL_STATUS(OK));
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
    case TUNNEL_OP(SUBSCRIBE):
        return set_subscribed(feature, true);
    case TUNNEL_OP(UNSUBSCRIBE):
        return set_subscribed(feature, false);
    default:
        return tunnel_status(TUNNEL_STATUS(INVALID));
    }

    zmk_torabo_TunnelResponse resp = zmk_torabo_TunnelResponse_init_zero;
    resp.status = TUNNEL_STATUS(OK);
    resp.blob.funcs.encode = encode_tunnel_blob;

    return TORABO_RESPONSE(tunnel, resp);
}

ZMK_RPC_SUBSYSTEM_HANDLER(torabo, tunnel, ZMK_STUDIO_RPC_HANDLER_UNSECURED);

int torabo_tunnel_notify(uint16_t feature_id, const uint8_t *buf, uint16_t len) {
    if (!torabo_tunnel_subscribed(feature_id)) {
        return -ENOTSUP;
    }

    zmk_torabo_TunnelNotification notif = zmk_torabo_TunnelNotification_init_zero;

    if (len > sizeof(notif.blob.bytes)) {
        LOG_ERR("Feature 0x%02x pushed %u bytes, max is %u", feature_id, len,
                (unsigned)sizeof(notif.blob.bytes));
        return -EMSGSIZE;
    }

    notif.feature_id = feature_id;
    notif.blob.size = len;
    memcpy(notif.blob.bytes, buf, len);

    zmk_studio_Notification n = ZMK_RPC_NOTIFICATION(torabo, tunnel, notif);

    return zmk_rpc_send_notification(&n);
}

/* ---- reachability --------------------------------------------------------
 * A subscription only makes sense while the client that asked for it can still
 * be reached. Studio RPC follows the selected endpoint, so a transport switch
 * (unplugging USB, say) already means the subscriber is gone; and a host BLE
 * link dropping means the same for the GATT transport. The split link to the
 * other half is a connection we opened as central, so it is left alone.
 */

static int torabo_tunnel_listener_cb(const zmk_event_t *eh) {
    if (as_zmk_endpoint_changed(eh)) {
        drop_all_subscriptions();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(torabo_tunnel, torabo_tunnel_listener_cb);
ZMK_SUBSCRIPTION(torabo_tunnel, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_ZMK_STUDIO_TRANSPORT_BLE)

static void torabo_tunnel_disconnected(struct bt_conn *conn, uint8_t reason) {
    struct bt_conn_info info;

    /* Only a link where we are the peripheral carries a host: the connections we
     * opened as central are the split halves. */
    if (bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_PERIPHERAL) {
        drop_all_subscriptions();
    }
}

BT_CONN_CB_DEFINE(torabo_tunnel_conn_cb) = {
    .disconnected = torabo_tunnel_disconnected,
};

#endif // IS_ENABLED(CONFIG_ZMK_STUDIO_TRANSPORT_BLE)
