/*
 * wifi.h
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#ifndef WIFI_WIFIHANDLER_H_
#define WIFI_WIFIHANDLER_H_

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "../ESP8266/esp8266.h"
#include "../settings.h"

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr);
Response_t WIFIHANDLER_HandleFeaturePacket(Connection_t* conn, char* features_template);
Response_t WIFIHANDLER_HandleNotificationRequest(Connection_t* conn, char* key_ptr);

void NOTIFICATION_Reset();
void NOTIFICATION_Set(char* text, uint8_t size);

void LED_Strobe(void);

#endif /* WIFI_WIFIHANDLER_H_ */
