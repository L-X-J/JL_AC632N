#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "app_main.h"
#include "bt_common.h"
#include "btcontroller_modules.h"
#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "rider_core_temp.h"

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[RIDER_APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/* Keep only tasks required by the application, BLE controller and SDK timers. */
const struct task_info task_info_table[] = {
    {"app_core", 1, 0, 640, 128},
    {"sys_event", 7, 0, 256, 0},
    {"btctrler", 4, 0, 512, 256},
    {"btencry", 1, 0, 512, 128},
    {"btstack", 3, 0, 768, 256},
    {"systimer", 7, 0, 128, 0},
    {"update", 1, 0, 512, 0},
    {"dw_update", 2, 0, 256, 128},
#if RCSP_BTMATE_EN
    {"rcsp_task", 2, 0, 640, 0},
#endif
    {0, 0},
};

APP_VAR app_var;

static u8 rider_btstack_started;
static const rider_ble_init_cfg_t rider_ble_config = {
    .same_address = 1,
    .appearance = 0x0300,
};

/** Initialise the small compatibility state consumed by SDK library code. */
void app_var_init(void)
{
    memset(&app_var, 0, sizeof(app_var));
    app_var.play_poweron_tone = 0;
    app_var.auto_off_time = 0;
    app_var.warning_tone_v = 340;
    app_var.poweroff_tone_v = 330;
    app_var.rf_power = 10;
}

/** Start the one BLE peripheral stack used by the Rider product. */
static void rider_app_start(void)
{
    if (rider_btstack_started) {
        return;
    }

    rider_btstack_started = 1;
    clk_set("sys", BT_NORMAL_HZ);
    bt_pll_para(TCFG_CLOCK_OSC_HZ, clk_get("sys"), 0, 0);
    btstack_ble_start_before_init(&rider_ble_config, 0);
    btstack_init();
}

/** Stop the BLE stack before the application instance is destroyed. */
static void rider_app_stop(void)
{
    if (!rider_btstack_started) {
        return;
    }

    btstack_ble_exit(0);
    rider_btstack_started = 0;
}

/** Handle application state transitions without introducing product logic. */
static int rider_state_machine(struct application *app, enum app_state state,
                               struct intent *it)
{
    (void)app;
    switch (state) {
    case APP_STA_START:
        if (it && it->action == ACTION_RIDER_CORE_TEMP) {
            rider_app_start();
        }
        break;
    case APP_STA_STOP:
    case APP_STA_DESTROY:
        rider_app_stop();
        break;
    default:
        break;
    }
    return 0;
}

/** Forward only Bluetooth status/HCI events to the local BLE lifecycle. */
static int rider_event_handler(struct application *app, struct sys_event *event)
{
    (void)app;
    if (!event) {
        return FALSE;
    }

    if (event->type == SYS_BT_EVENT) {
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            bt_comm_ble_status_event_handler(&event->u.bt);
            return 0;
        }
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
            bt_comm_ble_hci_event_handler(&event->u.bt);
            return 0;
        }
    }
    return FALSE;
}

static const struct application_operation rider_application_ops = {
    .state_machine = rider_state_machine,
    .event_handler = rider_event_handler,
};

REGISTER_APPLICATION(rider_core_temp_application) = {
    .name = "rider_core_temp",
    .action = ACTION_RIDER_CORE_TEMP,
    .ops = &rider_application_ops,
    .state = APP_STA_DESTROY,
};

/** Enter the product application selected by the board target. */
void app_main(void)
{
    struct intent it;

    app_var_init();
    init_intent(&it);
    it.name = "rider_core_temp";
    it.action = ACTION_RIDER_CORE_TEMP;
    start_app(&it);
}

/** Replace the current application and start the requested action. */
void app_switch(const char *name, int action)
{
    struct intent it;
    struct application *app;

    init_intent(&it);
    app = get_current_app();
    if (app) {
        it.name = app->name;
        it.action = ACTION_BACK;
        start_app(&it);
    }
    it.name = name;
    it.action = action;
    start_app(&it);
}

/** Keep the always-on BLE application eligible for system idle handling. */
int eSystemConfirmStopStatus(void)
{
    return 1;
}

__attribute__((used)) int *__errno(void)
{
    static int err;
    return &err;
}

#endif
