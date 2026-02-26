#include "ADS131M0x.h"
#include "ADS131M0xLowLevel.h"
#include "ADS131M0xRegisters.h"
#include "ADS131M0xTypes.h"

#include <assert.h>
#include <cstdint>
#include <stdbool.h>
#include <stddef.h>

#define ADS131M0X_FRAME_SIZE_BYTES 18U    // 6 words × 3 bytes
#define ADS131M0X_STARTUP_DUMMY_FRAMES 2U // Discard first 2 samples after reset
#define ADS131M0X_POR_DELAY_MS 1U         // ≥250µs after power-up

static uint16_t ADS131M0x_BuildWregCmd(uint8_t addr, uint8_t count);
static uint16_t ADS131M0x_BuildRregCmd(uint8_t addr, uint8_t count);

ADS131M0xError ads131m0xInit(ADS131M0x* device, const ADS131M0xConfig* cfg, const ADS131M0xHAL* hal) 
{
    if (device == NULL || cfg == NULL || hal == NULL || hal->spi_read == NULL || hal->spi_write == NULL)
        return ADS131M0X_ERROR_INVALID_ARG;

    device->hal = *hal;
    device->config = *cfg;
    device->is_initialized = true;

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