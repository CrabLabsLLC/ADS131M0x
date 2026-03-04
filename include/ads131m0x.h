#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ads131m0x.h"
#include "ads131m0x_types.h"
#include "ads131m0x_registers.h"

// ── Lifecycle ───────────────────────────────────────────────────────────
ADS131M0XError ads131m0xInit(ADS131M0XDevice* const dev, const ADS131M0XHAL* const hal);
ADS131M0XError ads131m0xDeinit(ADS131M0XDevice* const dev);
ADS131M0XError ads131m0xReset(ADS131M0XDevice* const dev);

// ── Global config ───────────────────────────────────────────────────────
ADS131M0XError ads131m0xSetOsr(ADS131M0XDevice* const dev, const ADS131M0XOsr osr);
ADS131M0XError ads131m0xSetPowerMode(ADS131M0XDevice* const dev, const ADS131M0XPowerMode mode);
ADS131M0XError ads131m0xEnableGlobalChop(ADS131M0XDevice* const dev, const bool is_enabled);
ADS131M0XError ads131m0xEnableCrc(ADS131M0XDevice* const dev, const bool is_enabled);

// ── Per-channel config ────────────────────────────────────────────────
ADS131M0XError ads131m0xEnableChannel(ADS131M0XDevice* const dev, const uint8_t channel);
ADS131M0XError ads131m0xDisableChannel(ADS131M0XDevice* const dev, const uint8_t channel);
ADS131M0XError ads131m0xSetChannelGain(ADS131M0XDevice* const dev, const uint8_t channel, const ADS131M0XGain gain);
ADS131M0XError ads131m0xSetChannelInput(ADS131M0XDevice* const dev, const uint8_t channel, const ADS131M0XMux input);

// ── Data ────────────────────────────────────────────────────────────────
ADS131M0XError ads131m0xReadAllChannels(const ADS131M0XDevice* const dev, int32_t* const samples);
ADS131M0XError ads131m0xConvertToVoltageUv(const ADS131M0XDevice* const dev, const int32_t raw, int32_t* const voltage_uv);

// ── Power management ────────────────────────────────────────────────────
ADS131M0XError ads131m0xStandby(ADS131M0XDevice* const dev);
ADS131M0XError ads131m0xWakeup(ADS131M0XDevice* const dev);
ADS131M0XError ads131m0xLock(ADS131M0XDevice* const dev);
ADS131M0XError ads131m0xUnlock(ADS131M0XDevice* const dev);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
