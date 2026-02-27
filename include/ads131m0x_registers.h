#pragma once
#ifndef ADS131M0X_REGISTERS_H
#define ADS131M0X_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Register addresses ──────────────────────────────────────────────────────
typedef enum
{
    ADS131M0X_REG_ID = 0x00U,     ///< Device ID (read-only)
    ADS131M0X_REG_STATUS = 0x01U, ///< Device status
    ADS131M0X_REG_MODE = 0x02U,   ///< Conversion mode
    ADS131M0X_REG_CLOCK = 0x03U,  ///< Clock configuration
    ADS131M0X_REG_GAIN1 = 0x04U,  ///< PGA gain CH0–CH3
    ADS131M0X_REG_CFG = 0x06U,    ///< Global configuration
} Ads131m0xRegister;

// ── Command opcodes (16-bit; shift left 8 to form 24-bit frame word) ────────
typedef enum
{
    ADS131M0X_CMD_NULL = 0x0000U, ///< No-op / clock out pending response / listening
    ADS131M0X_CMD_RESET = 0x0011U,   ///< Software reset
    ADS131M0X_CMD_STANDBY = 0x0022U, ///< Enter standby
    ADS131M0X_CMD_WAKEUP = 0x0033U,  ///< Exit standby
    ADS131M0X_CMD_LOCK = 0x0555U,    ///< Lock register writes
    ADS131M0X_CMD_UNLOCK = 0x0655U  ///< Unlock register writes
} Ads131m0xCommand;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_REGISTERS_H
