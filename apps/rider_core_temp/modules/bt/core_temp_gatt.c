#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"
#include "core_temp_profile.h"
#include "gatt_common/le_gatt_common.h"
#include "le_common.h"
#include "asm/adc_api.h"

#define LOG_TAG_CONST       GATT_SERVER
#define LOG_TAG             "[RIDER_GATT]"
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

#define RIDER_ADV_INTERVAL              (160 * 5)
#define RIDER_ADV_PACKET_MAX            31
#define RIDER_ATT_MTU_SIZE              247
#define RIDER_ATT_CBUFFER_SIZE          512
#define RIDER_STANDARD_PERIOD_SECONDS   10
#define RIDER_EXTERNAL_HR_TIMEOUT       15
#define RIDER_CORE_FRAME_MAX            5
#define RIDER_CORE_COMPAT_ADV_NAME      "CORE"

static u8 rider_adv_data[RIDER_ADV_PACKET_MAX];
static u8 rider_scan_rsp_data[RIDER_ADV_PACKET_MAX];
static adv_cfg_t rider_adv_config;
static u16 rider_connection_handle;
static u16 rider_tick_count;
static u8 rider_external_hr_age;
static u8 rider_gatt_ready;
static u8 rider_last_adv_valid;
static int16_t rider_last_adv_core;
static u8 rider_pending_cp_response[3];
static u8 rider_pending_cp_response_valid;
static u8 rider_cp_indication_in_flight;
static u8 rider_last_battery_level;
static u8 rider_battery_level_valid;

static uint16_t rider_att_read_callback(hci_con_handle_t connection_handle,
                                        uint16_t att_handle,
                                        uint16_t offset,
                                        uint8_t *buffer,
                                        uint16_t buffer_size);
static int rider_att_write_callback(hci_con_handle_t connection_handle,
                                    uint16_t att_handle,
                                    uint16_t transaction_mode,
                                    uint16_t offset,
                                    uint8_t *buffer,
                                    uint16_t buffer_size);
static int rider_event_packet_handler(int event, u8 *packet, u16 size, u8 *ext_param);

#if RIDER_BATTERY_FULL_MV <= RIDER_BATTERY_EMPTY_MV
#error "Rider battery full voltage must be above empty voltage"
#endif

static gatt_server_cfg_t rider_server_config = {
    .att_read_cb = rider_att_read_callback,
    .att_write_cb = rider_att_write_callback,
    .event_packet_handler = rider_event_packet_handler,
};

#if CONFIG_BT_SM_SUPPORT_ENABLE
/*
 * Dura may request an encrypted/bonded link after the LE connection.  Rider
 * has no display or input path, so Just Works is the only unattended method;
 * it provides encryption/bonding but cannot provide MITM authentication.
 */
static sm_cfg_t rider_sm_config = {
    /* DURA/CORE uses application-level accessory pairing.  Only respond if
     * the collector explicitly requests link security. */
    .slave_security_auto_req = 0,
    .slave_set_wait_security = 0,
    .io_capabilities = IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
    .authentication_req_flags = SM_AUTHREQ_BONDING,
    .min_key_size = 7,
    .max_key_size = 16,
    .sm_cb_packet_handler = NULL,
};
#endif

static gatt_ctrl_t rider_gatt_control_block = {
    .mtu_size = RIDER_ATT_MTU_SIZE,
    .cbuffer_size = RIDER_ATT_CBUFFER_SIZE,
    .multi_dev_flag = 0,
    .server_config = &rider_server_config,
    .client_config = NULL,
#if CONFIG_BT_SM_SUPPORT_ENABLE
    .sm_config = &rider_sm_config,
#else
    .sm_config = NULL,
#endif
    .hci_cb_packet_handler = NULL,
};

/** Copy an ATT value while preserving the callback's long-read contract. */
static u16 rider_copy_value(const u8 *value, u16 value_len, u16 offset,
                            u8 *buffer, u16 buffer_size)
{
    u16 copy_len;

    if (offset > value_len) {
        return 0;
    }
    if (!buffer) {
        return value_len;
    }

    copy_len = value_len - offset;
    if (copy_len > buffer_size) {
        copy_len = buffer_size;
    }
    if (copy_len) {
        memcpy(buffer, value + offset, copy_len);
    }
    return copy_len;
}

/** Read a little-endian CCCD value through the common GATT state store. */
static u16 rider_copy_ccc(u16 connection_handle, u16 ccc_handle, u16 offset,
                          u8 *buffer, u16 buffer_size)
{
    u16 ccc = ble_gatt_server_characteristic_ccc_get(connection_handle, ccc_handle);
    u8 value[2] = {(u8)ccc, (u8)(ccc >> 8)};

    return rider_copy_value(value, sizeof(value), offset, buffer, buffer_size);
}

/** Send only after the matching CCCD mode and ATT queue are available. */
static int rider_send_subscribed_value(u16 connection_handle, u16 value_handle,
                                       u16 ccc_handle, u16 required_ccc,
                                       u8 *value, u16 value_len)
{
    u16 ccc;

    if (!connection_handle || !value || !value_len) {
        return GATT_CMD_PARAM_ERROR;
    }
    ccc = ble_gatt_server_characteristic_ccc_get(connection_handle, ccc_handle);
    if (!(ccc & required_ccc) || !ble_comm_att_check_send(connection_handle, value_len)) {
        return GATT_CMD_USE_CCC_FAIL;
    }
    return ble_comm_att_send_data(connection_handle, value_handle, value, value_len,
                                  ATT_OP_AUTO_READ_CCC);
}

/** Convert the AC632N VBAT monitor into the protocol's 0..100 percentage. */
static u8 rider_get_battery_level(void)
{
    u32 battery_mv = adc_get_voltage(AD_CH_VBAT) * 4;
    u32 span_mv = RIDER_BATTERY_FULL_MV - RIDER_BATTERY_EMPTY_MV;

    if (battery_mv <= RIDER_BATTERY_EMPTY_MV) {
        return 0;
    }
    if (battery_mv >= RIDER_BATTERY_FULL_MV) {
        return 100;
    }
    return (u8)(((battery_mv - RIDER_BATTERY_EMPTY_MV) * 100 + span_mv / 2) /
                span_mv);
}

/** Notify a subscribed client only after a measured percentage changes. */
static void rider_refresh_battery(void)
{
    u8 battery_level = rider_get_battery_level();
    int result;

    if (rider_battery_level_valid && battery_level == rider_last_battery_level) {
        return;
    }
    if (!rider_connection_handle) {
        rider_last_battery_level = battery_level;
        rider_battery_level_valid = 1;
        return;
    }

    result = rider_send_subscribed_value(rider_connection_handle,
                                         RIDER_ATT_BATTERY_VALUE_HANDLE,
                                         RIDER_ATT_BATTERY_CCC_HANDLE,
                                         GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION,
                                         &battery_level, sizeof(battery_level));
    if (result == GATT_OP_RET_SUCESS) {
        rider_last_battery_level = battery_level;
        rider_battery_level_valid = 1;
    }
}

/** Encode the variable-length CORE temperature notification frame. */
static u16 rider_make_core_frame(u8 *frame, u16 frame_size,
                                 const rider_temperature_snapshot_t *snapshot)
{
    int16_t core = 0x7fff;
    u8 flags = 0x04; /* Quality & State is present; core temperature is mandatory. */
    u8 quality_state = 0x17; /* quality N/A + HR supported, no signal */
    u16 length = 0;

    if (!frame || frame_size < 4) {
        return 0;
    }
    if (snapshot && snapshot->valid) {
        core = snapshot->core_temperature_centi;
    }
    if (snapshot && snapshot->heart_rate_valid) {
        flags |= 0x10;
        quality_state = 0x27; /* quality N/A + HR supported and receiving */
    }

    frame[length++] = flags;
    frame[length++] = (u8)core;
    frame[length++] = (u8)((u16)core >> 8);
    frame[length++] = quality_state;
    if (flags & 0x10) {
        if (frame_size < length + 1) {
            return 0;
        }
        frame[length++] = snapshot->heart_rate;
    }
    return length;
}

/** Encode the SIG Health Thermometer IEEE-11073 FLOAT measurement. */
static u16 rider_make_standard_temperature(u8 *value, u16 value_size,
                                            const rider_temperature_snapshot_t *snapshot)
{
    int32_t mantissa;

    if (!value || value_size < 6) {
        return 0;
    }
    value[0] = 0x04; /* Celsius and Temperature Type present. */
    if (!snapshot || !snapshot->valid) {
        mantissa = 0x007fffff; /* IEEE-11073 FLOAT NaN mantissa. */
        value[1] = (u8)mantissa;
        value[2] = (u8)(mantissa >> 8);
        value[3] = (u8)(mantissa >> 16);
        value[4] = 0;
    } else {
        mantissa = snapshot->core_temperature_centi;
        value[1] = (u8)mantissa;
        value[2] = (u8)(mantissa >> 8);
        value[3] = (u8)(mantissa >> 16);
        value[4] = 0xfe; /* 10^-2 Celsius. */
    }
    value[5] = 0x02; /* General temperature type. */
    return 6;
}

/** Rebuild advertising and scan-response payloads from the current snapshot. */
static void rider_make_advertisement(const rider_temperature_snapshot_t *snapshot)
{
    u16 offset = 0;

    memset(rider_adv_data, 0, sizeof(rider_adv_data));
    memset(rider_scan_rsp_data, 0, sizeof(rider_scan_rsp_data));

    /* General discoverable, BR/EDR not supported. */
    rider_adv_data[offset++] = 2;
    rider_adv_data[offset++] = 0x01;
    rider_adv_data[offset++] = 0x06;

    /* Match the official CORE advertisement: HTS UUID is in the primary
     * packet, while the custom CTS UUID is returned in the scan response. */
    rider_adv_data[offset++] = 3;
    rider_adv_data[offset++] = 0x03;
    rider_adv_data[offset++] = 0x09;
    rider_adv_data[offset++] = 0x18;

    /* CORE-compatible collectors may use the advertised local name as a
     * secondary filter after checking the service UUID. */
    rider_adv_data[offset++] = sizeof(RIDER_CORE_COMPAT_ADV_NAME);
    rider_adv_data[offset++] = 0x09;
    memcpy(&rider_adv_data[offset], RIDER_CORE_COMPAT_ADV_NAME,
           sizeof(RIDER_CORE_COMPAT_ADV_NAME) - 1);
    offset += sizeof(RIDER_CORE_COMPAT_ADV_NAME) - 1;

    /* The documented beacon has no unavailable sentinel; omit it until valid. */
    if (snapshot && snapshot->valid) {
        int32_t milli = (int32_t)snapshot->core_temperature_centi * 10;
        if (milli >= 0 && milli <= 0xffff) {
            u16 temperature_milli = (u16)milli;
            rider_adv_data[offset++] = 7;
            rider_adv_data[offset++] = 0xff;
            rider_adv_data[offset++] = 0x0b; /* Manufacturer ID 0xF60B LE. */
            rider_adv_data[offset++] = 0xf6;
            rider_adv_data[offset++] = 0x00; /* Beacon data version. */
            rider_adv_data[offset++] = 0x04; /* Normal measurement state. */
            rider_adv_data[offset++] = (u8)temperature_milli;
            rider_adv_data[offset++] = (u8)(temperature_milli >> 8);
        }
    }

    /* Active scanners can discover the complementary services and CTS. */
    rider_scan_rsp_data[0] = 5;
    rider_scan_rsp_data[1] = 0x03;
    rider_scan_rsp_data[2] = 0x0a;
    rider_scan_rsp_data[3] = 0x18;
    rider_scan_rsp_data[4] = 0x0f;
    rider_scan_rsp_data[5] = 0x18;
    rider_scan_rsp_data[6] = 17;
    rider_scan_rsp_data[7] = 0x07;
    memcpy(&rider_scan_rsp_data[8], rider_core_service_uuid128,
           sizeof(rider_core_service_uuid128));

    rider_adv_config.adv_data = rider_adv_data;
    rider_adv_config.adv_data_len = offset;
    rider_adv_config.rsp_data = rider_scan_rsp_data;
    rider_adv_config.rsp_data_len = 8 + sizeof(rider_core_service_uuid128);
    rider_adv_config.adv_interval = RIDER_ADV_INTERVAL;
    rider_adv_config.adv_auto_do = 1;
    rider_adv_config.adv_type = ADV_IND;
    rider_adv_config.adv_channel = ADV_CHANNEL_ALL;
    memset(rider_adv_config.direct_address_info, 0,
           sizeof(rider_adv_config.direct_address_info));
    rider_adv_config.set_local_addr_tag = 0;
    memset(rider_adv_config.local_address_info, 0,
           sizeof(rider_adv_config.local_address_info));
}

/** Refresh the beacon only while disconnected, when its value actually changed. */
static void rider_refresh_advertisement(const rider_temperature_snapshot_t *snapshot)
{
    u8 valid = (snapshot && snapshot->valid) ? 1 : 0;
    int16_t core = valid ? snapshot->core_temperature_centi : 0;
    u8 changed = (valid != rider_last_adv_valid) ||
                 (valid && core != rider_last_adv_core);

    rider_make_advertisement(snapshot);
    if (changed && rider_gatt_ready && !rider_connection_handle) {
        if (ble_gatt_server_adv_enable(0) == GATT_OP_RET_SUCESS) {
            ble_gatt_server_adv_enable(1);
        }
    }
    rider_last_adv_valid = valid;
    rider_last_adv_core = core;
}

/** Try to flush the one outstanding Control Point response indication. */
static void rider_try_send_pending_cp_response(void)
{
    int result;

    if (!rider_pending_cp_response_valid || rider_cp_indication_in_flight ||
        !rider_connection_handle) {
        return;
    }
    result = rider_send_subscribed_value(rider_connection_handle,
                                         RIDER_ATT_CORE_CONTROL_POINT_VALUE_HANDLE,
                                         RIDER_ATT_CORE_CONTROL_POINT_CCC_HANDLE,
                                         GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION,
                                         rider_pending_cp_response,
                                         sizeof(rider_pending_cp_response));
    if (result == GATT_OP_RET_SUCESS) {
        rider_pending_cp_response_valid = 0;
        rider_cp_indication_in_flight = 1;
    }
}

/** Queue a CORE Control Point response until indication CCCD and ATT flow allow it. */
static int rider_queue_control_point_response(u8 opcode, u8 result)
{
    if (rider_pending_cp_response_valid || rider_cp_indication_in_flight) {
        return GATT_CMD_RET_BUSY;
    }
    rider_pending_cp_response[0] = 0x80;
    rider_pending_cp_response[1] = opcode;
    rider_pending_cp_response[2] = result;
    rider_pending_cp_response_valid = 1;
    rider_try_send_pending_cp_response();
    return GATT_OP_RET_SUCESS;
}

/** Implement only the documented direct external-heart-rate operation (0x13). */
static int rider_handle_control_point(const u8 *buffer, u16 buffer_size)
{
    u8 opcode;
    int result;

    if (!buffer || !buffer_size) {
        return GATT_CMD_PARAM_ERROR;
    }
    if (rider_pending_cp_response_valid || rider_cp_indication_in_flight) {
        return GATT_CMD_RET_BUSY;
    }
    opcode = buffer[0];
    if (opcode != 0x13) {
        return rider_queue_control_point_response(opcode, 0x02); /* unsupported */
    }

    if (buffer_size == 1) {
        rider_estimator_set_external_heart_rate(0, 0);
        rider_external_hr_age = 0;
        result = rider_queue_control_point_response(opcode, 0x01);
    } else if (buffer_size == 2 && buffer[1] != 0) {
        rider_estimator_set_external_heart_rate(buffer[1], 1);
        rider_external_hr_age = 0;
        result = rider_queue_control_point_response(opcode, 0x01);
    } else {
        result = rider_queue_control_point_response(opcode, 0x03); /* invalid parameter */
    }
    return result;
}

/** Handle dynamic ATT reads for GAP, CORE, standard and device information data. */
static uint16_t rider_att_read_callback(hci_con_handle_t connection_handle,
                                        uint16_t att_handle,
                                        uint16_t offset,
                                        uint8_t *buffer,
                                        uint16_t buffer_size)
{
    u8 value[32];
    rider_temperature_snapshot_t snapshot;
    u16 value_len;

    switch (att_handle) {
    case RIDER_ATT_GAP_NAME_VALUE_HANDLE:
        return rider_copy_value((const u8 *)RIDER_CORE_TEMP_NAME,
                                strlen(RIDER_CORE_TEMP_NAME), offset,
                                buffer, buffer_size);
    case RIDER_ATT_GAP_APPEARANCE_VALUE_HANDLE:
        value[0] = 0x00;
        value[1] = 0x03; /* Generic Thermometer appearance. */
        return rider_copy_value(value, 2, offset, buffer, buffer_size);
    case RIDER_ATT_GAP_CONN_PARAMS_VALUE_HANDLE:
        /* 20-40 ms interval, no latency, 6 s supervision timeout. */
        value[0] = 0x10;
        value[1] = 0x00;
        value[2] = 0x20;
        value[3] = 0x00;
        value[4] = 0x00;
        value[5] = 0x00;
        value[6] = 0x58;
        value[7] = 0x02;
        return rider_copy_value(value, 8, offset, buffer, buffer_size);
    case RIDER_ATT_CORE_TEMPERATURE_VALUE_HANDLE:
        rider_estimator_copy_snapshot(&snapshot);
        value_len = rider_make_core_frame(value, sizeof(value), &snapshot);
        return rider_copy_value(value, value_len, offset, buffer, buffer_size);
    case RIDER_ATT_STANDARD_TEMPERATURE_VALUE_HANDLE:
        rider_estimator_copy_snapshot(&snapshot);
        value_len = rider_make_standard_temperature(value, sizeof(value), &snapshot);
        return rider_copy_value(value, value_len, offset, buffer, buffer_size);
    case RIDER_ATT_TEMPERATURE_TYPE_VALUE_HANDLE:
        value[0] = 0x02;
        return rider_copy_value(value, 1, offset, buffer, buffer_size);
    case RIDER_ATT_BATTERY_VALUE_HANDLE:
        value[0] = rider_get_battery_level();
        return rider_copy_value(value, 1, offset, buffer, buffer_size);
    case RIDER_ATT_MANUFACTURER_VALUE_HANDLE:
        return rider_copy_value((const u8 *)RIDER_CORE_TEMP_MANUFACTURER,
                                strlen(RIDER_CORE_TEMP_MANUFACTURER), offset,
                                buffer, buffer_size);
    case RIDER_ATT_MODEL_VALUE_HANDLE:
        return rider_copy_value((const u8 *)RIDER_CORE_TEMP_MODEL,
                                strlen(RIDER_CORE_TEMP_MODEL), offset,
                                buffer, buffer_size);
    case RIDER_ATT_FIRMWARE_VALUE_HANDLE:
        return rider_copy_value((const u8 *)RIDER_CORE_TEMP_FIRMWARE_VERSION,
                                strlen(RIDER_CORE_TEMP_FIRMWARE_VERSION), offset,
                                buffer, buffer_size);
    case RIDER_ATT_SERVICE_CHANGED_CCC_HANDLE:
    case RIDER_ATT_CORE_TEMPERATURE_CCC_HANDLE:
    case RIDER_ATT_CORE_CONTROL_POINT_CCC_HANDLE:
    case RIDER_ATT_STANDARD_TEMPERATURE_CCC_HANDLE:
    case RIDER_ATT_BATTERY_CCC_HANDLE:
        return rider_copy_ccc(connection_handle, att_handle, offset, buffer, buffer_size);
    default:
        return 0;
    }
}

/** Handle CCCD writes and the CORE Control Point write operation. */
static int rider_att_write_callback(hci_con_handle_t connection_handle,
                                    uint16_t att_handle,
                                    uint16_t transaction_mode,
                                    uint16_t offset,
                                    uint8_t *buffer,
                                    uint16_t buffer_size)
{
    u16 ccc_config;

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE || offset != 0) {
        return GATT_CMD_PARAM_ERROR;
    }

    switch (att_handle) {
    case RIDER_ATT_SERVICE_CHANGED_CCC_HANDLE:
        if (!buffer || buffer_size != 2) {
            return GATT_CMD_PARAM_ERROR;
        }
        ccc_config = ((u16)buffer[0] | ((u16)buffer[1] << 8)) &
                     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION;
        return ble_gatt_server_characteristic_ccc_set(connection_handle, att_handle,
                                                       ccc_config);
    case RIDER_ATT_CORE_TEMPERATURE_CCC_HANDLE:
    case RIDER_ATT_STANDARD_TEMPERATURE_CCC_HANDLE:
    case RIDER_ATT_BATTERY_CCC_HANDLE:
        if (!buffer || buffer_size != 2) {
            return GATT_CMD_PARAM_ERROR;
        }
        ccc_config = ((u16)buffer[0] | ((u16)buffer[1] << 8)) &
                     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        {
            int result = ble_gatt_server_characteristic_ccc_set(connection_handle,
                                                                att_handle, ccc_config);
            if (att_handle == RIDER_ATT_BATTERY_CCC_HANDLE && result == GATT_OP_RET_SUCESS) {
                rider_battery_level_valid = 0;
                rider_refresh_battery();
            }
            return result;
        }
    case RIDER_ATT_CORE_CONTROL_POINT_CCC_HANDLE:
        if (!buffer || buffer_size != 2) {
            return GATT_CMD_PARAM_ERROR;
        }
        ccc_config = ((u16)buffer[0] | ((u16)buffer[1] << 8)) &
                     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION;
        {
            int result = ble_gatt_server_characteristic_ccc_set(connection_handle,
                                                                att_handle, ccc_config);
            rider_try_send_pending_cp_response();
            return result;
        }
    case RIDER_ATT_CORE_CONTROL_POINT_VALUE_HANDLE:
        return rider_handle_control_point(buffer, buffer_size);
    default:
        return GATT_CMD_PARAM_ERROR;
    }
}

/** Consume GATT lifecycle events and keep the single-connection state coherent. */
static int rider_event_packet_handler(int event, u8 *packet, u16 size, u8 *ext_param)
{
    u16 handle;

    switch (event) {
    case GATT_COMM_EVENT_CONNECTION_COMPLETE:
        if (packet && size >= 2) {
            rider_connection_handle = little_endian_read_16(packet, 0);
            rider_tick_count = 0;
            rider_external_hr_age = 0;
            rider_pending_cp_response_valid = 0;
            rider_cp_indication_in_flight = 0;
            rider_battery_level_valid = 0;
            rider_estimator_set_external_heart_rate(0, 0);
        }
        break;
    case GATT_COMM_EVENT_ENCRYPTION_REQUEST:
        /* The common server auto-confirms Just Works when this returns 0. */
        log_info("Rider security request: handle=%04x process=%d\n",
                 packet && size >= 2 ? little_endian_read_16(packet, 0) : 0,
                 ext_param ? ext_param[0] : 0);
        break;
    case GATT_COMM_EVENT_ENCRYPTION_CHANGE:
        if (packet && size >= 4) {
            log_info("Rider security result: handle=%04x status=%d process=%d\n",
                     little_endian_read_16(packet, 0), packet[2], packet[3]);
        }
        break;
    case GATT_COMM_EVENT_DISCONNECT_COMPLETE:
        if (packet && size >= 2) {
            handle = little_endian_read_16(packet, 0);
            log_info("Rider disconnect: handle=%04x reason=%02x\n",
                     handle, size >= 3 ? packet[2] : 0);
            if (handle == rider_connection_handle) {
                rider_connection_handle = 0;
                rider_external_hr_age = 0;
                rider_pending_cp_response_valid = 0;
                rider_cp_indication_in_flight = 0;
                rider_battery_level_valid = 0;
                rider_estimator_set_external_heart_rate(0, 0);
            }
        }
        break;
    case GATT_COMM_EVENT_SERVER_STATE:
        if (packet && size >= 3 &&
            (packet[0] == BLE_ST_IDLE || packet[0] == BLE_ST_DISCONN) &&
            little_endian_read_16(packet, 1) == rider_connection_handle) {
            rider_connection_handle = 0;
            rider_external_hr_age = 0;
            rider_pending_cp_response_valid = 0;
            rider_cp_indication_in_flight = 0;
            rider_battery_level_valid = 0;
            rider_estimator_set_external_heart_rate(0, 0);
        }
        break;
    case GATT_COMM_EVENT_CAN_SEND_NOW:
        rider_try_send_pending_cp_response();
        break;
    case GATT_COMM_EVENT_SERVER_INDICATION_COMPLETE:
        /* The common layer reports connection and value handles in packet[0:4].
         * Keep a queued response if an unrelated indication completes. */
        if (packet && size >= 4 &&
            little_endian_read_16(packet, 0) == rider_connection_handle &&
            little_endian_read_16(packet, 2) == RIDER_ATT_CORE_CONTROL_POINT_VALUE_HANDLE) {
            rider_cp_indication_in_flight = 0;
        }
        break;
    default:
        break;
    }
    return GATT_OP_RET_SUCESS;
}

/** Register the static profile and advertising payload after stack init. */
void rider_core_temp_gatt_before_init(void)
{
    if (!rider_gatt_ready) {
        ble_comm_init(&rider_gatt_control_block);
        rider_gatt_ready = 1;
    }
}

/** Install the profile and configure CORE-compatible advertising. */
void rider_core_temp_gatt_init(void)
{
    rider_temperature_snapshot_t snapshot;

    rider_connection_handle = 0;
    rider_tick_count = 0;
    rider_external_hr_age = 0;
    rider_pending_cp_response_valid = 0;
    rider_cp_indication_in_flight = 0;
    rider_battery_level_valid = 0;
    rider_last_adv_valid = 0;
    rider_last_adv_core = 0;
    rider_estimator_copy_snapshot(&snapshot);
    ble_comm_set_config_name(RIDER_CORE_TEMP_NAME, 0);
    ble_gatt_server_set_profile(rider_core_temp_profile_data,
                                sizeof(rider_core_temp_profile_data));
    rider_make_advertisement(&snapshot);
    ble_gatt_server_set_adv_config(&rider_adv_config);
}

/** Stop the GATT common module and release protocol state. */
void rider_core_temp_gatt_exit(void)
{
    rider_pending_cp_response_valid = 0;
    rider_cp_indication_in_flight = 0;
    rider_battery_level_valid = 0;
    rider_connection_handle = 0;
    if (rider_gatt_ready) {
        ble_comm_exit();
        rider_gatt_ready = 0;
    }
}

/** Product-facing BLE module switch used by update/common hooks. */
void ble_module_enable(uint8_t enable)
{
    ble_comm_module_enable(enable);
}

/** Publish CORE and standard measurements at their documented cadences. */
void rider_core_temp_ble_tick(void)
{
    u8 core_frame[RIDER_CORE_FRAME_MAX];
    u8 standard_value[6];
    u16 core_frame_len;
    u16 standard_value_len;
    rider_temperature_snapshot_t snapshot;

    rider_tick_count++;
    rider_estimator_copy_snapshot(&snapshot);
    if (snapshot.heart_rate_valid) {
        if (rider_external_hr_age < 0xff) {
            rider_external_hr_age++;
        }
        if (rider_external_hr_age >= RIDER_EXTERNAL_HR_TIMEOUT) {
            rider_estimator_set_external_heart_rate(0, 0);
            snapshot.heart_rate_valid = 0;
        }
    }

    rider_refresh_advertisement(&snapshot);
    rider_refresh_battery();
    if (!rider_connection_handle) {
        return;
    }

    core_frame_len = rider_make_core_frame(core_frame, sizeof(core_frame), &snapshot);
    rider_send_subscribed_value(rider_connection_handle,
                                RIDER_ATT_CORE_TEMPERATURE_VALUE_HANDLE,
                                RIDER_ATT_CORE_TEMPERATURE_CCC_HANDLE,
                                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION,
                                core_frame, core_frame_len);

    if ((rider_tick_count % RIDER_STANDARD_PERIOD_SECONDS) == 0) {
        standard_value_len = rider_make_standard_temperature(standard_value,
                                                              sizeof(standard_value),
                                                              &snapshot);
        rider_send_subscribed_value(rider_connection_handle,
                                    RIDER_ATT_STANDARD_TEMPERATURE_VALUE_HANDLE,
                                    RIDER_ATT_STANDARD_TEMPERATURE_CCC_HANDLE,
                                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION,
                                    standard_value, standard_value_len);
    }
}

#endif
