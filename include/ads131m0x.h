/**
 * @file ads131m0x.h
 * @brief ADS131M0x multi-channel delta-sigma ADC driver — public API.
 *
 * API layering (top = highest level):
 *   1. Initialization / Lifecycle
 *   2. Configuration
 *   3. Data acquisition & conversion
 *   4. Device control (standby, lock, reset)
 *   5. Diagnostics
 *   6. Register access
 *   7. Raw SPI access
 */
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

/** @brief Internal 1.2 V reference expressed in microvolts. */
#define ADS131M0X_REFERENCE_VOLTAGE_UV 1200000LL
/** @brief 2^23 — full-scale code range for a 24-bit signed ADC. */
#define ADS131M0X_23_BITS 8388608LL

// ── Initialization / Lifecycle ────────────────────────────────────────────────

/**
 * @brief Initialize the ADS131M0X device.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @param[in]     hal Pointer to the hardware abstraction layer.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal);

/**
 * @brief Deinitialize the ADS131M0X device.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xDeinit(ADS131M0X* const dev);

// ── Configuration ─────────────────────────────────────────────────────────────

/**
 * @brief Configure global device settings (MODE, CLOCK, CFG, THRSHLD).
 * @param[in,out] dev    Pointer to the ADS131M0X device structure.
 * @param[in]     config Pointer to the global configuration structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xConfigure(ADS131M0X* const dev, const ADS131M0XConfig* const config);

/**
 * @brief Configure a specific channel (CHn_CFG, OCAL, GCAL, GAIN, CLOCK enable).
 * @param[in,out] dev     Pointer to the ADS131M0X device structure.
 * @param[in]     channel The channel index (0 to ADS131M0X_CHANNEL_COUNT-1).
 * @param[in]     config  Pointer to the channel configuration structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xConfigureChannel(ADS131M0X* const dev, uint8_t channel, const ADS131M0XChannelConfig* const config);

// ── Data Acquisition & Conversion ─────────────────────────────────────────────

/**
 * @brief Read one conversion frame (status + channel data + CRC).
 * @param[in]  dev  Pointer to the ADS131M0X device structure.
 * @param[out] data Pointer to the data structure to populate.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadData(const ADS131M0X* const dev, ADS131M0XData* const data);

/**
 * @brief Convert a raw ADC code to microvolts.
 * @param[in] raw_code Signed 24-bit ADC output code.
 * @param[in] gain     PGA gain setting used during the conversion.
 * @return Voltage in microvolts (signed).
 * @note voltage_uv = raw_code * V_REF_UV / (gain_multiplier * 2^23)
 */
int64_t ads131m0xConvertToMicrovolts(int32_t raw_code, ADS131M0XGain gain);

// ── Device Control ────────────────────────────────────────────────────────────

/**
 * @brief Reset the ADS131M0X device via SPI command.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReset(ADS131M0X* const dev);

/**
 * @brief Put the ADS131M0X device into standby mode.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xStandby(ADS131M0X* const dev);

/**
 * @brief Wake up the ADS131M0X device from standby mode.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWakeup(ADS131M0X* const dev);

/**
 * @brief Lock the SPI interface (only NULL, RREG, UNLOCK accepted).
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xLock(ADS131M0X* const dev);

/**
 * @brief Unlock the SPI interface after a previous LOCK.
 * @param[in,out] dev Pointer to the ADS131M0X device structure.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xUnlock(ADS131M0X* const dev);

// ── Diagnostics ───────────────────────────────────────────────────────────────

/**
 * @brief Read the chip ID register (address 0x00).
 * @param[in]  dev Pointer to the ADS131M0X device structure.
 * @param[out] id  Pointer to store the 16-bit chip ID.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id);

/**
 * @brief Convert an error code to a human-readable string.
 * @param[in] err The error code to convert.
 * @return Pointer to a NUL-terminated string literal.
 */
const char* ads131m0xErrorToString(ADS131M0XError err);

// ── Register Access ───────────────────────────────────────────────────────────

/**
 * @brief Read a single register.
 * @param[in]  dev     Pointer to the ADS131M0X device structure.
 * @param[in]  address Register address (0x00–0x3F).
 * @param[out] value   Pointer to store the 16-bit register value.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadRegister(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value);

/**
 * @brief Read consecutive registers via a single RREG burst.
 * @param[in]  dev     Pointer to the ADS131M0X device structure.
 * @param[in]  address Starting register address.
 * @param[out] value   Array to store the register values.
 * @param[in]  count   Number of registers to read (1 to ADS131M0X_CHANNEL_COUNT).
 * @note The count limit is imposed by the fixed-size stack frame buffer,
 *       not by the device itself.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xReadRegisters(const ADS131M0X* const dev, const uint8_t address, uint16_t* const value, const uint8_t count);

/**
 * @brief Write a single register.
 * @param[in] dev     Pointer to the ADS131M0X device structure.
 * @param[in] address Register address (0x00–0x3F).
 * @param[in] value   16-bit value to write.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWriteRegister(const ADS131M0X* const dev, const uint8_t address, const uint16_t value);

/**
 * @brief Write consecutive registers via a single WREG burst.
 * @param[in] dev     Pointer to the ADS131M0X device structure.
 * @param[in] address Starting register address.
 * @param[in] value   Array of 16-bit values to write.
 * @param[in] count   Number of registers to write (1 to ADS131M0X_CHANNEL_COUNT).
 * @note The count limit is imposed by the fixed-size stack frame buffer,
 *       not by the device itself.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWriteRegisters(const ADS131M0X* const dev, const uint8_t address, const uint16_t* const value, const uint8_t count);

// ── Raw SPI Access ────────────────────────────────────────────────────────────

/**
 * @brief Raw SPI read — clocks in @p count bytes from DOUT.
 * @param[in]  dev   Pointer to the ADS131M0X device structure.
 * @param[out] value Buffer to receive the bytes.
 * @param[in]  count Number of bytes to read.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xRead(const ADS131M0X* const dev, void* const value, const uint8_t count);

/**
 * @brief Raw SPI write — clocks out @p count bytes on DIN.
 * @param[in] dev   Pointer to the ADS131M0X device structure.
 * @param[in] value Buffer containing the bytes to send.
 * @param[in] count Number of bytes to write.
 * @return ADS131M0X_ERROR_OK if successful, otherwise an error code.
 */
ADS131M0XError ads131m0xWrite(const ADS131M0X* const dev, const void* const value, const uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
