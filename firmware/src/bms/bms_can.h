/* bms_can.h — CAN message scheduling and composition.
 *
 * Message map (base = config.can_base_id, default 0x500):
 *   base+0x000  Pack status     100 ms  Vbat, Vpack, I_batt, BMS state, outputs/flags
 *   base+0x001  Fault status    100 ms  active_faults[31:0], latched_faults[31:0]
 *   base+0x010  Cell frame 0    100 ms  cells  0– 3 (uint16 mV each, 0xFFFF=invalid)
 *     ...
 *   base+0x022  Cell frame 18   100 ms  cells 72–74 + padding
 *   base+0x030  Temp frame 0    500 ms  temps  0– 3 (int16 °C×10 each, 0x8000=invalid)
 *     ...
 *   base+0x042  Temp frame 18   500 ms  temps 72–74 + padding
 */
#pragma once

/* Initialise CAN message state. Call after bms_config_load(). */
void bms_can_init(void);

/* Periodic tick — call every main loop iteration. Transmits frames on schedule. */
void bms_can_tick(void);
