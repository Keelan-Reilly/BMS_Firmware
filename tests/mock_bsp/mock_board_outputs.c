/* mock_board_outputs.c — mock output driver for host tests. */
#include "board_outputs.h"
#include <stdbool.h>

static bool s_master_ok, s_discharge, s_charge, s_charger_safety;
static BmsOutputsBitmask s_state;

void board_outputs_init_safe(void) {
    s_master_ok = s_discharge = s_charge = s_charger_safety = false;
    s_state = 0;
}

void board_outputs_set_master_ok(bool a) {
    s_master_ok = a;
    if (a) s_state |= OUTPUTS_BIT_MASTER_OK; else s_state &= ~OUTPUTS_BIT_MASTER_OK;
}
void board_outputs_set_discharge_permission(bool a) {
    s_discharge = a;
    if (a) s_state |= OUTPUTS_BIT_DISCHARGE; else s_state &= ~OUTPUTS_BIT_DISCHARGE;
}
void board_outputs_set_charge_permission(bool a) {
    s_charge = a;
    if (a) s_state |= OUTPUTS_BIT_CHARGE; else s_state &= ~OUTPUTS_BIT_CHARGE;
}
void board_outputs_set_charger_safety(bool a) {
    s_charger_safety = a;
    if (a) s_state |= OUTPUTS_BIT_CHARGER_SAFETY; else s_state &= ~OUTPUTS_BIT_CHARGER_SAFETY;
}
void board_outputs_assert_power_enable(void) {}
void board_outputs_release_power(void) {}
void board_outputs_set_led0(bool on) { (void)on; }
void board_outputs_set_power_led(bool on) { (void)on; }
void board_outputs_disable_all(void) {
    s_master_ok = s_discharge = s_charge = s_charger_safety = false;
    s_state = 0;
}
BmsOutputsBitmask board_outputs_get_state(void) { return s_state; }

void board_outputs_get_gpio_snapshot(BmsGpioSnapshot *out) {
    out->cs_cell            = 1u;
    out->cs_temp            = 1u;
    out->power_button       = 0u;
    out->charge_detect      = 0u;
    out->power_enable       = 1u;
    out->master_ok_raw      = (uint8_t)(s_state & OUTPUTS_BIT_MASTER_OK     ? 1u : 0u);
    out->discharge_raw      = (uint8_t)(s_state & OUTPUTS_BIT_DISCHARGE     ? 1u : 0u);
    out->charge_raw         = (uint8_t)(s_state & OUTPUTS_BIT_CHARGE        ? 1u : 0u);
    out->charger_safety_raw = (uint8_t)(s_state & OUTPUTS_BIT_CHARGER_SAFETY ? 1u : 0u);
}
