/**
 * @file ads131m0x_types.h
 * @brief ADS131M0x type definitions — enums, structs, HAL, and device handle.
 */
#pragma once
#ifndef ADS131M0X_TYPES_H
#define ADS131M0X_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Number of ADC channels — adjust for your device variant (4, 6, or 8). */
#define ADS131M0X_CHANNEL_COUNT 4U

// ── Error codes ──────────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_ERROR_OK             = 0,   ///< Success
	ADS131M0X_ERROR_NULL_PARAM     = -1,  ///< NULL pointer argument
	ADS131M0X_ERROR_NOT_INITIALIZED = -2, ///< Driver not initialized; call ads131m0xInit() first
	ADS131M0X_ERROR_SPI            = -3,  ///< SPI transaction failed or unexpected response
	ADS131M0X_ERROR_INVALID_CHANNEL = -4, ///< Channel index out of range
	ADS131M0X_ERROR_INVALID_PARAM  = -5,  ///< Out-of-range argument
	ADS131M0X_ERROR_CRC            = -6,  ///< CRC mismatch on received frame
	ADS131M0X_ERROR_LOCKED         = -7,  ///< SPI interface is locked; call ads131m0xUnlock() first
	ADS131M0X_ERROR_ID_MISMATCH    = -8,  ///< Chip ID does not match expected pattern
} ADS131M0XError;

// ── Opcode commands ──────────────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_CMD_NULL    = 0x0000U,  ///< No operation — clocks out the next response frame
	ADS131M0X_CMD_RESET   = 0x0011U,  ///< Software reset
	ADS131M0X_CMD_STANDBY = 0x0022U,  ///< Enter standby mode
	ADS131M0X_CMD_WAKEUP  = 0x0033U,  ///< Exit standby mode
	ADS131M0X_CMD_LOCK    = 0x0555U,  ///< Lock SPI (only NULL, RREG, UNLOCK accepted)
	ADS131M0X_CMD_UNLOCK  = 0x0655U,  ///< Unlock SPI
	ADS131M0X_CMD_RREG    = 0xA000U,  ///< Read register(s) — OR in address and count
	ADS131M0X_CMD_WREG    = 0x6000U,  ///< Write register(s) — OR in address and count
} ADS131M0XCommand;

// ── Command expected responses ────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_RESP_RESET_OK   = (0xFF00U | (0x20U | ADS131M0X_CHANNEL_COUNT)),  ///< Power-on / reset acknowledgment
	ADS131M0X_RESP_STANDBY    = 0x0022U,  ///< STANDBY acknowledgment
	ADS131M0X_RESP_WAKEUP     = 0x0033U,  ///< WAKEUP acknowledgment
	ADS131M0X_RESP_LOCK       = 0x0555U,  ///< LOCK acknowledgment
	ADS131M0X_RESP_UNLOCK     = 0x0655U,  ///< UNLOCK acknowledgment
} ADS131M0XResponse;

// ── Power modes (CLOCK.PWR) ───────────────────────────────────────────────────
typedef enum
{
	ADS131M0X_POWER_VERY_LOW = 0x00U,  ///< Very-low-power mode
	ADS131M0X_POWER_LOW      = 0x01U,  ///< Low-power mode
	ADS131M0X_POWER_HIGH_RES = 0x02U,  ///< High-resolution mode
} ADS131M0XPowerMode;

// ── Oversampling ratio (CLOCK.OSR) ───────────────────────────────────────────
typedef enum
{
	ADS131M0X_OSR_128   = 0x00U,
	ADS131M0X_OSR_256   = 0x01U,
	ADS131M0X_OSR_512   = 0x02U,
	ADS131M0X_OSR_1024  = 0x03U,  ///< Default
	ADS131M0X_OSR_2048  = 0x04U,
	ADS131M0X_OSR_4096  = 0x05U,
	ADS131M0X_OSR_8192  = 0x06U,
	ADS131M0X_OSR_16384 = 0x07U,
} ADS131M0XOSR;

// ── SPI word length (MODE.WLENGTH) ───────────────────────────────────────────
typedef enum
{
	ADS131M0X_WLENGTH_16_BIT      = 0x00U,
	ADS131M0X_WLENGTH_24_BIT      = 0x01U,  ///< Default
	ADS131M0X_WLENGTH_32_BIT_ZERO = 0x02U,  ///< 32-bit, zero-padded LSB
	ADS131M0X_WLENGTH_32_BIT_SIGN = 0x03U,  ///< 32-bit, sign-extended LSB
} ADS131M0XWordLength;

// ── PGA gain (GAIN1/GAIN2.PGAGAINn) ─────────────────────────────────────────
typedef enum
{
	ADS131M0X_GAIN_1   = 0x00U,  ///< 1× gain (default)
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
	ADS131M0X_MUX_NORMAL       = 0x00U,  ///< Normal differential input (default)
	ADS131M0X_MUX_SHORTED      = 0x01U,  ///< Inputs shorted (offset calibration)
	ADS131M0X_MUX_POSITIVE_DCM = 0x02U,  ///< Positive DC measurement
	ADS131M0X_MUX_NEGATIVE_DCM = 0x03U,  ///< Negative DC measurement
} ADS131M0XMux;

// ── Global DC-block high-pass filter coefficient (THRSHLD_LSB.DCBLOCK) ───────
typedef enum
{
	ADS131M0X_DCBLOCK_DISABLED = 0x00U,  ///< DC-block filter disabled (default)
	ADS131M0X_DCBLOCK_1_4      = 0x01U,  ///< HPF at f_sampling/4
	ADS131M0X_DCBLOCK_1_8      = 0x02U,  ///< HPF at f_sampling/8
	ADS131M0X_DCBLOCK_1_16     = 0x03U,  ///< HPF at f_sampling/16
	ADS131M0X_DCBLOCK_1_32     = 0x04U,  ///< HPF at f_sampling/32
	ADS131M0X_DCBLOCK_1_64     = 0x05U,  ///< HPF at f_sampling/64
	ADS131M0X_DCBLOCK_1_128    = 0x06U,  ///< HPF at f_sampling/128
	ADS131M0X_DCBLOCK_1_256    = 0x07U,  ///< HPF at f_sampling/256
	ADS131M0X_DCBLOCK_1_512    = 0x08U,  ///< HPF at f_sampling/512
	ADS131M0X_DCBLOCK_1_1024   = 0x09U,  ///< HPF at f_sampling/1024
	ADS131M0X_DCBLOCK_1_2048   = 0x0AU,  ///< HPF at f_sampling/2048
	ADS131M0X_DCBLOCK_1_4096   = 0x0BU,  ///< HPF at f_sampling/4096
	ADS131M0X_DCBLOCK_1_8192   = 0x0CU,  ///< HPF at f_sampling/8192
	ADS131M0X_DCBLOCK_1_16384  = 0x0DU,  ///< HPF at f_sampling/16384
	ADS131M0X_DCBLOCK_1_32768  = 0x0EU,  ///< HPF at f_sampling/32768
	ADS131M0X_DCBLOCK_1_65536  = 0x0FU,  ///< HPF at f_sampling/65536
} ADS131M0XDCBlock;

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
	ADS131M0X_CRC_POLYNOMIAL_CCITT = 0x00U,  ///< x^16 + x^12 + x^5 + 1 = 0x1021
	ADS131M0X_CRC_POLYNOMIAL_ANSI  = 0x01U,  ///< x^16 + x^15 + x^2 + 1 = 0x8005
} ADS131M0XCRCPolynomial;

// ── DRDY pin signal source (MODE.DRDY_SEL) ───────────────────────────────────
typedef enum
{
	ADS131M0X_DRDY_SEL_MOST_LAGGING = 0x00U,  ///< Most-lagging channel drives DRDY (default)
	ADS131M0X_DRDY_SEL_ALL_OR       = 0x01U,  ///< Logic-OR of all channel DRDY signals
	ADS131M0X_DRDY_SEL_MOST_LEADING = 0x02U,  ///< Most-leading channel drives DRDY
} ADS131M0XDataReadySelect;

// ── DRDY pin output format (MODE.DRDY_FMT) ───────────────────────────────────
typedef enum
{
	ADS131M0X_DRDY_FMT_LOGIC_LOW = 0x00U,  ///< Logic-low until frame is read (default)
	ADS131M0X_DRDY_FMT_PULSE     = 0x01U,  ///< Single-clock pulse
} ADS131M0XDataReadyFormat;

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
} ADS131M0XCurrentDetectNum;

// ── Current-detect cycle window length (CFG.CD_LEN) ─────────────────────────
typedef enum
{
	ADS131M0X_CD_LEN_128  = 0x00U,
	ADS131M0X_CD_LEN_256  = 0x01U,
	ADS131M0X_CD_LEN_512  = 0x02U,
	ADS131M0X_CD_LEN_768  = 0x03U,
	ADS131M0X_CD_LEN_1280 = 0x04U,
	ADS131M0X_CD_LEN_1792 = 0x05U,
	ADS131M0X_CD_LEN_2560 = 0x06U,
	ADS131M0X_CD_LEN_3584 = 0x07U,
} ADS131M0XCurrentDetectLen;


// ── Hardware abstraction layer ────────────────────────────────────────────────
typedef struct
{
	int  (*spiRead)(void* const data, const uint8_t length_bytes);         ///< Read @p length_bytes bytes from DOUT; returns 0 on success
	int  (*spiWrite)(const void* const data, const uint8_t length_bytes);  ///< Write @p length_bytes bytes to DIN; returns 0 on success
	bool (*drdyGet)(void);                                                  ///< Return true when DRDY is asserted (pin low)
	void (*syncResetSet)(const bool state);                                 ///< Drive the SYNC/RESET pin; false = assert reset
	void (*delayMs)(const uint32_t delay_ms);                              ///< Block for @p delay_ms milliseconds
} ADS131M0XHAL;

// ── Global configuration ──────────────────────────────────────────────────────
typedef struct
{
	/* CLOCK register */
	ADS131M0XPowerMode power_mode;          ///< Modulator power mode
	ADS131M0XOSR oversampling_ratio;        ///< Oversampling ratio
	bool is_turbo_mode_enabled;             ///< Enable turbo mode (doubles max data rate)

	/* MODE register — SPI framing */
	ADS131M0XWordLength word_length;        ///< SPI word width
	bool is_spi_timeout_enabled;            ///< Enable SPI timeout auto-reset

	struct
	{
		bool is_output_enabled;             ///< Append CRC word to output frames
		bool is_input_enabled;              ///< Validate CRC on incoming DIN frames
		ADS131M0XCRCPolynomial polynomial;  ///< CRC polynomial selection
	} crc;

	/* MODE register — DRDY output pin */
	struct
	{
		ADS131M0XDataReadySelect selection;  ///< Channel that drives DRDY
		bool is_hiz_enabled;                 ///< Hi-Z DRDY when CS is de-asserted
		ADS131M0XDataReadyFormat format;     ///< Logic-low or pulse output format
	} data_ready;

	/* CFG register — global-chop */
	struct
	{
		bool is_enabled;              ///< Enable global-chop mode
		ADS131M0XGlobalChopDelay delay;  ///< Modulator clocks between polarity swaps
	} global_chop;

	/* THRSHLD_LSB register — DC-block */
	ADS131M0XDCBlock dc_block;  ///< Global DC-block HPF corner frequency

	/* CFG + THRSHLD registers — current-detect fault */
	struct
	{
		bool is_enabled;                    ///< Enable current-detect monitoring
		bool is_all_channels_enabled;       ///< Apply threshold across all channels
		ADS131M0XCurrentDetectNum num;      ///< Consecutive over-threshold samples required
		ADS131M0XCurrentDetectLen len;      ///< Detection window length
		uint32_t threshold;                 ///< 24-bit detection threshold value
	} current_detect;

} ADS131M0XConfig;

// ── Channel configuration ─────────────────────────────────────────────────────
typedef struct
{
	bool is_enabled;              ///< Enable this channel's clock in CLOCK register
	ADS131M0XGain gain;           ///< PGA gain setting
	ADS131M0XMux mux;             ///< Input multiplexer selection
	int16_t phase_delay_cycles;   ///< Two's-complement phase delay in modulator clock cycles
	bool is_dc_block_disabled;    ///< Disable the per-channel DC-block filter override
	int32_t offset_cal;           ///< 24-bit offset calibration word
	uint32_t gain_cal;            ///< 24-bit gain calibration word
} ADS131M0XChannelConfig;

// ── Device handle ─────────────────────────────────────────────────────────────
typedef struct
{
	ADS131M0XHAL hal;  ///< Copy of the HAL function pointers provided at init

	bool is_initialized;  ///< True after a successful ads131m0xInit()
	bool is_locked;       ///< True when the SPI interface is locked

	ADS131M0XWordLength word_length;  ///< Active SPI word length

	struct
	{
		bool is_enabled;            ///< True when CRC output is active
		ADS131M0XCRCPolynomial type;  ///< Active CRC polynomial
	} crc;

} ADS131M0X;

// ── Conversion data ───────────────────────────────────────────────────────────
typedef struct
{
	uint16_t status;                              ///< STATUS word from the response frame
	int32_t  channel_data[ADS131M0X_CHANNEL_COUNT];  ///< Sign-extended 24-bit channel codes
	uint16_t crc;                                 ///< CRC word received from the device
	bool     is_crc_valid;                        ///< True if CRC check passed (or CRC disabled)
} ADS131M0XData;

#ifdef __cplusplus
}
#endif

#endif /* ADS131M0X_TYPES_H */
