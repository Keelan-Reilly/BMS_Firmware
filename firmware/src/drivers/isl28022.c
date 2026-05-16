/* isl28022.c — ISL28022 power monitor driver. */
#include "isl28022.h"
#include "board_i2c.h"
#include "board_pins.h"
#include <stdint.h>

/* Config register value: 60V range, PGA/8, 12-bit bus+shunt, continuous */
#define CFG_VALUE \
    ((uint16_t)( \
        (ISL28022_BRNG_60V             << 13u) | \
        (ISL28022_PG_DIV8              << 11u) | \
        (ISL28022_ADC_12BIT_532US      <<  7u) | \
        (ISL28022_ADC_12BIT_532US      <<  3u) | \
         ISL28022_MODE_CONT_SHUNT_BUS))

static BmsResult write_reg16(uint8_t reg, uint16_t val) {
    uint8_t data[2] = { (uint8_t)(val >> 8u), (uint8_t)(val & 0xFFu) };
    return board_i2c_write_reg(ISL28022_I2C_ADDR, reg, data, 2u);
}

static BmsResult read_reg16(uint8_t reg, uint16_t *out) {
    uint8_t buf[2];
    BmsResult r = board_i2c_read_reg(ISL28022_I2C_ADDR, reg, buf, 2u);
    if (r == BMS_OK) { *out = ((uint16_t)buf[0] << 8u) | buf[1]; }
    return r;
}

BmsResult isl28022_init(void) {
    /* Write configuration register */
    BmsResult r = write_reg16(ISL28022_REG_CONFIG, CFG_VALUE);
    if (r != BMS_OK) { return r; }

    /* Calibration register: not used here (current reg requires calibration).
     * We read raw shunt voltage and apply gain in bms_measurements instead.
     * Set calibration to 0 to disable internal current/power registers. */
    return write_reg16(ISL28022_REG_CALIB, 0x0000u);
}

BmsResult isl28022_read(int32_t *vbus_raw_mv, int32_t *vshunt_raw_uv) {
    uint16_t vbus_reg;
    BmsResult r = read_reg16(ISL28022_REG_VBUS, &vbus_reg);
    if (r != BMS_OK) { return r; }

    uint16_t vshunt_reg;
    r = read_reg16(ISL28022_REG_VSHUNT, &vshunt_reg);
    if (r != BMS_OK) { return r; }

    /* Bus voltage: bits[15:3] in 4 mV LSB */
    *vbus_raw_mv = (int32_t)((vbus_reg >> 3u) * 4u);

    /* Shunt voltage: 16-bit two's complement, 80 µV LSB (PG=/8) */
    int16_t vshunt_signed = (int16_t)vshunt_reg;
    *vshunt_raw_uv = (int32_t)vshunt_signed * (int32_t)ISL28022_SHUNT_LSB_UV_PG8;

    return BMS_OK;
}
