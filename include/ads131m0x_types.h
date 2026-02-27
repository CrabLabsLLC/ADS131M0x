#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    ADS131M0X_ERROR_OK = 0,          ///< Success
    ADS131M0X_ERROR_INVALID_ARG,     ///< NULL or bad parameter
    ADS131M0X_ERROR_SPI,             ///< SPI transfer failed
    ADS131M0X_ERROR_TIMEOUT,         ///< Operation timed out
    ADS131M0X_ERROR_NOT_INITIALIZED, ///< ads131m0xInit() not called
} Ads131m0xError;

typedef struct
{
    uint8_t (*spi_read)(void* buffer, const uint8_t length_bytes);
    uint8_t (*spi_write)(const void* const buffer, const uint8_t length_bytes);
} Ads131m0xHAL;

typedef struct
{
    bool is_crc_enabled;
} Ads131m0xConfig;

typedef struct
{
    Ads131m0xHAL    hal;            ///< Copy of the HAL function pointers
    Ads131m0xConfig config;         ///< Copy of the device configuration
    bool            is_initialized; ///< True after successful ads131m0xInit()
} Ads131m0x;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
