#pragma once
#ifndef ADS131M0X_HAL_H
#define ADS131M0X_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int (*spiRead)(void* const data, const uint8_t length);
	int (*spiWrite)(const void* const data, const uint8_t length);
	bool (*drdyGet)(void);
	void (*delayMs)(const uint32_t delayMs);
} ADS131M0XHAL;

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_HAL_H
