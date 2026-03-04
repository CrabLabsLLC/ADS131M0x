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

ADS131M0XError ads131m0xInit(ADS131M0XDevice* const dev, const ADS131M0XHAL* const hal);
ADS131M0XError ads131m0xReadChipId(const ADS131M0XDevice* const dev, uint16_t* const id);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
