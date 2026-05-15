/* board_i2c.c — I2C2 stub implementation (PA9=SCL, PA10=SDA).
 * STUB: returns BMS_ERR_NOT_SUPPORTED until real implementation is added.
 */
#include "board_i2c.h"
#include "board_pins.h"
#include "bms_hal.h"

void board_i2c_init(void) {
    /* PA9 SCL, PA10 SDA → AF4 open-drain */
    I2C_PORT->MODER &= ~(0xFu << 18);
    I2C_PORT->MODER |= (GPIO_MODER_AF << 18) | (GPIO_MODER_AF << 20);
    I2C_PORT->OTYPER |= (1u << I2C_SCL_PIN) | (1u << I2C_SDA_PIN);
    I2C_PORT->AFR[1] &= ~(0xFFu << 4);
    I2C_PORT->AFR[1] |= (I2C_AF << 4) | (I2C_AF << 8);
    /* I2C peripheral configuration: STUB — add timing register setup */
}

BmsResult board_i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr,
                              uint8_t *buf, uint8_t len) {
    (void)dev_addr; (void)reg_addr; (void)buf; (void)len;
    return BMS_ERR_NOT_SUPPORTED; /* STUB */
}

BmsResult board_i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr,
                               const uint8_t *data, uint8_t len) {
    (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return BMS_ERR_NOT_SUPPORTED; /* STUB */
}
