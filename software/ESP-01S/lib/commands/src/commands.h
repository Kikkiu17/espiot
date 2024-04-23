#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <stdint.h>

#define MAX_TRIES UINT32_MAX

#define OK 0x01
#define BUSY 0x02

#define STARTUP 0x10
#define READY 0x11
#define REQUEST_CURRENT 0x12
#define REQUEST_VOLTAGE 0x13
#define REQUEST_POWER 0x14
#define STM_RESET 0x15
#define TIMEOUT 0x20
#define NORESPONSE 0x21
#define FATAL_ERROR 0x22

#endif
