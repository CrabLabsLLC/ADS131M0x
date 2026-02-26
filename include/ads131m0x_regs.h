#pragma once
#ifndef ADS131M0X_REGS_H
#define ADS131M0X_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Register addresses ──────────────────────────────────────────────────────
#define ADS131M0X_REG_ID 0x00U     ///< Device ID (read-only)
#define ADS131M0X_REG_STATUS 0x01U ///< Device status
#define ADS131M0X_REG_MODE 0x02U   ///< Conversion mode
#define ADS131M0X_REG_CLOCK 0x03U  ///< Clock configuration
#define ADS131M0X_REG_GAIN1 0x04U  ///< PGA gain CH0–CH3
#define ADS131M0X_REG_CFG 0x06U    ///< Global configuration

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_REGS_H