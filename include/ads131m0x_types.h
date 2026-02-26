#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    // Full-duplex SPI: transmit tx_buf, receive into rx_buf, always 18 bytes
    int (*spi_transfer)(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len_bytes);
    void (*delay_ms)(uint32_t ms); ///< Blocking millisecond delay
} ADS131M0XHAL;

typedef struct 
{
    bool crc_enabled;
} ADS131M0XConfig;

typedef struct 
{
    ADS131M0XHAL hal;       ///< Copy of the HAL function pointers
    ADS131M0XConfig config; ///< Copy of the device configuration
    bool is_initialized;    ///< True after successful ads131m0xInit()
} ADS131M0X;


#endif // ADS131M0X_TYPES_H

#ifdef __cplusplus
}
#endif