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

// ── Initialization ────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal);
ADS131M0XError ads131m0xDeinit(ADS131M0X* const dev);

// ── Configuration ─────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xConfigureGlobal(ADS131M0X* const dev, const ADS131M0XGlobalConfig* const config);
ADS131M0XError ads131m0xConfigureChannel(ADS131M0X* const dev, uint8_t channel, const ADS131M0XChannelConfig* const config);

// ── Device control ────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xReset(ADS131M0X* const dev);
ADS131M0XError ads131m0xStandby(ADS131M0X* const dev);
ADS131M0XError ads131m0xWakeup(ADS131M0X* const dev);
ADS131M0XError ads131m0xLock(ADS131M0X* const dev);
ADS131M0XError ads131m0xUnlock(ADS131M0X* const dev);

// ── Data ──────────────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xReadData(const ADS131M0X* const dev, ADS131M0XData* const data);
int64_t ads131m0xConvertToMicrovolts(int32_t raw_code, ADS131M0XGain gain); // voltage_uv = raw * 1200000 / (gain * 2^23)

// ── Register access ───────────────────────────────────────────────────────────
ADS131M0XError ads131m0xReadRegister(ADS131M0X* const dev, uint8_t address, uint16_t* const value);
ADS131M0XError ads131m0xWriteRegister(ADS131M0X* const dev, uint8_t address, uint16_t value);

// ── Diagnostics ───────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id);
const char* ads131m0xErrorToString(ADS131M0XError err);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
