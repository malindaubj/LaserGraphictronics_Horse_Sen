#include "data_integrity.h"
#include "unity.h"

TEST_CASE("FNV-1a checksum matches known vector", "[data_integrity]") {
    static const char input[] = "DigiEquine";
    uint32_t checksum =
        data_integrity_fnv1a_update(DATA_INTEGRITY_FNV1A_INITIAL, input, sizeof(input) - 1U);
    TEST_ASSERT_EQUAL_HEX32(0x073DB0BBU, checksum);
}

TEST_CASE("null data leaves checksum unchanged", "[data_integrity]") {
    TEST_ASSERT_EQUAL_HEX32(0x12345678U, data_integrity_fnv1a_update(0x12345678U, NULL, 10U));
}
