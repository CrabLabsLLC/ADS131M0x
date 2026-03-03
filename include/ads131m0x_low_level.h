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

// ── Low Level API ───────────────────────────────────────────────────────────

Ads131m0xError ads131m0xWriteRegister(const Ads131m0x* const device,
                                      const Ads131m0xRegister reg,
                                      const uint16_t value);

Ads131m0xError ads131m0xReadRegister(const Ads131m0x* const device,
                                     const Ads131m0xRegister reg,
                                     uint16_t* const value);

Ads131m0xError ads131m0xSendCommand(const Ads131m0x* const device,
                                    const Ads131m0xCommand cmd);

// ── Bare Bones API ──────────────────────────────────────────────────────────

Ads131m0xError ads131m0xWrite(const Ads131m0x* const device,
                              const void* const buffer,
                              const uint8_t length_bytes);

Ads131m0xError ads131m0xRead(const Ads131m0x* const device,
                             void* const buffer,
                             const uint8_t length_bytes);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_LOW_LEVEL_H
