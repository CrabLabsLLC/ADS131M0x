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

ADS131M0XError ads131m0xInit(ADS131M0X* const dev, const ADS131M0XHAL* const hal);
ADS131M0XError ads131m0xReadChipId(const ADS131M0X* const dev, uint16_t* const id);

ADS131M0XError ads131m0xSendCommand(const ADS131M0X* const dev, const ADS131M0XCommand cmd);

ADS131M0XError ads131m0xReadRegisters(const ADS131M0X* const dev, const uint8_t reg, uint16_t* const values, const uint8_t count);
ADS131M0XError ads131m0xWriteRegisters(const ADS131M0X* const dev, const uint8_t reg, const uint16_t* const values, const uint8_t count);

ADS131M0XError ads131m0xRead(const ADS131M0X* const dev, void* const data, const uint8_t length);
ADS131M0XError ads131m0xWrite(const ADS131M0X* const dev, const void* const data, const uint8_t length);

const char* ads131m0xErrorToString(ADS131M0XError err);

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_H
