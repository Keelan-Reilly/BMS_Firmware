/* test_config_masks.c — 75-bit mask validation and bit indexing. */
#include "unity.h"
#include "bms_config.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static BmsConfig make_valid(void) {
    BmsConfig cfg; bms_config_load_defaults(&cfg); return cfg;
}

void test_all_75_bits_set_default_passes(void) {
    BmsConfig cfg = make_valid();
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_OK, bms_config_validate(&cfg, &err));
    /* byte[9] should be 0x07 (cells 72,73,74 = bits 0,1,2 set; bits 3-7 clear) */
    TEST_ASSERT_EQUAL_HEX8(0x07u, cfg.required_cell_mask[9]);
}

void test_bit75_reserved_must_be_clear(void) {
    BmsConfig cfg = make_valid();
    cfg.required_cell_mask[9] |= (1u << 3); /* bit 75 */
    cfg.config_crc32 = 0; cfg.config_crc32 = bms_config_compute_crc(&cfg);
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_ERR_CONFIG_INVALID, bms_config_validate(&cfg, &err));
}

void test_bit79_reserved_must_be_clear(void) {
    BmsConfig cfg = make_valid();
    cfg.balance_allowed_mask[9] |= (1u << 7); /* bit 79 */
    cfg.config_crc32 = 0; cfg.config_crc32 = bms_config_compute_crc(&cfg);
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_ERR_CONFIG_INVALID, bms_config_validate(&cfg, &err));
}

void test_all_zeros_mask_passes(void) {
    /* Zero mask is valid (excludes all cells from gating — allowed) */
    BmsConfig cfg = make_valid();
    memset(cfg.required_cell_mask, 0, 10);
    cfg.config_crc32 = 0; cfg.config_crc32 = bms_config_compute_crc(&cfg);
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_OK, bms_config_validate(&cfg, &err));
}

void test_bit_order_byte0_is_cells_0_to_7(void) {
    /* Bit N of the mask corresponds to cell N (0-indexed) */
    BmsConfig cfg = make_valid();
    memset(cfg.required_cell_mask, 0, 10);
    cfg.required_cell_mask[0] = 0x01u; /* only cell 0 */
    cfg.config_crc32 = 0; cfg.config_crc32 = bms_config_compute_crc(&cfg);
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_OK, bms_config_validate(&cfg, &err));
}

void test_mask_byte9_bit2_is_cell74(void) {
    /* Cell 74 = byte 9, bit 2 (74 / 8 = 9 remainder 2) */
    BmsConfig cfg = make_valid();
    memset(cfg.required_cell_mask, 0, 10);
    cfg.required_cell_mask[9] = (1u << 2u); /* cell 74 only */
    cfg.config_crc32 = 0; cfg.config_crc32 = bms_config_compute_crc(&cfg);
    uint16_t err;
    TEST_ASSERT_EQUAL(BMS_OK, bms_config_validate(&cfg, &err));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_all_75_bits_set_default_passes);
    RUN_TEST(test_bit75_reserved_must_be_clear);
    RUN_TEST(test_bit79_reserved_must_be_clear);
    RUN_TEST(test_all_zeros_mask_passes);
    RUN_TEST(test_bit_order_byte0_is_cells_0_to_7);
    RUN_TEST(test_mask_byte9_bit2_is_cell74);
    return UNITY_END();
}
