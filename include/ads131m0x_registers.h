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

// Phase 3: Commands and Expected Responses
// Datasheet: Section 8.5.1.10 (Table 8-11)
#define ADS131M0X_CMD_NULL    0x0000U
#define ADS131M0X_CMD_RESET   0x0011U
#define ADS131M0X_CMD_STANDBY 0x0022U
#define ADS131M0X_CMD_WAKEUP  0x0033U
#define ADS131M0X_CMD_LOCK    0x0555U
#define ADS131M0X_CMD_UNLOCK  0x0655U
#define ADS131M0X_CMD_RREG    0xA000U
#define ADS131M0X_CMD_WREG    0x6000U

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_REGISTERS_H
