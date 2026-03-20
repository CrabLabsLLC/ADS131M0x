/**
 * @file ads131m0x.c
 * @brief ADS131M0x multi-channel delta-sigma ADC driver implementation.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "ads131m0x.h"

/* Maximum bytes per SPI frame: (channel count + status word + CRC word) * maximum bytes per word (32-bit -> 4 bytes) */
#define ADS131M0X_FRAME_SIZE_MAX_BYTES ((ADS131M0X_CHANNEL_COUNT + 2U) * 4U)
#define ADS131M0X_CRC_POLYNOMIAL_HEX_CCITT 0x1021U
#define ADS131M0X_CRC_POLYNOMIAL_HEX_ANSI  0x8005U

// ── Static Forward Declarations ─────────────────────────────────────────────────
static uint8_t bytesPerWord(ADS131M0XWordLength word_length);
static void packWord(uint8_t* buf, uint16_t word, ADS131M0XWordLength word_length);
static int32_t signExtend(const uint8_t* bytes, ADS131M0XWordLength word_length);
static uint16_t calculateCRC(uint16_t crc_polynomial, const uint8_t* data, uint8_t length_bytes, uint16_t initialValue);
static ADS131M0XError sendCommand(const ADS131M0X* const dev, ADS131M0XCommand command, uint16_t* const response);
static ADS131M0XError checkCRC(const ADS131M0X* const dev, const uint8_t* buf);

// ── Initialization / Lifecycle ──────────────────────────────────────────────────
ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal)
{
	if (dev == NULL || hal == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (hal->spiRead == NULL || hal->spiWrite == NULL || hal->drdyGet == NULL || hal->delayMs == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	dev->hal = *hal;
	dev->is_initialized = false;
	dev->is_locked = false;
	dev->word_length = ADS131M0X_WLENGTH_24_BIT;
	dev->crc.is_enabled = false;
	dev->crc.type = ADS131M0X_CRC_POLYNOMIAL_CCITT;

	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;

	/* Hardware reset via SYNC/RESET pin */
	if (dev->hal.syncResetSet != NULL)
	{
		dev->hal.syncResetSet(false);
		dev->hal.delayMs(1);
		dev->hal.syncResetSet(true);
		dev->hal.delayMs(1);

		/* Read the initial frame — device outputs FF24h after reset */
		uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
		memset(rx_buf, 0, sizeof(rx_buf));

		ADS131M0XError err = ads131m0xRead(dev, rx_buf, frame_bytes);
		if (err != ADS131M0X_ERROR_OK)
			return err;

		const uint16_t response = ((uint16_t)rx_buf[0] << 8) | ((uint16_t)rx_buf[1]);
		if (response != ADS131M0X_RESP_RESET_OK)
			return ADS131M0X_ERROR_SPI;
	}
	else
	{
		/* Send ads131m0xReset command if no pin provided */
		ADS131M0XError err = ads131m0xReset(dev);
		if (err != ADS131M0X_ERROR_OK)
			return err;
	}

	dev->is_initialized = true;

	/* ── Enter Standby Mode ─────────────────────────────────────────────── */
	ADS131M0XError err = ads131m0xStandby(dev);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xDeinit(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	dev->is_initialized = false;
	dev->hal = (ADS131M0XHAL){0};

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReset(ADS131M0X* const dev)
{
	if (dev == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	// if (!dev->is_initialized)
	// 	return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	uint8_t tx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(tx_buf, 0, sizeof(tx_buf));
	const uint8_t bytes_per_word = bytesPerWord(dev->word_length);
	const uint8_t frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * bytes_per_word;
	packWord(tx_buf, (uint16_t)ADS131M0X_CMD_RESET, dev->word_length);

	ADS131M0XError err = ads131m0xWrite(dev, tx_buf, frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	dev->hal.delayMs(1);

	/* Device resets to 24-bit word length, so we must frame the response accordingly */
	const uint8_t reset_bytes_per_word = bytesPerWord(ADS131M0X_WLENGTH_24_BIT);
	const uint8_t reset_frame_bytes = (2U + ADS131M0X_CHANNEL_COUNT) * reset_bytes_per_word;

	uint8_t rx_buf[ADS131M0X_FRAME_SIZE_MAX_BYTES];
	memset(rx_buf, 0, sizeof(rx_buf));
	err = ads131m0xRead(dev, rx_buf, reset_frame_bytes);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	const uint16_t response = ((uint16_t)rx_buf[0] << 8) | ((uint16_t)rx_buf[1]);
	if (response != ADS131M0X_RESP_RESET_OK)
		return ADS131M0X_ERROR_SPI;

	/* Restore power-on defaults in device handle */
	dev->word_length = ADS131M0X_WLENGTH_24_BIT;
	dev->is_locked = false;
	dev->crc.is_enabled = false;
	dev->crc.type = ADS131M0X_CRC_POLYNOMIAL_CCITT;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id)
{
	if (dev == NULL || id == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	return ads131m0xReadRegister(dev, ADS131M0X_ID_ADDRESS, id);
}

int64_t ads131m0xConvertToMicrovolts(int32_t raw_code, ADS131M0XGain gain)
{
	// voltage_uv = raw * ADS131M0X_REFERENCE_VOLTAGE_UV / (gain_multiplier * 2^23)
	const uint32_t gain_multiplier = 1U << (uint32_t)gain;
	return raw_code * ADS131M0X_REFERENCE_VOLTAGE_UV / (gain_multiplier * ADS131M0X_23_BITS);
}

ADS131M0XError ads131m0xConfigure(ADS131M0X* const dev, const ADS131M0XConfig* const config)
{
	if (dev == NULL || config == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	ADS131M0XError err;

	/* Pack MODE register */
	uint16_t mode_val = 0;
	mode_val |= ((uint16_t)config->crc.output_enabled << ADS131M0X_MODE_REG_CRC_EN_SHIFT) & ADS131M0X_MODE_REG_CRC_EN_MASK;
	mode_val |= ((uint16_t)config->crc.input_enabled << ADS131M0X_MODE_RX_CRC_EN_SHIFT) & ADS131M0X_MODE_RX_CRC_EN_MASK;
	mode_val |= ((uint16_t)config->crc.polynomial << ADS131M0X_MODE_CRC_TYPE_SHIFT) & ADS131M0X_MODE_CRC_TYPE_MASK;
	mode_val |= ((uint16_t)config->word_length << ADS131M0X_MODE_WLENGTH_SHIFT) & ADS131M0X_MODE_WLENGTH_MASK;
	mode_val |= ((uint16_t)config->spi_timeout_enabled << ADS131M0X_MODE_TIMEOUT_SHIFT) & ADS131M0X_MODE_TIMEOUT_MASK;
	mode_val |= ((uint16_t)config->data_ready.selection << ADS131M0X_MODE_DRDY_SEL_SHIFT) & ADS131M0X_MODE_DRDY_SEL_MASK;
	mode_val |= ((uint16_t)config->data_ready.hiz << ADS131M0X_MODE_DRDY_HIZ_SHIFT) & ADS131M0X_MODE_DRDY_HIZ_MASK;
	mode_val |= ((uint16_t)config->data_ready.format << ADS131M0X_MODE_DRDY_FMT_SHIFT) & ADS131M0X_MODE_DRDY_FMT_MASK;

	/* Pack CLOCK register */
	uint16_t clock_val = 0;
	clock_val |= ((1U << ADS131M0X_CHANNEL_COUNT) - 1U) << 8U;
	clock_val |= ((uint16_t)config->turbo_mode << ADS131M0X_CLOCK_TBM_SHIFT) & ADS131M0X_CLOCK_TBM_MASK;
	clock_val |= ((uint16_t)config->oversampling_ratio << ADS131M0X_CLOCK_OSR_SHIFT) & ADS131M0X_CLOCK_OSR_MASK;
	clock_val |= ((uint16_t)config->power_mode << ADS131M0X_CLOCK_PWR_SHIFT) & ADS131M0X_CLOCK_PWR_MASK;

	/* Write MODE + CLOCK as burst */
	uint16_t mode_clock[2] = { mode_val, clock_val };
	err = ads131m0xWriteRegisters(dev, ADS131M0X_MODE_ADDRESS, mode_clock, 2);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Update device state */
	dev->word_length = config->word_length;
	dev->crc.is_enabled = config->crc.output_enabled;
	dev->crc.type = config->crc.polynomial;

	/* Pack CFG register */
	uint16_t cfg_val = 0;
	cfg_val |= ((uint16_t)config->global_chop.delay << ADS131M0X_CFG_GC_DLY_SHIFT) & ADS131M0X_CFG_GC_DLY_MASK;
	cfg_val |= ((uint16_t)config->global_chop.is_enabled << ADS131M0X_CFG_GC_EN_SHIFT) & ADS131M0X_CFG_GC_EN_MASK;
	cfg_val |= ((uint16_t)config->current_detect.all_channels << ADS131M0X_CFG_CD_ALLCH_SHIFT) & ADS131M0X_CFG_CD_ALLCH_MASK;
	cfg_val |= ((uint16_t)config->current_detect.num << ADS131M0X_CFG_CD_NUM_SHIFT) & ADS131M0X_CFG_CD_NUM_MASK;
	cfg_val |= ((uint16_t)config->current_detect.len << ADS131M0X_CFG_CD_LEN_SHIFT) & ADS131M0X_CFG_CD_LEN_MASK;
	cfg_val |= ((uint16_t)config->current_detect.is_enabled << ADS131M0X_CFG_CD_EN_SHIFT) & ADS131M0X_CFG_CD_EN_MASK;

	/* Write CFG register */
	err = ads131m0xWriteRegister(dev, ADS131M0X_CFG_ADDRESS, cfg_val);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Pack THRSHLD_MSB and THRSHLD_LSB */
	uint16_t threshold_msb = (uint16_t)((config->current_detect.threshold >> 8) & 0xFFFF);
	uint16_t threshold_lsb = (uint16_t)(((config->current_detect.threshold & 0xFF) << 8) | ((uint16_t)config->dc_block & 0x0F));

	/* Write THRSHLD_MSB and THRSHLD_LSB as burst */
	uint16_t threshold[2] = { threshold_msb, threshold_lsb };
	err = ads131m0xWriteRegisters(dev, ADS131M0X_THRSHLD_MSB_ADDRESS, threshold, 2);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xConfigureChannel(ADS131M0X* const dev, uint8_t channel, const ADS131M0XChannelConfig* const config)
{
	if (dev == NULL || config == NULL)
		return ADS131M0X_ERROR_NULL_PARAM;

	if (!dev->is_initialized)
		return ADS131M0X_ERROR_NOT_INITIALIZED;

	if (dev->is_locked)
		return ADS131M0X_ERROR_LOCKED;

	if (channel >= ADS131M0X_CHANNEL_COUNT)
		return ADS131M0X_ERROR_INVALID_CHANNEL;

	ADS131M0XError err;

	const uint8_t base = ADS131M0X_CH_BASE_ADDRESS + (channel * ADS131M0X_CH_STRIDE);

	/* Update CLOCK register (read-modify-write) */
	uint16_t clock_val = 0;
	err = ads131m0xReadRegister(dev, ADS131M0X_CLOCK_ADDRESS, &clock_val);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	if (config->is_enabled)
		clock_val |= (uint16_t)(1U << (8U + channel));
	else
		clock_val &= ~(uint16_t)(1U << (8U + channel));

	err = ads131m0xWriteRegister(dev, ADS131M0X_CLOCK_ADDRESS, clock_val);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Update GAIN register (read-modify-write) */
	const uint8_t gain_reg = ADS131M0X_GAIN1_ADDRESS + (channel / 4U);
	const uint8_t gain_shift = (channel % 4U) * 4U;
	const uint16_t gain_mask = 0x07U << gain_shift;

	uint16_t gain_val = 0;
	err = ads131m0xReadRegister(dev, gain_reg, &gain_val);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	gain_val &= ~gain_mask;
	gain_val |= ((uint16_t)config->gain << gain_shift) & gain_mask;

	err = ads131m0xWriteRegister(dev, gain_reg, gain_val);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Pack CHn_CFG register */
	uint16_t chn_cfg = 0;
	chn_cfg |= ((uint16_t)config->phase_delay_cycles << ADS131M0X_CHn_CFG_PHASE_SHIFT) & ADS131M0X_CHn_CFG_PHASE_MASK;
	chn_cfg |= ((uint16_t)config->dc_block_disabled << ADS131M0X_CHn_CFG_DCBLK_DIS_SHIFT) & ADS131M0X_CHn_CFG_DCBLK_DIS_MASK;
	chn_cfg |= ((uint16_t)config->mux << ADS131M0X_CHn_CFG_MUX_SHIFT) & ADS131M0X_CHn_CFG_MUX_MASK;

	/* Write CHn_CFG register */
	err = ads131m0xWriteRegister(dev, base, chn_cfg);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	/* Pack OCAL_MSB and OCAL_LSB register */
	uint16_t ocal_msb = (uint16_t)((config->offset_cal >> 8) & 0xFFFF);
	uint16_t ocal_lsb = (uint16_t)((config->offset_cal & 0xFF) << 8);

	/* Pack GCAL_MSB and GCAL_LSB register */
	uint16_t gcal_msb = (uint16_t)((config->gain_cal >> 8) & 0xFFFF);
	uint16_t gcal_lsb = (uint16_t)((config->gain_cal & 0xFF) << 8);

	/* Write all 4 channel registers as burst */
	uint16_t ch_regs[4] = { ocal_msb, ocal_lsb, gcal_msb, gcal_lsb };
	err = ads131m0xWriteRegisters(dev, base + ADS131M0X_CH_OCAL_MSB_OFFSET, ch_regs, 4);
	if (err != ADS131M0X_ERROR_OK)
		return err;

	return ADS131M0X_ERROR_OK;
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

const char* ads131m0xErrorToString(ADS131M0XError err)
{
	switch (err)
	{
		case ADS131M0X_ERROR_OK: return "OK";
		case ADS131M0X_ERROR_NULL_PARAM: return "NULL_PARAM";
		case ADS131M0X_ERROR_INVALID_PARAM: return "INVALID_PARAM";
		case ADS131M0X_ERROR_INVALID_CHANNEL: return "INVALID_CHANNEL";
		case ADS131M0X_ERROR_NOT_INITIALIZED: return "NOT_INITIALIZED";
		case ADS131M0X_ERROR_LOCKED: return "LOCKED";
		case ADS131M0X_ERROR_SPI: return "SPI";
		case ADS131M0X_ERROR_CRC: return "CRC";
		case ADS131M0X_ERROR_ID_MISMATCH: return "ID_MISMATCH";
		default: return "ADS131M0X_ERROR_UNKNOWN";
	}
}

// ── Raw SPI Access ──────────────────────────────────────────────────────────────
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
		case ADS131M0X_WLENGTH_32_BIT_SIGN:
			buf[0] = (uint8_t)(word >> 8U);
			buf[1] = (uint8_t)(word & 0xFFU);
			buf[2] = 0x00U;
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
