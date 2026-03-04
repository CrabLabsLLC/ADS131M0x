#include "ads131m0x.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_types.h"
#include "ads131m0x_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ADS131M0X_WORD_SIZE_BYTES  3U
#define ADS131M0X_FRAME_WORDS      6U
#define ADS131M0X_FRAME_SIZE_BYTES (ADS131M0X_WORD_SIZE_BYTES * ADS131M0X_FRAME_WORDS)
#define RESET_DELAY_MS           1U // 1ms delay after reset
#define ADS131M0X_NUM_CHANNELS   4U
#define ADS131M0X_MAX_REG_COUNT (ADS131M0X_FRAME_WORDS - 1U) // Words 1-5 carry register data

// ADS131M04 ID register: upper byte is 0x24 (datasheet Table 8-14)
#define ID_UPPER_BYTE  0x24U
#define ID_UPPER_MASK  0xFF00U

// ── Layer 1: Raw bus wrappers ─────────────────────────────────────────────
static ADS131M0XError ads131m0xRead(const ADS131M0X* const dev, void* const data, const uint8_t length)
{
    if (dev->hal.spiRead(data, length) != 0)
        return ADS131M0X_ERROR_COMM_FAIL;

	printf("rx: ");
	for (uint8_t i = 0; i < length; i++) printf("%02X ", ((uint8_t*)data)[i]);
	printf("\n");
    return ADS131M0X_ERROR_OK;
}

static ADS131M0XError ads131m0xWrite(const ADS131M0X* const dev, const void* const data, const uint8_t length)
{
    if (dev->hal.spiWrite(data, length) != 0)
        return ADS131M0X_ERROR_COMM_FAIL;

	printf("tx: ");
    for (uint8_t i = 0; i < length; i++) printf("%02X ", ((uint8_t*)data)[i]);
    printf("\n");

    return ADS131M0X_ERROR_OK;
}

// ── Layer 2: Register access ──────────────────────────────────────────────
// Datasheet:
/// Section 8.5.1.7 + Figure 8.18
/// Section 8.5.1.10 (Table 8-11)

ADS131M0XError ads131m0xWriteRegisters(const ADS131M0X* const dev, const uint8_t reg, const uint16_t* const values, const uint8_t count)
{
    if (count == 0U || count > ADS131M0X_MAX_REG_COUNT)
        return ADS131M0X_ERROR_INVALID_PARAM;

    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    // Word 0: WREG command with register address and count
    const uint16_t cmd = ADS131M0X_CMD_WREG | ((uint16_t)reg << 7U) | (uint16_t)(count - 1U);
    frame[0] = (uint8_t)(cmd >> 8U); // MSB: bits 15:8
    frame[1] = (uint8_t)(cmd & 0xFFU); // LSTB: bits 7:0

    // Words 1..count: 16-bit register data packed into 24-bit words (MSB first)
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t offset = (i + 1U) * ADS131M0X_WORD_SIZE_BYTES;
        frame[offset]     = (uint8_t)(values[i] >> 8U);
        frame[offset + 1] = (uint8_t)(values[i] & 0xFFU);
    }

    return ads131m0xWrite(dev, frame, sizeof(frame));
}

ADS131M0XError ads131m0xReadRegisters(const ADS131M0X* const dev, const uint8_t reg, uint16_t* const values, const uint8_t count)
{
    if (count == 0U || count > ADS131M0X_MAX_REG_COUNT)
        return ADS131M0X_ERROR_INVALID_PARAM;

    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    // Frame 1: send RREG command with register address and count
    const uint16_t cmd = ADS131M0X_CMD_RREG | ((uint16_t)reg << 7U) | (uint16_t)(count - 1U);
    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));

    if (err != ADS131M0X_ERROR_OK)
        return err;

    // Frame 2: send NULL, capture response
    uint8_t rx[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    err = ads131m0xRead(dev, rx, sizeof(rx));

    if (err != ADS131M0X_ERROR_OK)
        return err;

    // Unpack 16-bit register values from 24-bit response words (MSB first)
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t offset = i * ADS131M0X_WORD_SIZE_BYTES;
        values[i] = ((uint16_t)rx[offset] << 8U) | (uint16_t)rx[offset + 1];
    }

    return ADS131M0X_ERROR_OK;
}


ADS131M0XError ads131m0xSendCommand(const ADS131M0X* const dev, const ADS131M0XCommand cmd)
{
    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    const ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));

    return err;
}

// ── Layer 3: High-level API ──────────────────────────────────────────────

ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal)
{
    if (dev == NULL || hal == NULL)
        return ADS131M0X_ERROR_INVALID_PARAM;

    if (hal->spiRead == NULL || hal->spiWrite == NULL)
        return ADS131M0X_ERROR_INVALID_PARAM;

    if (hal->delayMs == NULL)
        return ADS131M0X_ERROR_INVALID_PARAM;

    dev->is_initialized = false;

    dev->hal.spiRead  = hal->spiRead;
    dev->hal.spiWrite = hal->spiWrite;
    dev->hal.delayMs  = hal->delayMs;
    dev->hal.drdyGet  = hal->drdyGet;
    dev->hal.sleepSet = hal->sleepSet;

    dev->word_length          = ADS131M0X_WLENGTH_24_BIT;
    dev->is_input_crc_enabled = false;

    /* Verify chip is present by reading ID register */
    uint16_t chip_id = 0;
    ADS131M0XError err = ads131m0xReadRegisters(dev, ADS131M0X_REG_ID, &chip_id, 1U);
    printf("Chip ID read: 0x%04X (err=%d)\n", chip_id, err);
    if (err != ADS131M0X_ERROR_OK)
        return ADS131M0X_ERROR_COMM_FAIL;

    if ((chip_id & ID_UPPER_MASK) != ((uint16_t)ID_UPPER_BYTE << 8U))
        return ADS131M0X_ERROR_BAD_ID;

    dev->is_initialized = true;
    return ADS131M0X_ERROR_OK;
}

ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id)
{
    if (dev == NULL || id == NULL)
        return ADS131M0X_ERROR_INVALID_PARAM;

    if (!dev->is_initialized)
        return ADS131M0X_ERROR_NOT_INIT;

	ADS131M0XError err = ads131m0xReadRegisters(dev, ADS131M0X_REG_ID, id, 1U);
	return err;
}
