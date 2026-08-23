#include "app_config.h"
#include "app_main.h"
#include "bt_common.h"
#include "btcontroller_config.h"
#include "syscfg_id.h"
#include "system/includes.h"
#include "user_cfg.h"
#include "rider_core_temp.h"

#define LOG_TAG_CONST       USER_CFG
#define LOG_TAG             "[RIDER_CFG]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

#define RIDER_RF_POWER_LEVEL       10
#define RIDER_BLE_POWER_LEVEL      6

BT_CONFIG bt_cfg = {
    .edr_name = RIDER_CORE_TEMP_NAME,
    .mac_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .tws_local_addr = {0, 0, 0, 0, 0, 0},
    .rf_power = RIDER_RF_POWER_LEVEL,
    .dac_analog_gain = 0,
    .mic_analog_gain = 0,
    .tws_device_indicate = 0,
};

const struct btif_item btif_table[] = {
    {CFG_BT_MAC_ADDR, 6},
    {CFG_BT_FRE_OFFSET, 6},
    {0, 0},
};

const int vm_max_size_config = VM_MAX_SIZE_CONFIG;

struct lp_ws_t lp_winsize = {
    .lrc_ws_inc = 480,
    .lrc_ws_init = 160,
    .bt_osc_ws_inc = 100,
    .bt_osc_ws_init = 140,
    .osc_change_mode = 0,
};

void lp_winsize_init(struct lp_ws_t *lp);

static STATUS rider_status_table;

/** Return the fixed product name; VM configuration cannot override it. */
const char *bt_get_local_name(void)
{
    return RIDER_CORE_TEMP_NAME;
}

/** Ignore generic name writes so the advertised product identity stays fixed. */
void bt_set_local_name(char *name, u8 len)
{
    (void)name;
    (void)len;
    memcpy(bt_cfg.edr_name, RIDER_CORE_TEMP_NAME, sizeof(RIDER_CORE_TEMP_NAME));
}

/** Return the persisted BR/LE base address selected during board init. */
const u8 *bt_get_mac_addr(void)
{
    return bt_cfg.mac_addr;
}

/** Update the in-memory address and persist it for the next boot. */
void bt_set_mac_addr(u8 *addr)
{
    if (!addr) {
        return;
    }
    memcpy(bt_cfg.mac_addr, addr, sizeof(bt_cfg.mac_addr));
    syscfg_write(CFG_BT_MAC_ADDR, bt_cfg.mac_addr, sizeof(bt_cfg.mac_addr));
}

/** Return the address to SDK test-box compatibility callers. */
void bt_get_vm_mac_addr(u8 *addr)
{
    if (addr) {
        memcpy(addr, bt_cfg.mac_addr, sizeof(bt_cfg.mac_addr));
    }
}

/** TWS is intentionally absent, so its local address is all zeroes. */
void bt_get_tws_local_addr(u8 *addr)
{
    if (addr) {
        memset(addr, 0, 6);
    }
}

/** Return a neutral value for code that queries the unused TWS marker. */
u16 bt_get_tws_device_indicate(u8 *tws_device_indicate)
{
    (void)tws_device_indicate;
    return 0;
}

/** Provide the legacy SDK PIN fallback; Rider BLE uses unattended Just Works. */
const char *bt_get_pin_code(void)
{
    return "0000";
}

/** Keep update/library volume queries harmless on this non-audio product. */
u8 get_max_sys_vol(void)
{
    return 15;
}

/** Keep the common tone-volume query deterministic without audio hardware. */
u8 get_tone_vol(void)
{
    return 15;
}

/** Return empty status records for SDK compatibility paths that remain linked. */
STATUS *get_led_config(void)
{
    return &rider_status_table;
}

/** Return empty status records for SDK compatibility paths that remain linked. */
STATUS *get_tone_config(void)
{
    return &rider_status_table;
}

/** Persist and return a valid local address, generating one on first boot. */
static void rider_load_mac(void)
{
    u8 mac[6];
    u8 invalid = 1;
    u8 all_zero = 1;
    u8 all_ff = 1;
    u8 index;

    if (syscfg_read(CFG_BT_MAC_ADDR, mac, sizeof(mac)) == sizeof(mac)) {
        invalid = 0;
        for (index = 0; index < sizeof(mac); ++index) {
            all_zero &= (mac[index] == 0);
            all_ff &= (mac[index] == 0xff);
        }
        invalid = all_zero || all_ff;
    }
    if (invalid) {
        get_random_number(mac, sizeof(mac));
        mac[0] = (mac[0] & 0xfc) | 0x02; /* locally administered unicast */
        syscfg_write(CFG_BT_MAC_ADDR, mac, sizeof(mac));
    }
    memcpy(bt_cfg.mac_addr, mac, sizeof(bt_cfg.mac_addr));
}

/** Read only hardware-independent settings and preserve the fixed identity. */
void cfg_file_parse(u8 idx)
{
    (void)idx;
    memcpy(bt_cfg.edr_name, RIDER_CORE_TEMP_NAME, sizeof(RIDER_CORE_TEMP_NAME));
    rider_load_mac();
    bt_cfg.rf_power = RIDER_RF_POWER_LEVEL;
    bt_max_pwr_set(bt_cfg.rf_power, 5, 8, RIDER_BLE_POWER_LEVEL);
    lp_winsize_init(&lp_winsize);
}

/** Compatibility hook used by SDK code that requests a MAC refresh. */
void bt_update_mac_addr(u8 *addr)
{
    bt_set_mac_addr(addr);
}

/** Read the current address after a reset request. */
void bt_reset_and_get_mac_addr(u8 *addr)
{
    if (addr) {
        memcpy(addr, bt_cfg.mac_addr, sizeof(bt_cfg.mac_addr));
    }
}

/** Pair-code control is a no-op because Rider has no classic pairing. */
void bt_set_pair_code_en(u8 en)
{
    (void)en;
}

#endif
