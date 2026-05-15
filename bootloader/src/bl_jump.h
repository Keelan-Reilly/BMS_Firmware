/* bl_jump.h — safe jump to application. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Verify that sp and reset_vector are plausible for the application region. */
bool bl_is_valid_sp(uint32_t sp);
bool bl_is_valid_reset_vector(uint32_t rv);

/* Jump to application at app_addr. Never returns.
 * Caller must have: disabled all IRQs, deasserted all outputs. */
void bl_jump_to_app(uint32_t app_addr) __attribute__((noreturn));
