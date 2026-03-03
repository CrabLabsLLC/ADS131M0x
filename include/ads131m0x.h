#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ads131m0x_types.h"
#include "ads131m0x_registers.h"
#include "ads131m0x_low_level.h"

// ── High Level API ──────────────────────────────────────────────────────────

Ads131m0xError ads131m0xInit(Ads131m0x* device,
                             const Ads131m0xConfig* config,
                             const Ads131m0xHAL* hal);

Ads131m0xError ads131m0xReadDeviceId(const Ads131m0x* const device,
                                     uint8_t* const channel_count);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
