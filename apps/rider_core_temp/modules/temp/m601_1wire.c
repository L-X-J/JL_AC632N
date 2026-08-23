#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"

#define LOG_TAG_CONST       RIDER_TEMP
#define LOG_TAG             "[RIDER_TEMP]"
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/* M601 follows the 1-Wire command sequence shown in doc/temp_sample. */
#define RIDER_M601_DQ_PORT             IO_PORTB_07
#define RIDER_M601_CONVERT_DELAY_MS    15
#define RIDER_M601_SCRATCHPAD_SIZE     9
#define RIDER_M601_MIN_TEMP_CENTI      (-4000)
#define RIDER_M601_MAX_TEMP_CENTI      12500

/* The supplied sample names byte 8 as a CRC over bytes 0..7.  Its polynomial
 * is not stated in that sample; Dallas/Maxim CRC-8 is the explicit integration
 * assumption and can be disabled by a board bring-up build if M601 documents a
 * different polynomial. */
#ifndef RIDER_M601_VALIDATE_CRC
#define RIDER_M601_VALIDATE_CRC         1
#endif

/* The bd19 SDK declares delay_us() but does not provide that symbol in the
 * libraries used by this target.  Keep the timing primitive local to the
 * product sensor driver and use the same timer path as the SDK charge driver. */
extern const int set_to_close_timer0_delay;

static void rider_delay_us(u32 usec)
{
    if (set_to_close_timer0_delay) {
        JL_MCPWM->MCPWM_CON0 &= ~BIT(8 + 3);
        JL_MCPWM->TMR3_CNT = 0;
        JL_MCPWM->TMR3_PR = clk_get("lsb") / 1000000 * usec;
        JL_MCPWM->TMR3_CON = BIT(10) | BIT(0);
        JL_MCPWM->MCPWM_CON0 |= BIT(8 + 3);
        while (!(JL_MCPWM->TMR3_CON & BIT(12)));
        JL_MCPWM->TMR3_CON = BIT(10);
        JL_MCPWM->MCPWM_CON0 &= ~BIT(8 + 3);
    } else {
        JL_TIMER0->CON = BIT(14);
        JL_TIMER0->CNT = 0;
        JL_TIMER0->PRD = clk_get("timer") / 1000000L * usec;
        JL_TIMER0->CON = BIT(0) | BIT(2) | BIT(6);
        while ((JL_TIMER0->CON & BIT(15)) == 0);
        JL_TIMER0->CON = BIT(14);
    }
}

static rider_temperature_sample_t rider_latest_sample;
static u16 rider_conversion_timeout;
static u8 rider_conversion_pending;
static u8 rider_m601_crc8(const u8 *data, u8 length);

/** Print the raw M601 frame and the validation result for hardware bring-up. */
static void rider_m601_log_sample(const u8 *scratchpad, int status,
                                  int16_t temperature_centi)
{
    u16 raw_bits = (u16)scratchpad[0] | ((u16)scratchpad[1] << 8);
    u8 crc_expected = rider_m601_crc8(scratchpad, 8);

    log_info("M601 read: seq=%u status=%d valid=%u raw=0x%04x temp_centi=%d "
             "crc=%02x/%02x\n",
             (unsigned)rider_latest_sample.sequence, status,
             (unsigned)(status == RIDER_TEMP_STATUS_OK), raw_bits,
             (int)temperature_centi, crc_expected, scratchpad[8]);
    put_buf(scratchpad, RIDER_M601_SCRATCHPAD_SIZE);
}

/** Put PB7 in the released open-drain state used by the sensor bus. */
static void rider_1wire_release(void)
{
    gpio_set_die(RIDER_M601_DQ_PORT, 1);
    gpio_set_pull_down(RIDER_M601_DQ_PORT, 0);
    gpio_set_pull_up(RIDER_M601_DQ_PORT, 1);
    gpio_direction_input(RIDER_M601_DQ_PORT);
}

/** Assert the low phase without sourcing current into the 1-Wire bus. */
static void rider_1wire_drive_low(void)
{
    gpio_set_pull_up(RIDER_M601_DQ_PORT, 0);
    gpio_set_pull_down(RIDER_M601_DQ_PORT, 0);
    gpio_direction_output(RIDER_M601_DQ_PORT, 0);
}

/** Reset the bus and report whether the M601 pulled it low for presence. */
static u8 rider_1wire_reset_presence(void)
{
    u8 present;

    rider_1wire_drive_low();
    rider_delay_us(480);
    rider_1wire_release();
    rider_delay_us(70);
    present = (gpio_read(RIDER_M601_DQ_PORT) == 0);
    rider_delay_us(410);
    return present;
}

/** Write one 1-Wire time slot, least significant bit first. */
static void rider_1wire_write_bit(u8 value)
{
    rider_1wire_drive_low();
    if (value) {
        rider_delay_us(2);
        rider_1wire_release();
        rider_delay_us(68);
    } else {
        rider_delay_us(60);
        rider_1wire_release();
        rider_delay_us(10);
    }
}

/** Read one 1-Wire time slot at the timing used by the supplied sample. */
static u8 rider_1wire_read_bit(void)
{
    u8 value;

    rider_1wire_drive_low();
    rider_delay_us(2);
    rider_1wire_release();
    rider_delay_us(10);
    value = (u8)(gpio_read(RIDER_M601_DQ_PORT) != 0);
    rider_delay_us(58);
    return value;
}

/** Transmit a byte with the 1-Wire LSB-first convention. */
static void rider_1wire_write_byte(u8 value)
{
    u8 bit;

    for (bit = 0; bit < 8; ++bit) {
        rider_1wire_write_bit(value & 0x01);
        value >>= 1;
    }
}

/** Receive a byte with the 1-Wire LSB-first convention. */
static u8 rider_1wire_read_byte(void)
{
    u8 bit;
    u8 value = 0;

    for (bit = 0; bit < 8; ++bit) {
        value |= (u8)(rider_1wire_read_bit() << bit);
    }
    return value;
}

/** Calculate Dallas/Maxim CRC-8 over a scratchpad prefix. */
static u8 rider_m601_crc8(const u8 *data, u8 length)
{
    u8 byte;
    u8 bit;
    u8 crc = 0;

    for (byte = 0; byte < length; ++byte) {
        crc ^= data[byte];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x01) ? (u8)((crc >> 1) ^ 0x8c) : (u8)(crc >> 1);
        }
    }
    return crc;
}

/** Convert the signed M601 raw value to centi-degrees without float support. */
static int16_t rider_m601_to_centi(u16 raw_bits)
{
    int32_t raw = (int16_t)raw_bits;
    int32_t scaled = raw * 100;

    /* Round to the nearest centi-degree without relying on signed shifts. */
    if (scaled >= 0) {
        scaled = (scaled + 128) / 256;
    } else {
        scaled = -(((-scaled) + 128) / 256);
    }
    scaled += 4000;
    return (int16_t)scaled;
}

/** Decode the complete scratchpad and reject electrical/physical outliers. */
static int rider_m601_decode(const u8 *scratchpad, int16_t *temperature_centi)
{
    u16 raw_bits;
    int16_t temperature;

#if RIDER_M601_VALIDATE_CRC
    if (rider_m601_crc8(scratchpad, 8) != scratchpad[8]) {
        return RIDER_TEMP_STATUS_CRC_ERROR;
    }
#endif

    raw_bits = (u16)scratchpad[0] | ((u16)scratchpad[1] << 8);
    temperature = rider_m601_to_centi(raw_bits);
    if (temperature < RIDER_M601_MIN_TEMP_CENTI ||
        temperature > RIDER_M601_MAX_TEMP_CENTI) {
        return RIDER_TEMP_STATUS_RANGE_ERROR;
    }

    *temperature_centi = temperature;
    return RIDER_TEMP_STATUS_OK;
}

/** Advance the sequence and publish an unavailable sample after a failure. */
static void rider_m601_record_failure(u8 status)
{
    rider_latest_sample.sequence++;
    rider_latest_sample.valid = 0;
    rider_latest_sample.status = status;
    rider_latest_sample.temperature_centi = 0x7fff;
    log_info("M601 sample failure: seq=%u status=%u valid=0 temp_centi=32767\n",
             (unsigned)rider_latest_sample.sequence, (unsigned)status);
}

/** Read and validate the scratchpad after the conversion delay expires. */
static void rider_m601_complete_conversion(void *priv)
{
    u8 scratchpad[RIDER_M601_SCRATCHPAD_SIZE];
    int16_t temperature_centi = 0x7fff;
    u8 index;
    int status;

    (void)priv;
    rider_conversion_timeout = 0;
    if (!rider_conversion_pending) {
        return;
    }
    rider_conversion_pending = 0;

    if (!rider_1wire_reset_presence()) {
        rider_m601_record_failure(RIDER_TEMP_STATUS_NO_DEVICE);
        return;
    }

    rider_1wire_write_byte(0xcc); /* SKIP ROM */
    rider_1wire_write_byte(0xbe); /* READ SCRATCHPAD */
    for (index = 0; index < RIDER_M601_SCRATCHPAD_SIZE; ++index) {
        scratchpad[index] = rider_1wire_read_byte();
    }

    status = rider_m601_decode(scratchpad, &temperature_centi);
    rider_latest_sample.sequence++;
    rider_latest_sample.status = (u8)status;
    rider_latest_sample.valid = (status == RIDER_TEMP_STATUS_OK);
    if (rider_latest_sample.valid) {
        rider_latest_sample.temperature_centi = temperature_centi;
    } else {
        rider_latest_sample.temperature_centi = 0x7fff;
    }
    rider_m601_log_sample(scratchpad, status, temperature_centi);
    rider_1wire_release();
}

/** Initialise PB7 and clear any in-flight conversion state. */
void rider_temp_init(void)
{
    memset(&rider_latest_sample, 0, sizeof(rider_latest_sample));
    rider_latest_sample.status = RIDER_TEMP_STATUS_NO_DEVICE;
    rider_conversion_timeout = 0;
    rider_conversion_pending = 0;
    log_info("M601 init: port=PB7 convert_delay_ms=%u crc_check=%u\n",
             RIDER_M601_CONVERT_DELAY_MS, RIDER_M601_VALIDATE_CRC);
    rider_1wire_release();
}

/** Cancel both the one-shot conversion callback and the bus operation. */
void rider_temp_stop(void)
{
    if (rider_conversion_timeout) {
        sys_timeout_del(rider_conversion_timeout);
        rider_conversion_timeout = 0;
    }
    rider_conversion_pending = 0;
    rider_1wire_release();
}

/** Start a conversion without blocking the BLE/event task for 15 ms. */
void rider_temp_start_conversion(void)
{
    if (rider_conversion_pending) {
        return;
    }
    if (!rider_1wire_reset_presence()) {
        rider_m601_record_failure(RIDER_TEMP_STATUS_NO_DEVICE);
        return;
    }

    log_info("M601 convert start: next_seq=%u\n",
             (unsigned)(rider_latest_sample.sequence + 1));
    rider_1wire_write_byte(0xcc); /* SKIP ROM */
    rider_1wire_write_byte(0x44); /* CONVERT T */
    rider_conversion_pending = 1;
    rider_conversion_timeout = sys_timeout_add(NULL, rider_m601_complete_conversion,
                                                RIDER_M601_CONVERT_DELAY_MS);
    if (!rider_conversion_timeout) {
        rider_conversion_pending = 0;
        rider_m601_record_failure(RIDER_TEMP_STATUS_NO_DEVICE);
        rider_1wire_release();
    }
}

/** Return the completion sequence used by the scheduler to detect new data. */
uint32_t rider_temp_sequence(void)
{
    return rider_latest_sample.sequence;
}

/** Copy the latest completed sample without exposing driver-owned storage. */
int rider_temp_copy_latest(rider_temperature_sample_t *sample)
{
    if (!sample || !rider_latest_sample.sequence) {
        return 0;
    }
    memcpy(sample, &rider_latest_sample, sizeof(*sample));
    return 1;
}

#endif
