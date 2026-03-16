#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

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
	ADS131M0X_ERROR_ID_MISMATCH,
} ADS131M0XError;

// ── Opcode commands ──────────────────────────────────────────────────────────
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

// ── Command expected responses ────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_RESP_RESET_OK   = 0xFF24U,
	ADS131M0X_RESP_RESET_FAIL = 0x0011U,
	ADS131M0X_RESP_STANDBY    = 0x0022U,
	ADS131M0X_RESP_WAKEUP     = 0x0033U,
	ADS131M0X_RESP_LOCK       = 0x0555U,
	ADS131M0X_RESP_UNLOCK     = 0x0655U,
} ADS131M0XResponse;

// ── Power modes (CLOCK.PWR) ───────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_POWER_VERY_LOW = 0x00U, ///< f_CLKIN = 2.048 MHz nominal
	ADS131M0X_POWER_LOW      = 0x01U, ///< f_CLKIN = 4.096 MHz nominal
	ADS131M0X_POWER_HIGH_RES = 0x02U, ///< f_CLKIN = 8.192 MHz nominal (default)
} ADS131M0XPowerMode;

// ── Oversampling ratio (CLOCK.OSR) ───────────────────────────────────────────
typedef enum
{
	ADS131M0X_OSR_128   = 0x00U,
	ADS131M0X_OSR_256   = 0x01U,
	ADS131M0X_OSR_512   = 0x02U,
	ADS131M0X_OSR_1024  = 0x03U, ///< Default
	ADS131M0X_OSR_2048  = 0x04U,
	ADS131M0X_OSR_4096  = 0x05U,
	ADS131M0X_OSR_8192  = 0x06U,
	ADS131M0X_OSR_16384 = 0x07U,
} ADS131M0XOSR;

// ── SPI word length (MODE.WLENGTH) ───────────────────────────────────────────
typedef enum
{
	ADS131M0X_WLENGTH_16_BIT      = 0x00U,
	ADS131M0X_WLENGTH_24_BIT      = 0x01U, ///< Default
	ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U, ///< 32-bit, zero-padded
	ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U, ///< 32-bit, sign-extended
} ADS131M0XWordLength;

// ── PGA gain (GAIN1/GAIN2.PGAGAINn) ─────────────────────────────────────────
typedef enum
{
	ADS131M0X_GAIN_1   = 0x00U, ///< Default
	ADS131M0X_GAIN_2   = 0x01U,
	ADS131M0X_GAIN_4   = 0x02U,
	ADS131M0X_GAIN_8   = 0x03U,
	ADS131M0X_GAIN_16  = 0x04U,
	ADS131M0X_GAIN_32  = 0x05U,
	ADS131M0X_GAIN_64  = 0x06U,
	ADS131M0X_GAIN_128 = 0x07U,
} ADS131M0XGain;

// ── Channel input MUX (CHn_CFG.MUXn) ────────────────────────────────────────
typedef enum
{
	ADS131M0X_MUX_NORMAL       = 0x00U, ///< Default
	ADS131M0X_MUX_SHORTED      = 0x01U, ///< Inputs shorted to mid-supply (offset cal)
	ADS131M0X_MUX_POSITIVE_DCM = 0x02U, ///< Positive DC test signal from internal reference
	ADS131M0X_MUX_NEGATIVE_DCM = 0x03U, ///< Negative DC test signal from internal reference
} ADS131M0XMux;

// ── Global DC-block high-pass filter coefficient (THRSHLD_LSB.DCBLOCK) ───────
typedef enum
{
	ADS131M0X_DCBLOCK_DISABLED = 0x00U, ///< Default
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
} ADS131M0XDcBlock;

// ── Global-chop delay (CFG.GC_DLY) ──────────────────────────────────────────
typedef enum
{
	ADS131M0X_GC_DLY_2     = 0x00U,
	ADS131M0X_GC_DLY_4     = 0x01U,
	ADS131M0X_GC_DLY_8     = 0x02U,
	ADS131M0X_GC_DLY_16    = 0x03U,
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

// ── CRC polynomial (MODE.CRC_TYPE) ───────────────────────────────────────────
typedef enum
{
	ADS131M0X_CRC_CCITT = 0x00U, ///< x^16 + x^12 + x^5 + 1
	ADS131M0X_CRC_ANSI  = 0x01U, ///< x^16 + x^15 + x^2 + 1 (make default)
} ADS131M0XCRCType;

// ── DRDY pin signal source (MODE.DRDY_SEL) ───────────────────────────────────
typedef enum
{
	ADS131M0X_DRDY_SEL_MOST_LAGGING = 0x00U, ///< Default
	ADS131M0X_DRDY_SEL_ALL_OR       = 0x01U,
	ADS131M0X_DRDY_SEL_MOST_LEADING = 0x02U,
} ADS131M0XDrdySel;

// ── DRDY pin output format (MODE.DRDY_FMT) ───────────────────────────────────
typedef enum
{
	ADS131M0X_DRDY_FMT_LOGIC_LOW = 0x00U, ///< Level low while data ready (default)
	ADS131M0X_DRDY_FMT_PULSE     = 0x01U, ///< Falling-edge pulse at clock boundary
} ADS131M0XDrdyFmt;

// ── Current-detect consecutive over-threshold sample count (CFG.CD_NUM) ──────
typedef enum
{
	ADS131M0X_CD_NUM_1   = 0x00U,
	ADS131M0X_CD_NUM_2   = 0x01U,
	ADS131M0X_CD_NUM_4   = 0x02U,
	ADS131M0X_CD_NUM_8   = 0x03U,
	ADS131M0X_CD_NUM_16  = 0x04U,
	ADS131M0X_CD_NUM_32  = 0x05U,
	ADS131M0X_CD_NUM_64  = 0x06U,
	ADS131M0X_CD_NUM_128 = 0x07U,
} ADS131M0XCdNum;

// ── Current-detect cycle window length (CFG.CD_LEN) ─────────────────────────
typedef enum
{
	ADS131M0X_CD_LEN_1   = 0x00U,
	ADS131M0X_CD_LEN_2   = 0x01U,
	ADS131M0X_CD_LEN_4   = 0x02U,
	ADS131M0X_CD_LEN_8   = 0x03U,
	ADS131M0X_CD_LEN_16  = 0x04U,
	ADS131M0X_CD_LEN_32  = 0x05U,
	ADS131M0X_CD_LEN_64  = 0x06U,
	ADS131M0X_CD_LEN_128 = 0x07U,
} ADS131M0XCdLen;

// ── Global configuration ──────────────────────────────────────────────────────
typedef struct
{
	// CLOCK register
	ADS131M0XPowerMode mode;
	ADS131M0XOSR osr;
	bool turbo_mode_is_enabled;

	// MODE register — SPI framing
	ADS131M0XWordLength word_length;
	bool reg_crc_is_enabled;
	bool rx_crc_is_enabled;
	ADS131M0XCRCType crc_type;
	bool timeout_is_enabled;

	// MODE register — DRDY output pin
	ADS131M0XDrdySel drdy_sel;
	bool drdy_hiz_is_enabled;
	ADS131M0XDrdyFmt drdy_fmt;

	// CFG register — global-chop
	bool global_chop_is_enabled;
	ADS131M0XGlobalChopDelay global_chop_delay;

	// THRSHLD_LSB register — DC-block
	ADS131M0XDcBlock dc_block;

	// CFG + THRSHLD registers — current-detect fault
	struct
	{
		bool is_enabled;
		bool all_channels;
		ADS131M0XCdNum num;
		ADS131M0XCdLen len;
		int32_t threshold;
	} current_detect;

} ADS131M0XGlobalConfig;

// ── Channel configuration ─────────────────────────────────────────────────────
typedef struct
{
	bool is_enabled;
	ADS131M0XGain gain;
	ADS131M0XMux mux;
	int16_t phase_delay_cycles;
	bool dc_block_is_disabled;
	int32_t offset_cal;
	uint32_t gain_cal;
} ADS131M0XChannelConfig;

// ── Hardware abstraction layer ────────────────────────────────────────────────
typedef struct
{
	int  (*spiRead)(void* const data, const uint8_t length_bytes);
	int  (*spiWrite)(const void* const data, const uint8_t length_bytes);
	bool (*drdyGet)(void);
	void (*syncResetSet)(bool state);
	void (*delayMs)(uint32_t delay_ms);
} ADS131M0XHAL;

// ── Device handle ─────────────────────────────────────────────────────────────
typedef struct
{
	ADS131M0XHAL hal;

	bool is_initialized;
	bool is_locked;

	ADS131M0XWordLength word_length;
	uint8_t active_channel_count;

	struct
	{
		bool is_enabled;
		ADS131M0XCRCType type;
	} crc;

} ADS131M0X;

// ── Conversion data ───────────────────────────────────────────────────────────
typedef struct
{
	uint16_t status;
	int32_t  channel_data[ADS131M0X_CHANNEL_COUNT];
	uint16_t crc;
	bool     crc_is_valid;
} ADS131M0XData;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_TYPES_H
