/* test_protocol_crc.c — CRC-16/CCITT-FALSE tests for protocol framing. */
#include "unity.h"
#include "bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_crc16_empty(void) {
    /* CRC of empty input = init value 0xFFFF */
    uint16_t crc = bms_protocol_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, crc);
}

void test_crc16_known_vector_hello(void) {
    /* CRC-16/CCITT-FALSE of "Hello" = 0xDADA (verified against Python reference) */
    const uint8_t hello[] = {'H','e','l','l','o'};
    uint16_t crc = bms_protocol_crc16(hello, 5);
    TEST_ASSERT_EQUAL_HEX16(0xDADAu, crc);
}

void test_crc16_known_vector_123456789(void) {
    /* CRC-16/CCITT-FALSE of "123456789" = 0x29B1 */
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = bms_protocol_crc16(data, 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc);
}

void test_crc16_single_zero(void) {
    uint8_t data = 0x00;
    uint16_t crc = bms_protocol_crc16(&data, 1);
    TEST_ASSERT_NOT_EQUAL(0u, crc); /* non-trivial */
}

void test_crc16_consistency(void) {
    const uint8_t data[] = {0xAA, 0x55, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL(bms_protocol_crc16(data, 8), bms_protocol_crc16(data, 8));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_crc16_known_vector_hello);
    RUN_TEST(test_crc16_known_vector_123456789);
    RUN_TEST(test_crc16_single_zero);
    RUN_TEST(test_crc16_consistency);
    return UNITY_END();
}
