#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ADS131M0X_WORD_SIZE_BYTES  3U
#define ADS131M0X_MAX_CHANNELS    8U

typedef enum
{
    ADS131M0X_ERROR_OK = 0,
    ADS131M0X_ERROR_INVALID_ARG,
    ADS131M0X_ERROR_SPI,
    ADS131M0X_ERROR_TIMEOUT,
    ADS131M0X_ERROR_NOT_INITIALIZED,
} Ads131m0xError;

typedef struct
{
    uint8_t (*spi_read)(void* buffer, const uint8_t length_bytes);
    uint8_t (*spi_write)(const void* const buffer, const uint8_t length_bytes);
    void (*cs_set)(const uint8_t level);   ///< 0 = assert (low), 1 = deassert (high)
    void (*delayMs)(const uint32_t delay_ms);
} Ads131m0xHAL;

typedef struct
{
    bool is_crc_enabled;
} Ads131m0xConfig;

typedef struct
{
    Ads131m0xHAL    hal;
    Ads131m0xConfig config;
    uint8_t         channel_count;
    bool            is_initialized;
} Ads131m0x;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
