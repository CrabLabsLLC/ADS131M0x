#include "ads131m0x.h"
#include "ads131m0x_low_level.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_types.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ADS131M0X_FRAME_SIZE_BYTES  18U ///< 6 words × 3 bytes
#define ADS131M0X_POR_DELAY_MS  5U

static uint16_t ads131m0xBuildWregCmd(const uint8_t addr, const uint8_t count);
static uint16_t ads131m0xBuildRregCmd(const uint8_t addr, const uint8_t count);

// ── High-Level API ──────────────────────────────────────────────────────────

Ads131m0xError ads131m0xInit(Ads131m0x* device,
                             const Ads131m0xConfig* config,
                             const Ads131m0xHAL* hal)
{
    if (device == NULL || config == NULL || hal == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (hal->spi_read == NULL || hal->spi_write == NULL ||
        hal->cs_set == NULL || hal->delayMs == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;

    device->hal = *hal;
    device->config = *config;
    device->is_initialized = false;
    device->channel_count = 0;

    // Software reset
    Ads131m0xError err = ads131m0xSendCommand(device, ADS131M0X_CMD_RESET);
    if (err != ADS131M0X_ERROR_OK)
        return err;

    device->hal.delayMs(ADS131M0X_POR_DELAY_MS);

    // Enable register reads
    device->is_initialized = true;

    // Unlock if locked (device unlocked by default, but safe to check)
    uint16_t status_val = 0;
    err = ads131m0xReadRegister(device, ADS131M0X_REG_STATUS, &status_val);
    if (err != ADS131M0X_ERROR_OK)
    {
        device->is_initialized = false;
        return err;
    }

    if (status_val & ADS131M0X_STATUS_LOCK_MASK)
    {
        err = ads131m0xSendCommand(device, ADS131M0X_CMD_UNLOCK);
        if (err != ADS131M0X_ERROR_OK)
        {
            device->is_initialized = false;
            return err;
        }
    }

    // Read device ID
    uint8_t ch_count = 0;
    err = ads131m0xReadDeviceId(device, &ch_count);
    if (err != ADS131M0X_ERROR_OK)
    {
        device->is_initialized = false;
        return err;
    }

    device->channel_count = ch_count;
    return ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xReadDeviceId(const Ads131m0x* const device,
                                     uint8_t* const channel_count)
{
    assert(device != NULL);
    assert(channel_count != NULL);

    if (device == NULL || channel_count == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    uint16_t id_val = 0;
    const Ads131m0xError err = ads131m0xReadRegister(device, ADS131M0X_REG_ID, &id_val);
    if (err != ADS131M0X_ERROR_OK)
        return err;

    *channel_count = (uint8_t)((id_val & ADS131M0X_ID_CHANCNT_MASK)
                               >> ADS131M0X_ID_CHANCNT_SHIFT);
    return ADS131M0X_ERROR_OK;
}

// ── Low-Level API ───────────────────────────────────────────────────────────

Ads131m0xError ads131m0xSendCommand(const Ads131m0x* const device,
                                    const Ads131m0xCommand cmd)
{
    assert(device != NULL);
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;

    uint8_t tx[ADS131M0X_FRAME_SIZE_BYTES] = {0};
    tx[0] = (uint8_t)((uint16_t)cmd >> 8);
    tx[1] = (uint8_t)((uint16_t)cmd);

    device->hal.cs_set(0);
    const uint8_t spi_err = device->hal.spi_write(tx, sizeof(tx));
    device->hal.cs_set(1);

    return spi_err ? ADS131M0X_ERROR_SPI : ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xWrite(const Ads131m0x* const device,
                              const void* const buffer,
                              const uint8_t length_bytes)
{
    assert(device != NULL);
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    device->hal.cs_set(0);
    const uint8_t spi_err = device->hal.spi_write(buffer, length_bytes);
    device->hal.cs_set(1);

    return spi_err ? ADS131M0X_ERROR_SPI : ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xRead(const Ads131m0x* const device,
                             void* const buffer,
                             const uint8_t length_bytes)
{
    assert(device != NULL);
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    device->hal.cs_set(0);
    const uint8_t spi_err = device->hal.spi_read(buffer, length_bytes);
    device->hal.cs_set(1);

    return spi_err ? ADS131M0X_ERROR_SPI : ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xWriteRegister(const Ads131m0x* const device,
                                      const Ads131m0xRegister reg,
                                      const uint16_t value)
{
    assert(device != NULL);
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint16_t cmd = ads131m0xBuildWregCmd((uint8_t)reg, 1);

    uint8_t tx[ADS131M0X_FRAME_SIZE_BYTES] = {0};
    tx[0] = (uint8_t)(cmd >> 8);
    tx[1] = (uint8_t)(cmd);
    tx[3] = (uint8_t)(value >> 8);
    tx[4] = (uint8_t)(value);

    device->hal.cs_set(0);
    const uint8_t spi_err = device->hal.spi_write(tx, sizeof(tx));
    device->hal.cs_set(1);

    return spi_err ? ADS131M0X_ERROR_SPI : ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xReadRegister(const Ads131m0x* const device,
                                     const Ads131m0xRegister reg,
                                     uint16_t* const value)
{
    assert(device != NULL);
    assert(value != NULL);
    if (device == NULL || value == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    // Frame N: send RREG command
    const uint16_t cmd = ads131m0xBuildRregCmd((uint8_t)reg, 1);
    uint8_t tx[ADS131M0X_FRAME_SIZE_BYTES] = {0};
    tx[0] = (uint8_t)(cmd >> 8);
    tx[1] = (uint8_t)(cmd);

    device->hal.cs_set(0);
    uint8_t spi_err = device->hal.spi_write(tx, sizeof(tx));
    device->hal.cs_set(1);

    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    // Frame N+1: clock out NULL, response in Word 0
    static const uint8_t zeros[ADS131M0X_FRAME_SIZE_BYTES] = {0};
    uint8_t rx[ADS131M0X_FRAME_SIZE_BYTES] = {0};

    device->hal.cs_set(0);
    spi_err = device->hal.spi_read(rx, sizeof(rx));
    device->hal.cs_set(1);

    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    *value = (uint16_t)((rx[0] << 8) | rx[1]);
    return ADS131M0X_ERROR_OK;
}

// ── Static ──────────────────────────────────────────────────────────────────

static uint16_t ads131m0xBuildWregCmd(const uint8_t addr, const uint8_t count)
{
    assert(addr <= 0x3FU);
    assert(count > 0);
    return (uint16_t)(ADS131M0X_CMD_WREG | ((addr & 0x3FU) << 7) | ((count - 1U) & 0x7FU));
}

static uint16_t ads131m0xBuildRregCmd(const uint8_t addr, const uint8_t count)
{
    assert(addr <= 0x3FU);
    assert(count > 0);
    return (uint16_t)(ADS131M0X_CMD_RREG | ((addr & 0x3FU) << 7) | ((count - 1U) & 0x7FU));
}
