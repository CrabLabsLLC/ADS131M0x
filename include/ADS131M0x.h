#pragma once
#ifndef ADS131M0X_H
#define ADS131M0X_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ADS131M0xTypes.h"
#include "ADS131M0xRegisters.h"

// ~~~~~~~~~~~~ HIgh Level API ~~~~~~~~~~~~ //

ADS131M0xError ADS131M0x_Init(ADS131M0x* device, const ADS131M0xConfig* cfg, const ADS131M0xHAL* hal);

#endif // ADS131M0X_H