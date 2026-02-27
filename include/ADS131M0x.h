#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ads131m0x_types.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_low_level.h"

// ~~~~~~~~~~~~ High Level API ~~~~~~~~~~~~ //

/**
 * @brief Initialize the ADS131M0x device.
 *
 * @param[in]  device   Device context structure. Must not be NULL.
 * @param[in]  config   Device configuration. Must not be NULL.
 * @param[in]  hal      HAL parameters. Must not be NULL.
 * @return ADS131M0X_ERROR_OK on success; otherwise an error code.
 */
Ads131m0xError ads131m0xInit(Ads131m0x* device, const Ads131m0xConfig* config, const Ads131m0xHAL* hal);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
