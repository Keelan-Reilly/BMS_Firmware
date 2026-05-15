/* test_flash_layout.c — static and runtime flash map invariant checks. */
#include "unity.h"
#include "bms_constants.h"
#include "bl_config.h"

void setUp(void) {}
void tearDown(void) {}

void test_bootloader_does_not_overlap_app(void) {
    TEST_ASSERT_TRUE(BL_START_ADDR + BL_SIZE_BYTES <= APP_START_ADDR);
}

void test_app_does_not_overlap_config_a(void) {
    TEST_ASSERT_TRUE(APP_START_ADDR + APP_REGION_SIZE <= CONFIG_A_START_ADDR);
}

void test_config_a_does_not_overlap_config_b(void) {
    TEST_ASSERT_TRUE(CONFIG_A_START_ADDR + CONFIG_SLOT_SIZE <= CONFIG_B_START_ADDR);
}

void test_config_b_does_not_exceed_flash(void) {
    TEST_ASSERT_TRUE(CONFIG_B_START_ADDR + CONFIG_SLOT_SIZE <= FLASH_BASE_ADDR + FLASH_SIZE_BYTES);
}

void test_all_regions_page_aligned(void) {
    TEST_ASSERT_EQUAL(0u, BL_START_ADDR    % FLASH_PAGE_SIZE);
    TEST_ASSERT_EQUAL(0u, APP_START_ADDR   % FLASH_PAGE_SIZE);
    TEST_ASSERT_EQUAL(0u, CONFIG_A_START_ADDR % FLASH_PAGE_SIZE);
    TEST_ASSERT_EQUAL(0u, CONFIG_B_START_ADDR % FLASH_PAGE_SIZE);
}

void test_config_slot_fits_bms_config(void) {
    TEST_ASSERT_TRUE(CONFIG_SCHEMA_SIZE <= CONFIG_SLOT_SIZE);
}

void test_total_flash_not_exceeded(void) {
    uint32_t total_used = (CONFIG_B_START_ADDR + CONFIG_SLOT_SIZE) - FLASH_BASE_ADDR;
    TEST_ASSERT_TRUE(total_used <= FLASH_SIZE_BYTES);
}

void test_bl_start_addr_is_flash_base(void) {
    TEST_ASSERT_EQUAL_HEX32(0x08000000u, BL_START_ADDR);
}

void test_app_start_addr_after_bootloader(void) {
    TEST_ASSERT_EQUAL_HEX32(0x08008000u, APP_START_ADDR);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bootloader_does_not_overlap_app);
    RUN_TEST(test_app_does_not_overlap_config_a);
    RUN_TEST(test_config_a_does_not_overlap_config_b);
    RUN_TEST(test_config_b_does_not_exceed_flash);
    RUN_TEST(test_all_regions_page_aligned);
    RUN_TEST(test_config_slot_fits_bms_config);
    RUN_TEST(test_total_flash_not_exceeded);
    RUN_TEST(test_bl_start_addr_is_flash_base);
    RUN_TEST(test_app_start_addr_after_bootloader);
    return UNITY_END();
}
