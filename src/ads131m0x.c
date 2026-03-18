#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "ads131m0x.h"

/* Maximum bytes per SPI frame: (channel count + status word + CRC word) * maximum bytes per word (32-bit -> 4 bytes) */
#define ADS131M0X_FRAME_SIZE_MAX_BYTES ((ADS131M0X_CHANNEL_COUNT + 2U) * 4U)
#define ADS131M0X_CRC_POLYNOMIAL_HEX_CCITT 0x1021U
#define ADS131M0X_CRC_POLYNOMIAL_HEX_ANSI  0x8005U

// ── Forward Declarations ──────────────────────────────────────────────────────
static uint8_t bytesPerWord(ADS131M0XWordLength word_length);
static void packWord(uint8_t* buf, uint16_t word, ADS131M0XWordLength word_length);
static int32_t signExtend(const uint8_t* bytes, ADS131M0XWordLength word_length);
static uint16_t calculateCRC(uint16_t crc_polynomial, const uint8_t* data, uint8_t length_bytes, uint16_t initialValue);
static ADS131M0XError sendCommand(const ADS131M0X* const dev, ADS131M0XCommand command, uint16_t* const response);
static ADS131M0XError checkCRC(const ADS131M0X* const dev, const uint8_t* buf);

// ── Public Functions ────────────────────────────────────────────────────────
ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal)
{
	if (dev == NULL || hal == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (hal->spiRead == NULL || hal->spiWrite == NULL || hal->drdyGet == NULL || hal->syncResetSet == NULL || hal->delayMs == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

    dev->hal = *hal;
    dev->is_initialized = false;
    dev->is_locked = false;
    dev->word_length = ADS131M0X_WLENGTH_24_BIT;  // power-on default
    dev->active_channel_count = ADS131M0X_CHANNEL_COUNT;
    dev->crc.is_enabled = false;
    dev->crc.type = ADS131M0X_CRC_POLYNOMIAL_CCITT;

    dev->hal.syncResetSet(false);
    dev->hal.delayMs(1); // hold for at least 1ms
    dev->hal.syncResetSet(true);
    dev->hal.delayMs(1); // let device boot

    uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
    memset(rx_buf, 0, sizeof(rx_buf));
	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes    = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

    ADS131M0XError err = ads131m0xRead(dev, rx_buf, frame_bytes);
    if (err != ADS131M0X_ERROR_OK)
        return err;

	const uint16_t response = ((uint16_t)rx_buf[0] << 8) | ((uint16_t)rx_buf[1]);
	if (response != ADS131M0X_RESP_RESET_OK)
		return ADS131M0X_ERROR_ID_MISMATCH;

    dev->is_initialized = true;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xDeinit(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

    dev->is_initialized = false;
    dev->hal = (ADS131M0XHAL){0}; // zero all function pointers — prevents stale HAL calls

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReset(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;
	packWord(tx_buf, (uint16_t)ADS131M0X_CMD_RESET, dev->word_length);

    ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
    if (err != ADS131M0X_ERROR_OK)
        return err;

	dev->hal.delayMs(1); // Wait for device to complete internal reset

	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(rx_buf, 0, sizeof(rx_buf));
	err = ads131m0xRead(dev, rx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	const uint16_t response = ((uint16_t)rx_buf[0] << 8) | ((uint16_t)rx_buf[1]);
	if (response != ADS131M0X_RESP_RESET_OK)
		return ADS131M0X_ERROR_SPI;

    dev->word_length = ADS131M0X_WLENGTH_24_BIT;  // power-on default
    dev->active_channel_count = ADS131M0X_CHANNEL_COUNT;
    dev->is_locked = false;
    dev->crc.is_enabled = false;
    dev->crc.type = ADS131M0X_CRC_POLYNOMIAL_CCITT;

	return ADS131M0X_ERROR_OK;
}

int64_t ads131m0xConvertToMicrovolts(int32_t raw_code, ADS131M0XGain gain)
{
	// voltage_uv = raw * ADS131M0X_REFERENCE_VOLTAGE_UV / (gain_multiplier * 2^23)
	const uint32_t gain_multiplier = 1U << (uint8_t)gain;
	return raw_code * ADS131M0X_REFERENCE_VOLTAGE_UV / (gain_multiplier * ADS131M0X_23_BITS);
}

ADS131M0XError ads131m0xReadData(const ADS131M0X* const dev, ADS131M0XData* const data)
{
	if (dev == NULL || data == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word; // Status + CRC + Channel words
	const uint8_t crc_offset = (1U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	/* Frame 1: send NULL command */
	memset(tx_buf, 0, sizeof(tx_buf));
	packWord(tx_buf, (uint16_t)ADS131M0X_CMD_NULL, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Frame 2: read response containing status + channel data + CRC */
	memset(rx_buf, 0, sizeof(rx_buf));
	err = ads131m0xRead(dev, rx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Extract status from word[0] */
	data->status = ((uint16_t)rx_buf[0] << 8U) | rx_buf[1];

	/* Extract CRC from last word — always present on wire */
	data->crc = ((uint16_t)rx_buf[crc_offset] << 8U) | rx_buf[crc_offset + 1U];

	/* Validate CRC if enabled */
	ADS131M0XError crc_err = checkCRC(dev, rx_buf);
	if (crc_err != ADS131M0X_ERROR_OK && crc_err != ADS131M0X_ERROR_CRC)
		return crc_err;
	data->crc_valid = (crc_err == ADS131M0X_ERROR_OK);

	/* Extract channel data from words 1..CHANNEL_COUNT */
	for (uint8_t i = 0; i < ADS131M0X_CHANNEL_COUNT; i++)
	{
		uint8_t offset = (i + 1U) * bytes_per_word;
		data->channel_data[i] = signExtend(&rx_buf[offset], dev->word_length);
	}

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xStandby(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	uint16_t response = 0;
	ADS131M0XError err = sendCommand(dev, ADS131M0X_CMD_STANDBY, &response);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	if (response != ADS131M0X_RESP_STANDBY)
		return ADS131M0X_ERROR_SPI;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xWakeup(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	uint16_t response = 0;
	ADS131M0XError err = sendCommand(dev, ADS131M0X_CMD_WAKEUP, &response);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	if (response != ADS131M0X_RESP_WAKEUP)
		return ADS131M0X_ERROR_SPI;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xLock(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	uint16_t response = 0;
	ADS131M0XError err = sendCommand(dev, ADS131M0X_CMD_LOCK, &response);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	if (response != ADS131M0X_RESP_LOCK)
		return ADS131M0X_ERROR_SPI;

	dev->is_locked = true;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xUnlock(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	uint16_t response = 0;
	ADS131M0XError err = sendCommand(dev, ADS131M0X_CMD_UNLOCK, &response);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	if (response != ADS131M0X_RESP_UNLOCK)
		return ADS131M0X_ERROR_SPI;

	dev->is_locked = false;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReadRegister(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value)
{
	if (dev == NULL || value == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	/* Frame 1: send RREG command */
	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	uint16_t opcode = ADS131M0X_CMD_RREG | ((uint16_t)address << 7U);
	packWord(tx_buf, opcode, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Frame 2: read register value */
	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(rx_buf, 0, sizeof(rx_buf));
	err = ads131m0xRead(dev, rx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Validate CRC */
	ADS131M0XError crc_err = checkCRC(dev, rx_buf);
	if (crc_err != ADS131M0X_ERROR_OK)
		return crc_err;

	*value = ((uint16_t)rx_buf[0] << 8U) | rx_buf[1];

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReadRegisters(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value, const uint8_t count)
{
	if (dev == NULL || value == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	/* Limited to CHANNEL_COUNT by the fixed-size stack frame buffer.
	 * Larger bursts require a larger buffer.*/
	if (count > ADS131M0X_CHANNEL_COUNT || count == 0)
		return ADS131M0X_ERROR_INVALID_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];

	/* Frame 1: send RREG command with register count */
	memset(tx_buf, 0, sizeof(tx_buf));
	uint16_t opcode = ADS131M0X_CMD_RREG | ((uint16_t)address << 7U) | ((count - 1U) & 0x7FU);
	packWord(tx_buf, opcode, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Frame 2: read the single response frame containing all register values */
	memset(rx_buf, 0, sizeof(rx_buf));
	err = ads131m0xRead(dev, rx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Validate CRC */
	ADS131M0XError crc_err = checkCRC(dev, rx_buf);
	if (crc_err != ADS131M0X_ERROR_OK)
		return crc_err;

	for (uint8_t i = 0; i < count; i++)
	{
		uint8_t offset = (count == 1) ? 0 : (i + 1U) * bytes_per_word;
		value[i] = ((uint16_t)rx_buf[offset] << 8U) | rx_buf[offset + 1U];
	}

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xWriteRegister(const ADS131M0X* const dev, const uint8_t address, const uint16_t value)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	uint16_t opcode = ADS131M0X_CMD_WREG | ((uint16_t)address << 7U);
	packWord(tx_buf, opcode, dev->word_length);
	packWord(&tx_buf[bytes_per_word], value, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xWriteRegisters(const ADS131M0X* const dev, const uint8_t address, const uint16_t* const value, const uint8_t count)
{
	if (dev == NULL || value == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (count > ADS131M0X_CHANNEL_COUNT || count == 0)
		return ADS131M0X_ERROR_INVALID_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	// Status + CRC + Channel words
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	uint16_t opcode = ADS131M0X_CMD_WREG | ((uint16_t)address << 7U) | ((count - 1U) & 0x7FU);
	packWord(tx_buf, opcode, dev->word_length);
	for (uint8_t i = 0; i < count; i++)
	{
		packWord(&tx_buf[(1 + i) * bytes_per_word], value[i], dev->word_length);
	}

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	return ADS131M0X_ERROR_OK;
}

// ── Bare Bones API ──────────────────────────────────────────────────────────
ADS131M0XError ads131m0xRead(const ADS131M0X* const dev, void* const value, const uint8_t count)
{
	if (dev == NULL || value == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (dev->hal.spiRead(value, count) != 0)
		return ADS131M0X_ERROR_SPI;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xWrite(const ADS131M0X* const dev, const void* const value, const uint8_t count)
{
	if (dev == NULL || value == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (dev->hal.spiWrite(value, count) != 0)
		return ADS131M0X_ERROR_SPI;

	return ADS131M0X_ERROR_OK;
}

// ── Static Helper Functions ────────────────────────────────────────────────
static uint8_t bytesPerWord(ADS131M0XWordLength word_length)
{
	switch (word_length)
	{
		case ADS131M0X_WLENGTH_16_BIT:
			return 2U;
		case ADS131M0X_WLENGTH_24_BIT:
			return 3U;
		case ADS131M0X_WLENGTH_32_BIT_ZERO:
		case ADS131M0X_WLENGTH_32_BIT_SIGN:
			return 4U;
		default:
			return 3U;
	}
}

static void packWord(uint8_t* buf, uint16_t word, ADS131M0XWordLength word_length)
{
	switch (word_length)
	{
		case ADS131M0X_WLENGTH_16_BIT:
			buf[0] = (uint8_t)(word >> 8U);
			buf[1] = (uint8_t)(word & 0xFFU);
			break;
		case ADS131M0X_WLENGTH_24_BIT:
			buf[0] = (uint8_t)(word >> 8U);
			buf[1] = (uint8_t)(word & 0xFFU);
			buf[2] = 0x00U;
			break;
		case ADS131M0X_WLENGTH_32_BIT_ZERO:
			buf[0] = (uint8_t)(word >> 8U);
			buf[1] = (uint8_t)(word & 0xFFU);
			buf[2] = 0x00U;
			buf[3] = 0x00U;
			break;
		case ADS131M0X_WLENGTH_32_BIT_SIGN:
			buf[0] = (word & 0x8000U) ? 0xFF : 0x00;
			buf[1] = (uint8_t)(word >> 8U);
			buf[2] = (uint8_t)(word & 0xFFU);
			buf[3] = 0x00U;
			break;
		default:
			break;
	}
}

static int32_t signExtend(const uint8_t* bytes, ADS131M0XWordLength word_length)
{
	switch (word_length)
	{
		case ADS131M0X_WLENGTH_16_BIT:
		{
			int32_t upper_byte   = ((int32_t) bytes[0] << 24);
			int32_t lower_byte   = ((int32_t) bytes[1] << 16);
			return (((int32_t) (upper_byte | lower_byte)) >> 16);
		}
		case ADS131M0X_WLENGTH_24_BIT:
		{
			int32_t upper_byte   = ((int32_t) bytes[0] << 24);
			int32_t middle_byte  = ((int32_t) bytes[1] << 16);
			int32_t lower_byte   = ((int32_t) bytes[2] << 8);
			return (((int32_t) (upper_byte | middle_byte | lower_byte)) >> 8);
		}
		case ADS131M0X_WLENGTH_32_BIT_ZERO:
		{
			int32_t upper_byte   = ((int32_t) bytes[0] << 24);
			int32_t middle_byte  = ((int32_t) bytes[1] << 16);
			int32_t lower_byte   = ((int32_t) bytes[2] << 8);
			return (((int32_t) (upper_byte | middle_byte | lower_byte)) >> 8);
		}
		case ADS131M0X_WLENGTH_32_BIT_SIGN:
		{
			int32_t sign_byte    = ((int32_t) bytes[0] << 24);
			int32_t upper_byte   = ((int32_t) bytes[1] << 16);
			int32_t middle_byte  = ((int32_t) bytes[2] << 8);
			int32_t lower_byte   = ((int32_t) bytes[3] << 0);
			return (sign_byte | upper_byte | middle_byte | lower_byte);
		}
		default:
			return 0;
	}
}

static uint16_t calculateCRC(uint16_t crc_polynomial, const uint8_t* data, uint8_t length_bytes, uint16_t initialValue)
{
	uint16_t crc = initialValue;

	for (uint8_t i = 0; i < length_bytes; i++)
	{
		for (uint8_t bit = 0x80U; bit != 0; bit >>= 1)
		{
			bool data_msb = (data[i] & bit) != 0;
			bool crc_msb  = (crc & 0x8000U) != 0;
			crc <<= 1;
			if (data_msb ^ crc_msb)
			{
				crc ^= crc_polynomial;
			}
		}
	}

	return crc;
}

static ADS131M0XError sendCommand(const ADS131M0X* const dev, ADS131M0XCommand command, uint16_t* const response)
{
	uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word; // Status + CRC + Channel words

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	packWord(tx_buf, (uint16_t)command, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(rx_buf, 0, sizeof(rx_buf));

	err = ads131m0xRead(dev, rx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	ADS131M0XError crc_err = checkCRC(dev, rx_buf);
	if (crc_err != ADS131M0X_ERROR_OK)
		return crc_err;

	if (response != NULL)
		*response = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];

	return ADS131M0X_ERROR_OK;
}

static ADS131M0XError checkCRC(const ADS131M0X* const dev, const uint8_t* buf)
{
	if (dev == NULL || buf == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->crc.is_enabled)
		return ADS131M0X_ERROR_OK;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	const uint8_t crc_offset = (ADS131M0X_CHANNEL_COUNT + 1U) * bytes_per_word;

	const uint16_t crc_polynomial = (dev->crc.type == ADS131M0X_CRC_POLYNOMIAL_ANSI)
									? ADS131M0X_CRC_POLYNOMIAL_HEX_ANSI
									: ADS131M0X_CRC_POLYNOMIAL_HEX_CCITT;

	const uint16_t calculated_crc  = calculateCRC(crc_polynomial, buf, crc_offset, 0xFFFFU);
	const uint16_t received_crc  = ((uint16_t)buf[crc_offset] << 8) | buf[crc_offset + 1U];

	if (calculated_crc != received_crc)
		return ADS131M0X_ERROR_CRC;

	return ADS131M0X_ERROR_OK;
}
