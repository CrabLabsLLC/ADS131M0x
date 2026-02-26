#include "ADS131M0x.h"
#include "ADS131M0xLowLevel.h"
#include "ADS131M0xRegisters.h"
#include "ADS131M0xTypes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define ADS131M0X_FRAME_SIZE_BYTES 18U    // 6 words × 3 bytes
#define ADS131M0X_STARTUP_DUMMY_FRAMES 2U // Discard first 2 samples after reset
#define ADS131M0X_POR_DELAY_MS 1U         // ≥250µs after power-up

static uint16_t ADS131M0x_BuildWregCmd(uint8_t addr, uint8_t count);
static uint16_t ADS131M0x_BuildRregCmd(uint8_t addr, uint8_t count);

ADS131M0xError ADS131M0x_Init(ADS131M0x* device, const ADS131M0xConfig* cfg, const ADS131M0xHAL* hal) 
{
    if (device == NULL || cfg == NULL || hal == NULL || hal->spi_read == NULL || hal->spi_write == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;

    device->hal = *hal;
    device->config = *cfg;
    device->is_initialized = true;

    return ADS131M0X_ERROR_OK;
}

ADS131M0xError ADS131M0x_Write(const ADS131M0x* const device, const void* const buffer, uint8_t length)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint8_t spi_err = device->hal.spi_write(buffer, length);
    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    return ADS131M0X_ERROR_OK;
}

ADS131M0xError ADS131M0x_Read(const ADS131M0x* const device, void* const buffer, uint8_t length)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    const uint8_t spi_err = device->hal.spi_read(buffer, length);
    if (spi_err)
        return ADS131M0X_ERROR_SPI;

    return ADS131M0X_ERROR_OK;
}

ADS131M0xError ADS131M0x_WriteRegister(const ADS131M0x *const device, const ADS131M0xRegister reg, uint16_t value)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    uint16_t cmd = ADS131M0x_BuildWregCmd((uint8_t)reg, 1);

    const uint8_t tx[6] = 
    {
        (uint8_t)(cmd >> 8), // high byte of cmd
        (uint8_t)(cmd), // low byte of cmd
        0x00U, // padding
        (uint8_t)(value >> 8),
        (uint8_t)(value),
        0x00U
    };

    const ADS131M0xError err = ADS131M0x_Write(device, tx, sizeof(tx));
    if (err)
        return err;

    return ADS131M0X_ERROR_OK;
}

ADS131M0xError ADS131M0x_ReadRegister (const ADS131M0x* const device, const ADS131M0xRegister reg, void* const buffer)
{
    if (device == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;
    if (!device->is_initialized)
        return ADS131M0X_ERROR_NOT_INITIALIZED;

    uint16_t cmd = ADS131M0x_BuildRregCmd((uint8_t)reg, 1);

    const uint8_t rx[6] = 
    {
        (uint8_t)(cmd >> 8), // high byte of cmd
        (uint8_t)(cmd), // low byte of cmd
        0x00U, // padding
        (uint8_t)(value >> 8),
        (uint8_t)(value),
        0x00U
    };

    const ADS131M0xError err = ADS131M0x_Write(device, rx, sizeof(rx));
    if (err)
        return err;

    return ADS131M0X_ERROR_OK;
}

static uint16_t ADS131M0x_BuildWregCmd(uint8_t addr, uint8_t count)
{
    // Combine:
    //   WREG opcode base  (bits 15:13 = 011)
    //   register address  (bits 12:7)
    //   count - 1         (bits 6:0)
    return (uint16_t)(0x6000U | (((addr) & 0x3FU) << 7) | (((count) - 1U) & 0x7FU));
}

static uint16_t ADS131M0x_BuildRregCmd(uint8_t addr, uint8_t count)
{
    return (uint16_t)(0xA000U | (((addr) & 0x3FU) << 7) | (((count) - 1U) & 0x7FU));
}