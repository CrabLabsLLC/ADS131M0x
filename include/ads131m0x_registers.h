#pragma once
#ifndef ADS131M0X_REGISTERS_H
#define ADS131M0X_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Register addresses ──────────────────────────────────────────────────────
// Datasheet: Section 8.6 - Table 8-12
// Phase 1: Register Addresses
#define ADS131M0X_REG_ID             0x00U
#define ADS131M0X_REG_STATUS         0x01U
#define ADS131M0X_REG_MODE           0x02U
#define ADS131M0X_REG_CLOCK          0x03U
#define ADS131M0X_REG_GAIN           0x04U
// 05h is reserved
#define ADS131M0X_REG_CFG            0x06U
#define ADS131M0X_REG_THRSHLD_MSB    0x07U
#define ADS131M0X_REG_THRSHLD_LSB    0x08U

// ADS131M04 ID register: upper byte is 0x24 (datasheet Table 8-14)
#define ADS131M0X_ID_UPPER_BYTE  0x24U
#define ADS131M0X_ID_UPPER_MASK  0xFF00U

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_REGISTERS_H
