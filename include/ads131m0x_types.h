#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ads131m0x_hal.h"

#include <stdbool.h>
#include <stdint.h>

// ── Error codes ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_ERROR_OK,
	ADS131M0X_ERROR_NULL_PARAM,
	ADS131M0X_ERROR_NOT_INITIALIZED,
	ADS131M0X_ERROR_SPI,
	ADS131M0X_ERROR_INVALID_CHANNEL,
	ADS131M0X_ERROR_INVALID_PARAM,
	ADS131M0X_ERROR_CRC,
	ADS131M0X_ERROR_LOCKED,
	ADS131M0X_ERROR_ID_MISMATCH
} ADS131M0XError;

// ── Power modes ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_POWER_VERY_LOW = 0x00U,
	ADS131M0X_POWER_LOW      = 0x01U,
	ADS131M0X_POWER_HIGH_RES = 0x02U // Default
} ADS131M0XPowerMode;

// ── Oversampling ratio ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_OSR_128 = 0x00U,
	ADS131M0X_OSR_256 = 0x01U,
	ADS131M0X_OSR_512 = 0x02U,
	ADS131M0X_OSR_1024 = 0x03U, // Default
	ADS131M0X_OSR_2048 = 0x04U,
	ADS131M0X_OSR_4096 = 0x05U,
	ADS131M0X_OSR_8192 = 0x06U,
	ADS131M0X_OSR_16384 = 0x07U
} ADS131M0XOSR;

// ── SPI Word size ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_WORD_16BIT = 0x00U,
	ADS131M0X_WORD_24BIT = 0x01U, // Default
	ADS131M0X_WORD_32BIT_ZERO_PAD = 0x02U,
	ADS131M0X_WORD_32BIT_SIGN_EXT = 0x03U
} ADS131M0XWordSize;

#define CHANNEL_COUNT (4)   // ADS131M04 -> 4 Channels

#if ((CHANNEL_COUNT < 1) || (CHANNEL_COUNT > 8))
    #error Invalid channel count configured in 'ads131m0x.h'.
#endif

// ── SPI Opcodes ──────────────────────────────────────────────────────────────
#define ADS131M0X_OPCODE_NULL           ((uint16_t) 0x0000)
#define ADS131M0X_OPCODE_RESET          ((uint16_t) 0x0011)
#define ADS131M0X_OPCODE_STANDBY        ((uint16_t) 0x0022)
#define ADS131M0X_OPCODE_WAKEUP         ((uint16_t) 0x0033)
#define ADS131M0X_OPCODE_LOCK           ((uint16_t) 0x0555)
#define ADS131M0X_OPCODE_UNLOCK         ((uint16_t) 0x0655)
#define ADS131M0X_OPCODE_RREG           ((uint16_t) 0xA000)
#define ADS131M0X_OPCODE_WREG           ((uint16_t) 0x6000)

// Build RREG/WREG opcodes with address: opcode | (addr << 7)
#define ADS131M0X_OPCODE_RREG_ADDR(a)   ((uint16_t)(ADS131M0X_OPCODE_RREG | ((uint16_t)(a) << 7)))
#define ADS131M0X_OPCODE_WREG_ADDR(a)   ((uint16_t)(ADS131M0X_OPCODE_WREG | ((uint16_t)(a) << 7)))

typedef enum
{
    ADS131M0X_WLENGTH_16_BIT      = 0x00U,
    ADS131M0X_WLENGTH_24_BIT      = 0x01U,  ///< Default
    ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U,
    ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U,
} ADS131M0XWordLength;



/* Enable this define statement to use CRC on DIN... */
//#define ENABLE_CRC_IN

/* Select CRC type */
#define CRC_CCITT
//#define CRC_ANSI

/* Disable assertions when not in the CCS "Debug" configuration */
#ifndef _DEBUG
    #define NDEBUG
#endif

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
