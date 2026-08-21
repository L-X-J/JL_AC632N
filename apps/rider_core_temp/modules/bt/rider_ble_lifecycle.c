#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"
#include "bt_common.h"
#include "btcontroller_modules.h"
#include "btstack/btstack_task.h"
#include "btstack/avctp_user.h"
#include "btstack/le/ble_api.h"
#include "gatt_common/le_gatt_common.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/** Configure the LE controller address and GATT common module before btstack. */
void btstack_ble_start_before_init(const rider_ble_init_cfg_t *cfg, int param)
{
    u8 ble_address[6];
    const u8 *base_address;

    (void)param;
    base_address = bt_get_mac_addr();
    if (!base_address) {
        memset(ble_address, 0, sizeof(ble_address));
    } else if (cfg && cfg->same_address) {
        memcpy(ble_address, base_address, sizeof(ble_address));
    } else {
        lib_make_ble_address(ble_address, (u8 *)base_address);
    }
    le_controller_set_mac(ble_address);
    bt_ble_before_start_init();
}

/** Finish the LE application setup after btstack reports BT_STATUS_INIT_OK. */
void btstack_ble_start_after_init(int param)
{
    (void)param;
    bt_ble_init();
}

/** Tear down the product BLE stack in the reverse order of startup. */
void btstack_ble_exit(int param)
{
    (void)param;
    bt_ble_exit();
#if TCFG_USER_EDR_ENABLE == 0
    btstack_exit();
#endif
}

/** Forward the SDK's asynchronous status event to the product lifecycle. */
int bt_comm_ble_status_event_handler(struct bt_event *bt)
{
    if (bt && bt->event == BT_STATUS_INIT_OK) {
        btstack_ble_start_after_init(0);
    }
    return 0;
}

/** Rider has no HCI test-box or classic-Bluetooth event handling. */
int bt_comm_ble_hci_event_handler(struct bt_event *bt)
{
    (void)bt;
    return 0;
}

/** Compatibility hook used by generic SDK update/test modules. */
void bt_ble_adv_enable(u8 enable)
{
    ble_module_enable(enable);
}

/** Disconnect the only possible LE peer. */
void ble_app_disconnect(void)
{
    ble_gatt_server_disconnect_all();
}

/** Register the GATT common callbacks before the controller starts. */
void bt_ble_before_start_init(void)
{
    rider_core_temp_gatt_before_init();
}

/** Register profile, enable advertising and begin sensor sampling. */
void bt_ble_init(void)
{
    rider_core_temp_gatt_init();
    ble_module_enable(1);
    /* The scheduler owns sensor/estimator initialization so a restart clears
     * stale samples exactly once before the first conversion is queued. */
    rider_core_temp_start_scheduler();
}

/** Stop sampling before disabling and releasing the GATT common module. */
void bt_ble_exit(void)
{
    rider_core_temp_stop_scheduler();
    ble_module_enable(0);
    rider_core_temp_gatt_exit();
}

#endif
