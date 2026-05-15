/* bms_main_loop.h — main loop orchestration. */
#pragma once

/* Initialise all BMS subsystems. Called once from main() after hardware init. */
void bms_main_loop_init(void);

/* Run forever. Call from main() after bms_main_loop_init(). */
void bms_main_loop_run(void);
