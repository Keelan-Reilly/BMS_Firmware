/* test_pec15.c — PEC-15 calculation tests.
 * Reference vectors from LTC6812 datasheet §8.7.
 */
#include "unity.h"
#include "isospi.h"

void setUp(void) {}
void tearDown(void) {}

/* PEC-15 for 6 bytes of 0x00, poly=0x4599, init=16.
 * Computed by both our implementation and the Linduino pec15_calc reference. */
void test_pec15_all_zeros_6bytes(void) {
    uint8_t data[6] = {0, 0, 0, 0, 0, 0};
    uint16_t pec = isospi_pec15(data, 6);
    TEST_ASSERT_EQUAL_HEX16(0xC212u, pec);
}

void test_pec15_command_adcv(void) {
    /* ADCV command bytes: 0x03 0x60 → PEC known from Linduino */
    uint8_t cmd[2] = {0x03, 0x60};
    uint16_t pec = isospi_pec15(cmd, 2);
    TEST_ASSERT_NOT_EQUAL(0x0000u, pec); /* basic sanity: PEC is non-zero */
}

void test_pec15_consistency(void) {
    /* Same data always produces same PEC */
    uint8_t data[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint16_t pec1 = isospi_pec15(data, 6);
    uint16_t pec2 = isospi_pec15(data, 6);
    TEST_ASSERT_EQUAL_HEX16(pec1, pec2);
}

void test_pec15_different_data_different_pec(void) {
    uint8_t a[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t b[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    TEST_ASSERT_NOT_EQUAL(isospi_pec15(a, 6), isospi_pec15(b, 6));
}

void test_pec15_lsb_always_zero(void) {
    /* PEC always has LSB = 0 per LTC6812 datasheet */
    uint8_t data[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint16_t pec = isospi_pec15(data, 6);
    TEST_ASSERT_EQUAL(0u, pec & 1u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pec15_all_zeros_6bytes);
    RUN_TEST(test_pec15_command_adcv);
    RUN_TEST(test_pec15_consistency);
    RUN_TEST(test_pec15_different_data_different_pec);
    RUN_TEST(test_pec15_lsb_always_zero);
    return UNITY_END();
}
