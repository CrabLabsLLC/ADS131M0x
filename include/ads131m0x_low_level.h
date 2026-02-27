#pragma once
#ifndef ADS131M0X_LOW_LEVEL_H
#define ADS131M0X_LOW_LEVEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ads131m0x_types.h"
#include "ads131m0x_registers.h"

// ~~~~~~~~~~~~ Low Level API ~~~~~~~~~~~~ //

/**
 * @brief Write to a device register.
 *
 * @param[in]  device  Device descriptor. Must not be NULL.
 * @param[in]  reg     Register to write.
 * @param[in]  value   Value to write.
 * @return ADS131M0X_ERROR_OK on success; otherwise an error code.
 */
Ads131m0xError ads131m0xWriteRegister(const Ads131m0x* const device, const Ads131m0xRegister reg, const uint16_t value);

/**
 * @brief Read from a device register.
 *
 * @param[in]  device  Device descriptor. Must not be NULL.
 * @param[in]  reg     Register to read.
 * @param[out] buffer  Buffer to store the read value.
 * @return ADS131M0X_ERROR_OK on success; otherwise an error code.
 */
Ads131m0xError ads131m0xReadRegister(const Ads131m0x* const device, const Ads131m0xRegister reg, void* const buffer);

// ~~~~~~~~~~~~ Bare Bones API ~~~~~~~~~~~ //

/**
 * @brief Write raw buffer to the device via SPI.
 *
 * @param[in]  device        Device descriptor. Must not be NULL.
 * @param[in]  buffer        Data buffer to write.
 * @param[in]  length_bytes  Number of bytes to write.
 * @return ADS131M0X_ERROR_OK on success; otherwise an error code.
 */
Ads131m0xError ads131m0xWrite(const Ads131m0x* const device, const void* const buffer, const uint8_t length_bytes);

/**
 * @brief Read raw buffer from the device via SPI.
 *
 * @param[in]  device        Device descriptor. Must not be NULL.
 * @param[out] buffer        Data buffer to read into.
 * @param[in]  length_bytes  Number of bytes to read.
 * @return ADS131M0X_ERROR_OK on success; otherwise an error code.
 */
Ads131m0xError ads131m0xRead(const Ads131m0x* const device, void* const buffer, const uint8_t length_bytes);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_LOW_LEVEL_H
