#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

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
} ADS131M0XError;

typedef struct
{
    uint8_t (*spi_read)(void* buffer, const uint8_t length);
    uint8_t (*spi_write)(const void* const buffer, const uint8_t length);
} ADS131M0XHAL;

typedef struct
{
    bool crc_enabled;
} ADS131M0XConfig;

typedef struct
{
    ADS131M0XHAL    hal;            ///< Copy of the HAL function pointers
    ADS131M0XConfig config;         ///< Copy of the device configuration
    bool            is_initialized; ///< True after successful ads131m0xInit()
} ADS131M0X;

#endif // ADS131M0X_TYPES_H