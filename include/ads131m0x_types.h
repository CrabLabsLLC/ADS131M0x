#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ads131m0x_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ── Error codes ──────────────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_ERROR_OK            =  0,   ///< Success
    ADS131M0X_ERROR_GENERAL       = -1,
    ADS131M0X_ERROR_INVALID_PARAM = -2,
    ADS131M0X_ERROR_COMM_FAIL     = -3,
    ADS131M0X_ERROR_BAD_ID        = -4,
    ADS131M0X_ERROR_TIMEOUT       = -5,
    ADS131M0X_ERROR_NOT_INIT      = -6,
    ADS131M0X_ERROR_CRC           = -7,
} ADS131M0XError;

// ── Config structure ──────────────────────────────────────────────────
typedef struct
{
    bool is_input_crc_enabled;       ///< True if RX_CRC_EN is set
    ADS131M0XWordLength word_length; ///< Required to frame SPI transactions correctly
} ADS131M0XConfig;

// ── Device handle ──────────────────────────────────────────────────
typedef struct
{
    ADS131M0XHAL    hal;
	bool            is_initialized;
	ADS131M0XWordLength word_length;
	bool            is_input_crc_enabled;
} ADS131M0XDevice;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
