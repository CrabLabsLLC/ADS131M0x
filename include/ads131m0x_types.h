#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ── Register fields with named options ──────────────────────────────────────────────────────

// Datasheet Section: 8.6.3 (Table 8-16)
// Description: Data word length selection
typedef enum
{
    ADS131M0X_WLENGTH_16_BIT      = 0x00U,
    ADS131M0X_WLENGTH_24_BIT      = 0x01U,  ///< Default
    ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U,
    ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U,
} ADS131M0XWordLength;

// Datasheet Section: 8.6.3 (Table 8-16)
// Description: DRDY pin signal source selection
typedef enum
{
    ADS131M0X_DRDY_SEL_MOST_LAGGING = 0x00U,  ///< Default
    ADS131M0X_DRDY_SEL_LOGIC_OR     = 0x01U,
    ADS131M0X_DRDY_SEL_MOST_LEADING = 0x02U,
} ADS131M0XDrdySel;

// Datasheet Section: 8.6.4 (Table 8-17)
// Description: Modulator oversampling ratio selection
typedef enum
{
    ADS131M0X_OSR_128   = 0x00U,
    ADS131M0X_OSR_256   = 0x01U,
    ADS131M0X_OSR_512   = 0x02U,
    ADS131M0X_OSR_1024  = 0x03U,  ///< Default
    ADS131M0X_OSR_2048  = 0x04U,
    ADS131M0X_OSR_4096  = 0x05U,
    ADS131M0X_OSR_8192  = 0x06U,
    ADS131M0X_OSR_16256 = 0x07U,
} ADS131M0XOsr;

// Datasheet Section: 8.6.4 (Table 8-17)
// Description: Power mode selection
typedef enum
{
    ADS131M0X_PWR_VLP = 0x00U,  ///< Very-low-power
    ADS131M0X_PWR_LP  = 0x01U,  ///< Low-power
    ADS131M0X_PWR_HR  = 0x02U,  ///< High-resolution (default)
} ADS131M0XPowerMode;

// Datasheet Section: 8.6.5 (Table 8-18)
// Description: PGA gain selection
typedef enum
{
    ADS131M0X_GAIN_1X   = 0x00U,  ///< Default
    ADS131M0X_GAIN_2X   = 0x01U,
    ADS131M0X_GAIN_4X   = 0x02U,
    ADS131M0X_GAIN_8X   = 0x03U,
    ADS131M0X_GAIN_16X  = 0x04U,
    ADS131M0X_GAIN_32X  = 0x05U,
    ADS131M0X_GAIN_64X  = 0x06U,
    ADS131M0X_GAIN_128X = 0x07U,
} ADS131M0XGain;

// Datasheet Section: 8.6.7 (Table 8-20)
// Description: Global Chop delay selection
// Delay in modulator clock periods before measurement begins
typedef enum
{
    ADS131M0X_GC_DLY_2     = 0x00U,
    ADS131M0X_GC_DLY_4     = 0x01U,
    ADS131M0X_GC_DLY_8     = 0x02U,
    ADS131M0X_GC_DLY_16    = 0x03U,  ///< Default
    ADS131M0X_GC_DLY_32    = 0x04U,
    ADS131M0X_GC_DLY_64    = 0x05U,
    ADS131M0X_GC_DLY_128   = 0x06U,
    ADS131M0X_GC_DLY_256   = 0x07U,
    ADS131M0X_GC_DLY_512   = 0x08U,
    ADS131M0X_GC_DLY_1024  = 0x09U,
    ADS131M0X_GC_DLY_2048  = 0x0AU,
    ADS131M0X_GC_DLY_4096  = 0x0BU,
    ADS131M0X_GC_DLY_8192  = 0x0CU,
    ADS131M0X_GC_DLY_16384 = 0x0DU,
    ADS131M0X_GC_DLY_32768 = 0x0EU,
    ADS131M0X_GC_DLY_65536 = 0x0FU,
} ADS131M0XGlobalChopDelay;

// Datasheet Section: 8.6.7 (Table 8-20)
// Description: Current-detect measurement length in conversion periods
typedef enum
{
    ADS131M0X_CD_NUM_1   = 0x00U,  ///< Default
    ADS131M0X_CD_NUM_2   = 0x01U,
    ADS131M0X_CD_NUM_4   = 0x02U,
    ADS131M0X_CD_NUM_8   = 0x03U,
    ADS131M0X_CD_NUM_16  = 0x04U,
    ADS131M0X_CD_NUM_32  = 0x05U,
    ADS131M0X_CD_NUM_64  = 0x06U,
    ADS131M0X_CD_NUM_128 = 0x07U,
} ADS131M0XCurrentDetectNum;

//Description: No. current-detect exceeded thresholds to trigger a detection
typedef enum
{
    ADS131M0X_CD_LEN_128  = 0x00U,  ///< Default
    ADS131M0X_CD_LEN_256  = 0x01U,
    ADS131M0X_CD_LEN_512  = 0x02U,
    ADS131M0X_CD_LEN_768  = 0x03U,
    ADS131M0X_CD_LEN_1280 = 0x04U,
    ADS131M0X_CD_LEN_1792 = 0x05U,
    ADS131M0X_CD_LEN_2560 = 0x06U,
    ADS131M0X_CD_LEN_3584 = 0x07U,
} ADS131M0XCurrentDetectLength;

// Datasheet Section: 8.6.9 (Table 8-22)
// Description: DC block filter selection
// Value of coefficient a
typedef enum
{
    ADS131M0X_DCBLOCK_DISABLED = 0x00U,  ///< Default
    ADS131M0X_DCBLOCK_1_4      = 0x01U,
    ADS131M0X_DCBLOCK_1_8      = 0x02U,
    ADS131M0X_DCBLOCK_1_16     = 0x03U,
    ADS131M0X_DCBLOCK_1_32     = 0x04U,
    ADS131M0X_DCBLOCK_1_64     = 0x05U,
    ADS131M0X_DCBLOCK_1_128    = 0x06U,
    ADS131M0X_DCBLOCK_1_256    = 0x07U,
    ADS131M0X_DCBLOCK_1_512    = 0x08U,
    ADS131M0X_DCBLOCK_1_1024   = 0x09U,
    ADS131M0X_DCBLOCK_1_2048   = 0x0AU,
    ADS131M0X_DCBLOCK_1_4096   = 0x0BU,
    ADS131M0X_DCBLOCK_1_8192   = 0x0CU,
    ADS131M0X_DCBLOCK_1_16384  = 0x0DU,
    ADS131M0X_DCBLOCK_1_32768  = 0x0EU,
    ADS131M0X_DCBLOCK_1_65536  = 0x0FU,
} ADS131M0XDcBlockFilter;

// Datasheet Section: 8.6.25 (Table 8-23)
// Description: MUX input selection
typedef enum
{
    ADS131M0X_MUX_ANALOG_IN = 0x00U,  ///< AINnP and AINnN (default)
    ADS131M0X_MUX_SHORTED   = 0x01U,
    ADS131M0X_MUX_TEST_POS  = 0x02U,
    ADS131M0X_MUX_TEST_NEG  = 0x03U,
} ADS131M0XMux;

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
