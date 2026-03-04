#include "ads131m0x.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_types.h"
#include "ads131m0x_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ADS131M0X_WORD_SIZE_BYTES  3U
#define ADS131M0X_FRAME_WORDS      6U
#define ADS131M0X_FRAME_SIZE_BYTES (ADS131M0X_WORD_SIZE_BYTES * ADS131M0X_FRAME_WORDS)
#define RESET_DELAY_MS           1U // 1ms delay after reset

// ── Layer 1: Raw bus wrappers ─────────────────────────────────────────────
static ADS131M0XError ads131m0xRead(const ADS131M0XDevice* const dev, void* const data, const uint8_t length)
{
    if (dev->hal.spiRead(data, length) != 0)
        return ADS131M0X_ERROR_COMM_FAIL;
    return ADS131M0X_ERROR_OK;
}

static ADS131M0XError ads131m0xWrite(const ADS131M0XDevice* const dev, const void* const data, const uint8_t length)
{
    if (dev->hal.spiWrite(data, length) != 0)
        return ADS131M0X_ERROR_COMM_FAIL;
    return ADS131M0X_ERROR_OK;
}

// ── Layer 2: Register access ──────────────────────────────────────────────
// Datasheet:
/// Section 8.5.1.7 + Figure 8.18
/// Section 8.5.1.10 (Table 8-11)

static ADS131M0XError ads131m0xWriteRegister(const ADS131M0XDevice* const dev, const uint8_t reg, const uint16_t value)
{
    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    // Word 0: WREG command
    const uint16_t cmd = ADS131M0X_CMD_WREG | ((uint16_t)reg << 7U);
    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    // Word 1: Register data
    frame[3] = (uint8_t)(value >> 8U);
    frame[4] = (uint8_t)(value & 0xFFU);

    // Words 2-5 are zeros (is_input_crc_enabled == false by default)

    const ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));

    return err;
}

static ADS131M0XError ads131m0xReadRegister(const ADS131M0XDevice* const dev, const uint8_t reg, uint16_t* const value)
{
    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    // Frame 1: send RREG command
    const uint16_t cmd = ADS131M0X_CMD_RREG | ((uint16_t)reg << 7U);
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

    // Response word: 16-bit register data
    *value = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];

    return ADS131M0X_ERROR_OK;
}

static ADS131M0XError ads131m0xSendCommand(const ADS131M0XDevice* const dev, const uint16_t cmd)
{
    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    const ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));

    return err;
}

// ── Layer 3: High-level API ──────────────────────────────────────────────

ADS131M0XError ads131m0xInit(ADS131M0XDevice* const dev, const ADS131M0XHAL* const hal)
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
    dev->hal.resetSet = hal->resetSet;
    dev->hal.drdyGet  = hal->drdyGet;
    dev->hal.sleepSet = hal->sleepSet;

    dev->word_length          = ADS131M0X_WLENGTH_24_BIT;
    dev->is_input_crc_enabled = false;

    if (dev->hal.resetSet != NULL)
    {
        dev->hal.resetSet(false);
        dev->hal.delayMs(RESET_DELAY_MS);
        dev->hal.resetSet(true);
        dev->hal.delayMs(RESET_DELAY_MS);
    }

    /* Verify chip is present by reading ID register */
    uint16_t chip_id = 0;
    ADS131M0XError err = ads131m0xReadRegister(dev, ADS131M0X_REG_ID, &chip_id);
    if (err != ADS131M0X_ERROR_OK)
        return ADS131M0X_ERROR_COMM_FAIL;

    if ((chip_id & ID_UPPER_MASK) != ((uint16_t)ID_UPPER_BYTE << 8U))
        return ADS131M0X_ERROR_BAD_ID;

    dev->is_initialized = true;
    return ADS131M0X_ERROR_OK;
}
