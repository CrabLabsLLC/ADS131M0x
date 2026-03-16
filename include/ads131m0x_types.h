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

// ── SPI Opcodes ──────────────────────────────────────────────────────────────
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

// ── Command Expected Responses ────────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_RESP_RESET_OK   = 0xFF24U,
    ADS131M0X_RESP_RESET_FAIL = 0x0011U,
    ADS131M0X_RESP_STANDBY    = 0x0022U,
    ADS131M0X_RESP_WAKEUP     = 0x0033U,
    ADS131M0X_RESP_LOCK       = 0x0555U,
    ADS131M0X_RESP_UNLOCK     = 0x0655U,
} ADS131M0XResponse;

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

// ── SPI Word length ──────────────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_WLENGTH_16_BIT = 0x00U,
    ADS131M0X_WLENGTH_24_BIT = 0x01U,  ///< Default
    ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U,
    ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U,
} ADS131M0XWordLength;

// ── PGA Gain per channel ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_GAIN_1 = 0x00U, // Default
	ADS131M0X_GAIN_2 = 0x01U,
	ADS131M0X_GAIN_4 = 0x02U,
	ADS131M0X_GAIN_8 = 0x03U,
	ADS131M0X_GAIN_16 = 0x04U,
	ADS131M0X_GAIN_32 = 0x05U,
	ADS131M0X_GAIN_64 = 0x06U,
	ADS131M0X_GAIN_128 = 0x07U
} ADS131M0XGain;

// ── Channel input MUX ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_MUX_NORMAL = 0x00U, // Default
	ADS131M0X_MUX_SHORTED = 0x01U,
	ADS131M0X_MUX_POSITIVE_DCM = 0x02U,
	ADS131M0X_MUX_NEGATIVE_DCM = 0x03U
} ADS131M0XMux;

// ── Global DC-block high-pass filter coefficient ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_DCBLOCK_DISABLED = 0x00U,
	ADS131M0X_DCBLOCK_1_4 = 0x01U,
	ADS131M0X_DCBLOCK_1_8 = 0x02U,
	ADS131M0X_DCBLOCK_1_16 = 0x03U,
	ADS131M0X_DCBLOCK_1_32 = 0x04U,
	ADS131M0X_DCBLOCK_1_64 = 0x05U,
	ADS131M0X_DCBLOCK_1_128 = 0x06U,
	ADS131M0X_DCBLOCK_1_256 = 0x07U,
	ADS131M0X_DCBLOCK_1_512 = 0x08U,
	ADS131M0X_DCBLOCK_1_1024 = 0x09U,
	ADS131M0X_DCBLOCK_1_2048 = 0x0AU,
	ADS131M0X_DCBLOCK_1_4096 = 0x0BU,
	ADS131M0X_DCBLOCK_1_8192 = 0x0CU,
	ADS131M0X_DCBLOCK_1_16384 = 0x0DU,
	ADS131M0X_DCBLOCK_1_32768 = 0x0EU,
	ADS131M0X_DCBLOCK_1_65536 = 0x0FU
} ADS131M0XDcBlock;

// ── Global-chop Delay ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_GC_DLY_2 = 0x00U,
	ADS131M0X_GC_DLY_4 = 0x01U,
	ADS131M0X_GC_DLY_8 = 0x02U,
	ADS131M0X_GC_DLY_16 = 0x03U,
	ADS131M0X_GC_DLY_32 = 0x04U,
	ADS131M0X_GC_DLY_64 = 0x05U,
	ADS131M0X_GC_DLY_128 = 0x06U,
	ADS131M0X_GC_DLY_256 = 0x07U,
	ADS131M0X_GC_DLY_512 = 0x08U,
	ADS131M0X_GC_DLY_1024 = 0x09U,
	ADS131M0X_GC_DLY_2048 = 0x0AU,
	ADS131M0X_GC_DLY_4096 = 0x0BU,
	ADS131M0X_GC_DLY_8192 = 0x0CU,
	ADS131M0X_GC_DLY_16384 = 0x0DU,
	ADS131M0X_GC_DLY_32768 = 0x0EU,
	ADS131M0X_GC_DLY_65536 = 0x0FU
} ADS131M0XGlobalChopDelay;

// ── CRC type ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_CRC_CCITT = 0x00U,
	ADS131M0X_CRC_ANSI = 0x01U // Default
} ADS131M0XCRCType;

// ── Global device configuration ──────────────────────────────────────────────────────────────
typedef struct
{
	ADS131M0XPowerMode powerMode;
	ADS131M0XOSR osr;
	ADS131M0XWordLength wordLength;
	ADS131M0XCRCType crcType; // Default: ADS131M0X_CRC_ANSI
	bool globalChopEnable; // Default: false
	ADS131M0XGlobalChopDelay globalChopDelay;
	ADS131M0XDcBlock dcBlock; // Default: ADS131M0X_DCBLOCK_DISABLED
} ADS131M0XGlobalConfig;

typedef struct
{
    int  (*spiRead)(void* const data, const uint8_t length);
    int  (*spiWrite)(const void* const data, const uint8_t length);
    bool (*drdyGet)(void);
    void (*delayMs)(const uint32_t delayMs);
} ADS131M0XHAL;

// ── Device handle ──────────────────────────────────────────────────────────────
typedef struct
{
	ADS131M0XHAL hal;
	bool isInitialized;
	uint16_t registerMap[64];
	ADS131M0XWordLength currentWordLength;
    bool isLocked; // Guards writeRegisters()

    struct
    {
        bool isEnabled;
        ADS131M0XCRCType currentType;
    } crc;

} ADS131M0X;

// ── Data struct ──────────────────────────────────────────────────────────────
typedef struct
{
	uint16_t status; // STATUS register
	int32_t channelData[ADS131M0X_CHANNEL_COUNT];
	uint16_t crc;
	bool crcValid;
} ADS131M0XData;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
