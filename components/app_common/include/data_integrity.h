#pragma once

#include <stddef.h>
#include <stdint.h>

#define DATA_INTEGRITY_FNV1A_INITIAL 2166136261U

uint32_t data_integrity_fnv1a_update(uint32_t checksum, const void *data, size_t length);
