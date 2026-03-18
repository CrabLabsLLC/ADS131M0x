#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "ads131m0x_types.h"
#include "ads131m0x_registers.h"

#define ADS131M0X_REFERENCE_VOLTAGE_V 1.2f

// ── Initialization ────────────────────────────────────────────────────────────
/**
 * @brief Initialize the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param hal Pointer to the hardware abstraction layer.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal);
/**
 * @brief Deinitialize the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xDeinit(ADS131M0X* const dev);

// ── Configuration ─────────────────────────────────────────────────────────────
/**
 * @brief Configure the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param config Pointer to the configuration structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xConfigure(ADS131M0X* const dev, const ADS131M0XConfig* const config);
/**
 * @brief Configure a specific channel of the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param channel The channel to configure.
 * @param config Pointer to the channel configuration structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xConfigureChannel(ADS131M0X* const dev, uint8_t channel, const ADS131M0XChannelConfig* const config);

// ── Device control ────────────────────────────────────────────────────────────
/**
 * @brief Reset the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReset(ADS131M0X* const dev);
/**
 * @brief Put the ADS131M0X device into standby mode.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xStandby(ADS131M0X* const dev);
/**
 * @brief Wake up the ADS131M0X device from standby mode.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWakeup(ADS131M0X* const dev);
/**
 * @brief Lock the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xLock(ADS131M0X* const dev);
/**
 * @brief Unlock the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xUnlock(ADS131M0X* const dev);

// ── Data ──────────────────────────────────────────────────────────────────────
/**
 * @brief Read data from the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param data Pointer to the data structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadData(const ADS131M0X* const dev, ADS131M0XData* const data);
/**
 * @brief Convert raw code to microvolts.
 * @param raw_code The raw code from the ADC.
 * @param gain The gain setting.
 * @return The voltage in microvolts.
 */
int64_t ads131m0xConvertToMicrovolts(int32_t raw_code, ADS131M0XGain gain); // voltage_uv = raw * ADS131M0X_REFERENCE_VOLTAGE_V * 1e6 / (gain * 2^23)

// ── Register access ───────────────────────────────────────────────────────────
/**
 * @brief Read consecutive registers from the ADS131M0X device.
 * @param[in]  dev     Pointer to the ADS131M0X device structure.
 * @param[in]  address The starting register address.
 * @param[out] value   Pointer to the array to store the register values.
 * @param[in]  count   The number of registers to read (1 to ADS131M0X_CHANNEL_COUNT).
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadRegisters(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value, const uint8_t count);
/**
 * @brief Read a single register from the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param address The address of the register to read.
 * @param value Pointer to the variable to store the register value.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadRegister(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value);
/**
 * @brief Write consecutive registers to the ADS131M0X device.
 * @param[in] dev     Pointer to the ADS131M0X device structure.
 * @param[in] address The starting register address.
 * @param[in] value   Pointer to the array of register values to write.
 * @param[in] count   The number of registers to write (1 to ADS131M0X_CHANNEL_COUNT).
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWriteRegisters(const ADS131M0X* const dev, const uint8_t address, const uint16_t* const value, const uint8_t count);
/**
 * @brief Write a single register to the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param address The address of the register to write.
 * @param value The value to write to the register.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWriteRegister(const ADS131M0X* const dev, const uint8_t address, const uint16_t value);

// ── SPI access ───────────────────────────────────────────────────────────
/**
 * @brief Read data from the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param value Pointer to the data structure.
 * @param count The number of bytes to read.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xRead(const ADS131M0X* const dev, void* const value, const uint8_t count);
/**
 * @brief Write data to the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param value Pointer to the data structure.
 * @param count The number of bytes to write.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWrite(const ADS131M0X* const dev, const void* const value, const uint8_t count);

// ── Diagnostics ───────────────────────────────────────────────────────────────
/**
 * @brief Read the chip ID from the ADS131M0X device.
 * @param dev Pointer to the ADS131M0X device structure.
 * @param id Pointer to the variable to store the chip ID.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id);
/**
 * @brief Convert an error code to a human-readable string.
 * @param err The error code to convert.
 * @return A pointer to the error string.
 */
const char* ads131m0xErrorToString(ADS131M0XError err);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
