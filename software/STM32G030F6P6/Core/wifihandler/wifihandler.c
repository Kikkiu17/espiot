/*
 * wifi.c
 *
 *  Created on: Apr 12, 2025
 *      Author: Kikkiu
 */

#include "wifihandler.h"
#include "../Flash/flash.h"

Notification_t notification;

void NOTIFICATION_Set(char* text, uint8_t size)
{
	notification.text = text;
	notification.size = size;
}

void NOTIFICATION_Reset()
{
	notification.text = NULL;
	notification.size = 0;
}

Response_t WIFIHANDLER_HandleNotificationRequest(Connection_t* conn, char* key_ptr)
{
	if (conn->request_type == GET)
	{
		if (notification.size == 0 || notification.text == NULL)
			return WIFI_SendResponse(conn, "200 OK", "Vuoto", 5);
		else
			return WIFI_SendResponse(conn, "200 OK", notification.text, notification.size);
	}

	return WIFI_SendResponse(conn, "400 Bad Request", "Sono supportate solo richieste NOTIFICATION GET", 47);
}

Response_t WIFIHANDLER_HandleWiFiRequest(Connection_t* conn, char* command_ptr)
{
	if (conn->request_type == POST)
	{
		if (WIFI_RequestKeyHasValue(conn, command_ptr, "changename"))
		{
			char* name_ptr = WIFI_RequestHasKey(conn, "name");
			if (name_ptr == NULL)
				return WIFI_SendResponse(conn, "400 Bad Request", "Chiave \"name\" non trovata", 25);
			else
			{
				uint32_t name_size = 0;
				name_ptr = WIFI_GetKeyValue(conn, name_ptr, &name_size);
				if (name_ptr == NULL)
					return WIFI_SendResponse(conn, "400 Bad Request", "Nome non trovato", 16);
				else
				{
					WIFI_SetName(conn->wifi, name_ptr);
#ifdef ENABLE_SAVE_TO_FLASH
					FLASH_WriteSaveData();	// save name
#endif
					return WIFI_SendResponse(conn, "200 OK", "Nome cambiato", 13);
				}
			}

		}
		else return WIFI_SendResponse(conn, "400 Bad Request", "Comando POST WiFi non riconosciuto. "
			"Scrivi wifi=help per una lista di comandi", 77);
	}
	else if (conn->request_type == GET)
	{
		if (WIFI_RequestKeyHasValue(conn, command_ptr, "SSID"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->SSID, strlen(conn->wifi->SSID));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "IP"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->IP, strlen(conn->wifi->IP));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "ID"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->hostname, strlen(conn->wifi->hostname));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "name"))
		{
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->name, strlen(conn->wifi->name));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "buf"))
		{
			// this is possible if RESPONSE_MAX_SIZE is at least as big as WIFI_BUF_MAX_SIZE
			if (RESPONSE_MAX_SIZE < WIFI_BUF_MAX_SIZE)
			{
				return WIFI_SendResponse(conn, "500 Internal server error", "RESPONSE_MAX_SIZE is "
						"smaller than WIFI_BUF_MAX_SIZE", 51);
			}
			else
				return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, sizeof(conn->wifi->buf));
		}
		else if (WIFI_RequestKeyHasValue(conn, command_ptr, "conn"))
		{
			sprintf(conn->wifi->buf, "Connection ID: %d\nrequest size: %" PRIu32
					"\nrequest: %s", conn->connection_number, conn->request_size, conn->request);
			return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
		}
		else return WIFI_SendResponse(conn, "400 Bad Request", "Comando GET WiFi non riconosciuto. "
				"Scrivi wifi=help per una lista di comandi", 76);
	}

	return ERR;
}

uint32_t feature_voltage;
uint32_t feature_current_integer_part;
uint32_t feature_current_decimal_part;
uint32_t feature_power_integer_part;
uint32_t feature_power_decimal_part;

Response_t WIFIHANDLER_HandleFeaturePacket(Connection_t* conn, char* features_template)
{
	memset(conn->wifi->buf, 0, WIFI_BUF_MAX_SIZE);
	sprintf(conn->wifi->buf, features_template,
			feature_voltage,
			feature_current_integer_part, feature_current_decimal_part,
			feature_power_integer_part, feature_power_decimal_part,
			uwTick);
	return WIFI_SendResponse(conn, "200 OK", conn->wifi->buf, strlen(conn->wifi->buf));
}

uint32_t strobeon = 0;
uint8_t strobeoff = 0;
void LED_Strobe(void)
{
	if (uwTick - strobeon > STROBE_DELAY)
	{
		strobeon = uwTick;
		strobeoff = 1;
  	  HAL_GPIO_TogglePin(STATUS_Port, STATUS_Pin);
	}

	if (strobeoff && uwTick - strobeon > STROBE_DURATION)
	{
		strobeoff = 0;
		HAL_GPIO_TogglePin(STATUS_Port, STATUS_Pin);
	}
}
