/* bms_outputs.h — permission output gating layer.
 *
 * This module is the ONLY caller of board_outputs_set_*() for permission outputs.
 * It gates permission requests through the active fault bitmap before driving GPIOs.
 * No other BMS module may include board_outputs.h or call board_output functions.
 */
#pragma once
#include "bms_types.h"

/* Apply a permission request, filtered through active/latched fault bitmaps.
 * A permission is only set if: the request is true AND no blocking fault is active.
 * Any FATAL fault causes deassert_all and must trigger IWDG or halt upstream. */
void bms_outputs_apply(const BmsPermissionRequest *req,
                        uint64_t active_faults,
                        uint64_t latched_faults);

/* Emergency deassert all permission outputs (calls board_outputs_disable_all). */
void bms_outputs_deassert_all(void);

/* Get current logical output state bitmask (for protocol reporting). */
BmsOutputsBitmask bms_outputs_get_state(void);
