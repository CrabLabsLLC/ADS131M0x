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

// Datasheet Section 8.6.3 (Table 8-16)
typedef enum
{
    ADS131M0X_WLENGTH_16_BIT      = 0x00U,
    ADS131M0X_WLENGTH_24_BIT      = 0x01U,  ///< Default
    ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U,
    ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U,
} ADS131M0XWordLength;

// Datasheet Section 8.5.1.7-8: Frame geometry
#define ADS131M0X_NUM_CHANNELS      4U
#define ADS131M0X_FRAME_WORDS       6U
#define ADS131M0X_WORD_SIZE_24BIT   3U
#define ADS131M0X_FRAME_SIZE_24BIT  (ADS131M0X_WORD_SIZE_24BIT * ADS131M0X_FRAME_WORDS)

// ── Error codes ──────────────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_ERROR_OK,
    ADS131M0X_ERROR_GENERAL,
    ADS131M0X_ERROR_INVALID_PARAM,
    ADS131M0X_ERROR_COMM_FAIL,
    ADS131M0X_ERROR_BAD_ID,
    ADS131M0X_ERROR_TIMEOUT,
    ADS131M0X_ERROR_NOT_INIT,
    ADS131M0X_ERROR_CRC,
} ADS131M0XError;

// Datasheet Section 8.5.1.10 (Table 8-11)
typedef enum
{
    ADS131M0X_CMD_NULL    = 0x0000U,
    ADS131M0X_CMD_RESET   = 0x0011U,
    ADS131M0X_CMD_STANDBY = 0x0022U,
    ADS131M0X_CMD_WAKEUP  = 0x0033U,
    ADS131M0X_CMD_LOCK    = 0x0555U,
    ADS131M0X_CMD_UNLOCK  = 0x0655U,
    ADS131M0X_CMD_RREG    = 0xA000U,
    ADS131M0X_CMD_WREG    = 0x6000U,
} ADS131M0XCommand;

// ── Command Responses ────────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_RESP_RESET_OK   = 0xFF24U,
    ADS131M0X_RESP_RESET_FAIL = 0x0011U,
    ADS131M0X_RESP_STANDBY    = 0x0022U,
    ADS131M0X_RESP_WAKEUP     = 0x0033U,
    ADS131M0X_RESP_LOCK       = 0x0555U,
    ADS131M0X_RESP_UNLOCK     = 0x0655U,
} ADS131M0XResponse;

// ── Device handle ────────────────────────────────────────────────────────────
typedef struct
{
    ADS131M0XHAL       hal;
    bool               is_initialized;
    ADS131M0XWordLength word_length;
    bool               is_input_crc_enabled;
} ADS131M0X;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
