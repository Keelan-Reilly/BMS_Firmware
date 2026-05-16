/* isl28022.h — ISL28022 power monitor driver (I2C, 16-bit ADC).
 *
 * Used for Vbat and I_batt measurement. PA9=SCL, PA10=SDA via I2C2.
 * I2C address: ISL28022_I2C_ADDR (from board_pins.h — confirm A0/A1 strap).
 *
 * Raw readings are returned to the caller; scaling to engineering units is
 * done in bms_measurements using config-provided gain/offset.
 */
#pragma once
#include <stdint.h>
#include "bms_types.h"

/* ISL28022 register addresses */
#define ISL28022_REG_CONFIG     (0x00u)
#define ISL28022_REG_VSHUNT     (0x01u)
#define ISL28022_REG_VBUS       (0x02u)
#define ISL28022_REG_POWER      (0x03u)
#define ISL28022_REG_CURRENT    (0x04u)
#define ISL28022_REG_CALIB      (0x05u)

/* Configuration register bit fields (ISL28022 datasheet Table 4) */
#define ISL28022_MODE_CONT_SHUNT_BUS  (0x07u)   /* MODE[2:0]: continuous shunt+bus */
#define ISL28022_BRNG_60V             (0x02u)   /* BRNG[1:0]: 60 V bus range */
#define ISL28022_PG_DIV8              (0x03u)   /* PG[1:0]: PGA /8 → ±320 mV range */
#define ISL28022_ADC_12BIT_532US      (0x03u)   /* BADC/SADC: 12-bit, 532 µs */

/* Bus voltage LSB = 4 mV. Shift right 3 for raw value. */
#define ISL28022_BUS_LSB_UV           (4000u)   /* 4 mV per raw count */

/* Shunt voltage LSB depends on PG setting:
 * PG=/1: 10 µV, PG=/2: 20 µV, PG=/4: 40 µV, PG=/8: 80 µV.
 * With PG_DIV8 and max shunt = 320 mV / shunt_mohm. */
#define ISL28022_SHUNT_LSB_UV_PG8    (80u)      /* 80 µV per LSB when PG=/8 */

/* Initialise ISL28022: write config + calibration registers.
 * Must be called after board_i2c_init(). */
BmsResult isl28022_init(void);

/* Read bus voltage (Vbat) and shunt voltage (proportional to current).
 * vbus_raw_mv:  bus voltage in mV (LSB = 4 mV, right-shifted from register).
 * vshunt_raw_uv: shunt voltage in µV (signed, LSB = ISL28022_SHUNT_LSB_UV_PG8).
 * Returns BMS_OK or BMS_ERR_I2C on comms failure. */
BmsResult isl28022_read(int32_t *vbus_raw_mv, int32_t *vshunt_raw_uv);
