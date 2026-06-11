#include "data_integrity.h"

uint32_t data_integrity_fnv1a_update(uint32_t checksum, const void *data, size_t length) {
    if (data == NULL) {
        return checksum;
    }
    const uint8_t *bytes = data;
    for (size_t i = 0; i < length; ++i) {
        checksum ^= bytes[i];
        checksum *= 16777619U;
    }
    return checksum;
}
