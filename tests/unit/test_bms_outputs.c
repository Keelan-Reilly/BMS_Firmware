/* test_bms_outputs.c — permission output gating unit tests. */
#include "unity.h"
#include "bms_outputs.h"
#include "board_outputs.h"

void setUp(void) {
    board_outputs_init_safe();
}

void tearDown(void) {}

static BmsPermissionRequest all_wanted(void) {
    return (BmsPermissionRequest){true, true, true, true};
}

void test_no_faults_all_permissions_granted(void) {
    BmsPermissionRequest req = all_wanted();
    bms_outputs_apply(&req, 0u, 0u);
    BmsOutputsBitmask s = bms_outputs_get_state();
    TEST_ASSERT_TRUE(s & OUTPUTS_BIT_MASTER_OK);
    TEST_ASSERT_TRUE(s & OUTPUTS_BIT_DISCHARGE);
    TEST_ASSERT_TRUE(s & OUTPUTS_BIT_CHARGE);
}

void test_fatal_fault_deasserts_all(void) {
    BmsPermissionRequest req = all_wanted();
    bms_outputs_apply(&req, FAULT_MASK(FAULT_BIT_CONFIG_INVALID), 0u);
    TEST_ASSERT_EQUAL(0u, bms_outputs_get_state());
}

void test_discharge_blocking_fault_blocks_discharge_not_charge(void) {
    BmsPermissionRequest req = all_wanted();
    bms_outputs_apply(&req, FAULT_MASK(FAULT_BIT_CELL_UV_HARD), 0u);
    BmsOutputsBitmask s = bms_outputs_get_state();
    TEST_ASSERT_FALSE(s & OUTPUTS_BIT_DISCHARGE);
    /* Charge may still be allowed (UV doesn't block charge in this design) */
}

void test_ov_hard_fault_blocks_charge(void) {
    BmsPermissionRequest req = all_wanted();
    bms_outputs_apply(&req, FAULT_MASK(FAULT_BIT_CELL_OV_HARD), 0u);
    BmsOutputsBitmask s = bms_outputs_get_state();
    TEST_ASSERT_FALSE(s & OUTPUTS_BIT_CHARGE);
}

void test_deassert_all_clears_all_permissions(void) {
    BmsPermissionRequest req = all_wanted();
    bms_outputs_apply(&req, 0u, 0u);
    TEST_ASSERT_NOT_EQUAL(0u, bms_outputs_get_state());
    bms_outputs_deassert_all();
    TEST_ASSERT_EQUAL(0u, bms_outputs_get_state());
}

void test_no_permission_wanted_no_output_set(void) {
    BmsPermissionRequest req = {false, false, false, false};
    bms_outputs_apply(&req, 0u, 0u);
    TEST_ASSERT_EQUAL(0u, bms_outputs_get_state());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_faults_all_permissions_granted);
    RUN_TEST(test_fatal_fault_deasserts_all);
    RUN_TEST(test_discharge_blocking_fault_blocks_discharge_not_charge);
    RUN_TEST(test_ov_hard_fault_blocks_charge);
    RUN_TEST(test_deassert_all_clears_all_permissions);
    RUN_TEST(test_no_permission_wanted_no_output_set);
    return UNITY_END();
}
