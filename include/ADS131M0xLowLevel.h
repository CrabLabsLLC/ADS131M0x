#pragma once
#ifndef ADS131M0XLOWLEVEL_H
#define ADS131M0XLOWLEVEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ADS131M0xTypes.h"
#include "ADS131M0xRegisters.h"

// ~~~~~~~~~~~~ Low Level API ~~~~~~~~~~~~ //

ADS131M0xError ADS131M0x_WriteRegister (const ADS131M0x* const device, const ADS131M0xRegister reg, uint16_t value);

ADS131M0xError ADS131M0x_ReadRegister (const ADS131M0x* const device, const ADS131M0xRegister reg, void* const buffer);

// ~~~~~~~~~~~~ Bare Bones API ~~~~~~~~~~~ //

ADS131M0xError ADS131M0x_Write(const ADS131M0x* const device, const void* const buffer, uint8_t length);

ADS131M0xError ADS131M0x_Read(const ADS131M0x* const device, void* const buffer, uint8_t length);

#endif // ADS131M0XLOWLEVEL_H