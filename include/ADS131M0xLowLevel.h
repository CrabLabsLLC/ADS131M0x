#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ADS131M0xTypes.h"
#include "ADS131M0xRegisters.h"

// ~~~~~~~~~~~~ Low Level API ~~~~~~~~~~~~ //

ADS131M0xError ads131m0xWriteRegister (const ADS131M0x* const device, uint8_t reg, uint16_t value);

#endif // ADS131M0X_H