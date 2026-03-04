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
// Datasheet: Section 8.5.1.7 + Figure 8.18

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

    dev->hal.csSet(true);
    const ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));
    dev->hal.csSet(false);

    return err;
}

static ADS131M0XError ads131m0xReadRegister(const ADS131M0XDevice* const dev, const uint8_t reg, uint16_t* const value)
{
    uint8_t frame[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    // Frame 1: send RREG command
    const uint16_t cmd = ADS131M0X_CMD_RREG | ((uint16_t)reg << 7U);
    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    dev->hal.csSet(true);
    ADS131M0XError err = ads131m0xWrite(dev, frame, sizeof(frame));
    dev->hal.csSet(false);

    if (err != ADS131M0X_ERROR_OK)
        return err;

    // Frame 2: send NULL, capture response
    uint8_t rx[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    dev->hal.csSet(true);
    err = ads131m0xRead(dev, rx, sizeof(rx));
    dev->hal.csSet(false);

    if (err != ADS131M0X_ERROR_OK)
        return err;

    // Response word: 16-bit register data
    *value = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];

    return ADS131M0X_ERROR_OK;
}
