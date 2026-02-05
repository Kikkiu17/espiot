/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "../credentials.h"
#include "../ESP8266/esp8266.h"
#include "../wifihandler/wifihandler.h"
#include "../Flash/flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t adc_buf[ADC_BUF_LEN];
uint32_t previous_voltage = 0;

WIFI_t wifi;
Connection_t conn;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
float SENS_GetCurrent()
{
	float max = 0, min = 4096;
	// 12-bit ADC must be used
	for (uint32_t i = 0; i < ADC_BUF_LEN; i+=2)
	{
		if (adc_buf[i] > max)
			max = adc_buf[i];
		if (adc_buf[i] < min)
			min = adc_buf[i];
	}

	max = max / 4096.0 * 3.3;
	min = min / 4096.0 * 3.3;

	/*
	 * Irms = measured_amplitude * divider_ratio / 2 / ACS712_sensitivity / √2
	 * where
	 * masured_amplitude = fabsf(max - min)
	 * divider_ratio = 1.37 (voltage divider ratio)
	 * division by two is used to get only one side of the sine wave
	 * ACS712_sensitivity = 0.185 mV/A
	 * division by √2 is used to get RMS value
	 *
	 * the numbers below are obtained by doing these multiplications and division in advance
	 */

	// adjust_value == 1.37 * 1.9145
	float adjust_value = 2.62286;
	float abs_amplitude = fabsf(max - min);
	// used with this particular ACS sensor, since its reading is not linear under 0.6A
	if (abs_amplitude < 0.25)
		adjust_value = 2.09;

	// abs_amplitude * 1.37 * 1.9145
	float Irms = abs_amplitude * adjust_value * CURRENT_CALIB_VALUE;

	if (Irms <= CURRENT_DEAD_ZONE) return 0;
	else return Irms;
}


uint32_t SENS_GetVoltage()
{
	// 12-bit ADC must be used
	uint32_t voltage_mean = 0, voltage = 0;
	for (uint32_t i = 1; i < ADC_BUF_LEN; i+=2)
		voltage_mean += adc_buf[i];

	/*
	 * 0.001745 = 3.3 / (4096 * 1024) * 2255
	 * 2255 is a calibration value. the voltage output of the operational amplifier
	 * has to be multiplied by this value to get the value of the mains voltage
	 * 256x, 8-BIT SHIFT OVERSAMPLER IS REQUIRED!
	 */

	voltage = (float)voltage_mean * 0.001731;
	if (voltage > 250)
		return 230;

	voltage = (voltage + previous_voltage) / 2;
		previous_voltage = voltage;

	return voltage * VOLTAGE_CALIB_VALUE;
}


HAL_StatusTypeDef ESP_SendRawValue(uint32_t value, uint32_t received_byte)
{
	uint8_t send_buffer[11];
	uint32_t raw_value = *(uint32_t*)&value;

	for (int32_t i = 10; i >= 1; i--)
	{
		send_buffer[i] = raw_value % 10;
		raw_value /= 10;
	}
	send_buffer[0] = received_byte;

	GPIOA->BSRR |= GPIO_PIN_7;
	HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, send_buffer, 11, HAL_MAX_DELAY);
	GPIOA->BRR |= GPIO_PIN_7;
	return status;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  if (ESP8266_Init() == TIMEOUT)
  {
	  while (1)
		  __NOP();
  }

#ifdef ENABLE_SAVE_TO_FLASH
  FLASH_ReadSaveData();
  if (WIFI_SetName(&wifi, savedata.name) == ERR)
    WIFI_SetName(&wifi, (char*)ESP_NAME); // happens when there is nothing saved to FLASH, so set default name
  WIFI_SetIP(&wifi, savedata.ip);         // if there is nothing saved to FLASH, this function does nothing
#else
  WIFI_SetName(&wifi, (char*)ESP_NAME);
#endif

  memcpy(wifi.SSID, ssid, strlen(ssid));
  memcpy(wifi.pw, password, strlen(password));
  HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 1);
  uint32_t connect_status = WIFI_Connect(&wifi);
  if (connect_status == FAIL || connect_status == ERROR)
  {
    // try again
    NVIC_SystemReset();
  }
  HAL_GPIO_WritePin(STATUS_Port, STATUS_Pin, 0);
  WIFI_EnableNTPServer(&wifi, 0);

  /*
  The first time the ESP connects to WiFi, the gateway assigns an IP to it, which now gets saved to FLASH.
  The next time the ESP connects, the gateway could assign a different IP; to prevent this, the function
  WIFI_SetIP(&wifi, savedata.ip); loads the IP previously saved on FLASH so that the ESP tries to connect
  and get this IP
  */
  strncpy(savedata.ip, wifi.IP, 15);
  FLASH_WriteSaveData();

  WIFI_StartServer(&wifi, SERVER_PORT);

  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buf, ADC_BUF_LEN);
  HAL_TIM_Base_Start(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  Response_t wifistatus = WAITING;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, 1);
  //uint32_t reconnection_timestamp = 0;
  while (1)
  {
	  /*if (uwTick - reconnection_timestamp > RECONNECTION_DELAY_MILLIS)
	  {
		  // check every RECONNECTION_DELAY_MINS if this device is connected to wifi. if it is, get
		  // latest connection info, otherwise connect
		  WIFI_Connect(&wifi);
		  reconnection_timestamp = uwTick;
	  }*/

	  wifistatus = WAITING;
	  // HANDLE WIFI CONNECTION
	  wifistatus = WIFI_ReceiveRequest(&wifi, &conn, AT_SHORT_TIMEOUT);
	  if (wifistatus == OK)
	  {
		  HAL_GPIO_TogglePin(STATUS_Port, STATUS_Pin);
		  char* key_ptr = NULL;

		  if ((key_ptr = WIFI_RequestHasKey(&conn, "wifi")))
			  WIFIHANDLER_HandleWiFiRequest(&conn, key_ptr);

		  else if (conn.request_type == GET)
		  {
			  if ((key_ptr = WIFI_RequestHasKey(&conn, "features")))
			  {
				  feature_voltage = SENS_GetVoltage();
				  float current = SENS_GetCurrent();
				  if (current < CURRENT_THRESHOLD)
					  current = 0.0f;
				  feature_current_integer_part = current;
				  feature_current_decimal_part = current * 100 - feature_current_integer_part * 100;
				  float power = feature_voltage * current;
				  feature_power_integer_part = power;
				  feature_power_decimal_part = power * 100 - feature_power_integer_part * 100;
				  WIFIHANDLER_HandleFeaturePacket(&conn, (char*)FEATURES_TEMPLATE);
			  }
			  else if ((key_ptr = WIFI_RequestHasKey(&conn, "notification")))
				  WIFIHANDLER_HandleNotificationRequest(&conn, key_ptr);
			  // other GET requests code here...

			  else WIFI_SendResponse(&conn, "404 Not Found", "Unknown command", 15);
		  }

		  else if (conn.request_type == POST)
		  {
			  // other POST requests code here...
		  }

		  // other requests code here...

		  HAL_GPIO_TogglePin(STATUS_Port, STATUS_Pin);
	  }
	  else if (wifistatus != TIMEOUT)
	  {
		  sprintf(wifi.buf, "Status: %d", wifistatus);
		  WIFI_ResetComm(&wifi, &conn);
		  WIFI_SendResponse(&conn, "500 Internal server error", wifi.buf, strlen(wifi.buf));
	  }

	  WIFI_ResetConnectionIfError(&wifi, &conn, wifistatus);
	  /*uint8_t uart_buf[11];
	  GPIOB->BSRR |= GPIO_PIN_0;
	  HAL_UART_Receive(&huart1, uart_buf, 11, 10);
	  GPIOB->BRR |= GPIO_PIN_0;
	  //HAL_Delay(250);
	  if (uart_buf[0] == 0x10)
	  {
		  uart_buf[0] = 0x00;
		  uint8_t data[1] = {0x01};
		  HAL_UART_Transmit(&huart1, data, 1, HAL_MAX_DELAY);
	  }
	  else if (uart_buf[0] == 0x12)
	  {
		  uart_buf[0] = 0x00;
		  float current = SENS_GetCurrent();
		  ESP_SendRawValue(*(uint32_t*)&current, 0x12);
	  }
	  else if (uart_buf[0] == 0x13)
	  {
		  uart_buf[0] = 0x00;
		  uint32_t voltage = SENS_GetVoltage();
		  ESP_SendRawValue(voltage, 0x13);
	  }
	  else if (uart_buf[0] == 0x14)
	  {
		  uart_buf[0] = 0x00;
		  // the power value can be calculated in the esp8266, but why not doing it here?
		  uint32_t power = SENS_GetVoltage() * SENS_GetCurrent();
		  ESP_SendRawValue(power, 0x14);
	  }
	  else if (uart_buf[0] == 0x15)
	  {
		  NVIC_SystemReset();
	  }*/
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/*void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    //HAL_UART_Transmit(&huart1, UART1_rxBuffer, 12, 100);
    HAL_UART_Receive_DMA(&huart1, usart_buf, 16);
}*/

/*void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
}*/

/*void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	uint32_t maxx = 0, minn = 4096;
	// 12-bit ADC must be used
	for (uint32_t i = 0; i < ADC_BUF_LEN; i+=2)
	{
		if (adc_buf[i] > maxx)
			maxx = adc_buf[i];
		if (adc_buf[i] < minn)
			minn = adc_buf[i];
	}

	max = (float)maxx;
	min = (float)minn;
}*/

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
