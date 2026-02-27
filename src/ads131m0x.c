#include "ads131m0x.h"
#include "ads131m0x_low_level.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_types.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ADS131M0X_FRAME_SIZE_BYTES     18U ///< 6 words × 3 bytes
#define ADS131M0X_STARTUP_DUMMY_FRAMES 2U  ///< Discard first 2 samples after reset
#define ADS131M0X_POR_DELAY_MS         1U  ///< ≥250µs after power-up

static uint16_t ads131m0xBuildWregCmd(const uint8_t addr, const uint8_t count);
static uint16_t ads131m0xBuildRregCmd(const uint8_t addr, const uint8_t count);

Ads131m0xError ads131m0xInit(Ads131m0x* device, const Ads131m0xConfig* config, const Ads131m0xHAL* hal)
{
    if (device == NULL || config == NULL || hal == NULL || hal->spi_read == NULL || hal->spi_write == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;

    device->hal = *hal;
    device->config = *config;
    device->is_initialized = true;

    return ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xWrite(const Ads131m0x* const device, const void* const buffer, const uint8_t length_bytes)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint8_t spi_err = device->hal.spi_write(buffer, length_bytes);
    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    return ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xRead(const Ads131m0x* const device, void* const buffer, const uint8_t length_bytes)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint8_t spi_err = device->hal.spi_read(buffer, length_bytes);
    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    return ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xWriteRegister(const Ads131m0x* const device, const Ads131m0xRegister reg, const uint16_t value)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint16_t cmd = ads131m0xBuildWregCmd((uint8_t)reg, 1);

    const uint8_t tx[6] =
    {
        (uint8_t)(cmd >> 8), // high byte of cmd
        (uint8_t)(cmd),      // low byte of cmd
        0x00U,               // padding
        (uint8_t)(value >> 8),
        (uint8_t)(value),
        0x00U
    };

    const Ads131m0xError err = ads131m0xWrite(device, tx, sizeof(tx));
    if (err)
        return err;

    return ADS131M0X_ERROR_OK;
}

Ads131m0xError ads131m0xReadRegister(const Ads131m0x* const device, const Ads131m0xRegister reg, void* const buffer)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint16_t cmd = ads131m0xBuildRregCmd((uint8_t)reg, 1);

    const uint8_t tx[3] =
    {
        (uint8_t)(cmd >> 8), // high byte of cmd
        (uint8_t)(cmd),      // low byte of cmd
        0x00U,               // padding
    };

    const Ads131m0xError write_err = ads131m0xWrite(device, tx, sizeof(tx));
    if (write_err)
        return write_err;

    const Ads131m0xError read_err = ads131m0xRead(device, buffer, 3);
    if (read_err)
        return read_err;

    return ADS131M0X_ERROR_OK;
}

static uint16_t ads131m0xBuildWregCmd(const uint8_t addr, const uint8_t count)
{
    // Combine:
    //   WREG opcode base  (bits 15:13 = 011)
    //   register address  (bits 12:7)
    //   count - 1         (bits 6:0)
    return (uint16_t)(0x6000U | ((addr & 0x3FU) << 7) | ((count - 1U) & 0x7FU));
}

static uint16_t ads131m0xBuildRregCmd(const uint8_t addr, const uint8_t count)
{
    return (uint16_t)(0xA000U | ((addr & 0x3FU) << 7) | ((count - 1U) & 0x7FU));
}
