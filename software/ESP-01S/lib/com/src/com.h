#ifndef _COM_H_
#define _COM_H_

#include <Arduino.h>
#include "commands.h"

uint8_t sendByte(uint8_t data, uint8_t* buf);
uint8_t sendByteRetry(uint8_t data, uint8_t* buf, size_t size, uint32_t attempts, uint32_t wait);
void sendByteNoResponse(uint8_t data);

#endif
