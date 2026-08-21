#ifndef RIDER_CORE_TEMP_APP_MAIN_H
#define RIDER_CORE_TEMP_APP_MAIN_H

#include "system/includes.h"

typedef struct _APP_VAR {
    s8 music_volume;
    s8 call_volume;
    s8 wtone_volume;
    u8 opid_play_vol_sync;
    u8 aec_dac_gain;
    u8 aec_mic_gain;
    u8 rf_power;
    u8 goto_poweroff_flag;
    u8 goto_poweroff_cnt;
    u8 play_poweron_tone;
    u8 remote_dev_company;
    u8 siri_stu;
    int auto_stop_page_scan_timer;
    volatile int auto_shut_down_timer;
    volatile int wait_exit_timer;
    u16 auto_off_time;
    u16 warning_tone_v;
    u16 poweroff_tone_v;
    u32 start_time;
    s8 usb_mic_gain;
} APP_VAR;

extern APP_VAR app_var;

void app_main(void);
void app_var_init(void);
void app_switch(const char *name, int action);

#endif
