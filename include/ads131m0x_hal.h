#pragma once
#ifndef ADS131M0X_HAL_H
#define ADS131M0X_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
	int (*spiRead)(void* const data, const uint8_t length);
	int (*spiWrite)(const void* const data, const uint8_t length);
	void (*sleepSet)(const bool state);       ///< Sleep/enable control
	bool (*drdyGet)(void);                    ///< Data ready interrupt pin
	void (*delayMs)(const uint32_t delayMs);
} ADS131M0XHAL;

#endif // ADS131M0X_HAL_H
