//========================== Khai bao thu vien ========================

#include <Arduino.h>
#include <string.h>
#include "Wire.h"
#include <SPI.h>
#include "WiFi.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>



//========================== Cac ham su dung ========================

void DS3231_init();
void DS3231_getData();

void MQTT_initClient(char* _topic, char* _espID, PubSubClient& _mqttClient);
void MQTT_postData(uint32_t, uint32_t, int, int, int, uint32_t);

void SDcard_init();
void SDcard_getData(uint32_t, uint32_t, int, int, int, int, uint32_t, uint32_t , int, int);
void SDcardScreen_SplitStringData();
void SDcard_readFile();
void SDcard_saveDataFile(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void runProgramWithSD();

void SHT_getData();
void SHT_init();

void TFLP01_getData();
void TFLP01_init();

void Screen_init();
void Screen_saveCalibData2SDcard();
void Screen_getDisplayData();
void Screen_displayData();
void Screen_displayCalibData();

void O3_init();
void O3_getData();


//========================== Khai bao cac file code ========================

#include "./config.h"
#include "./ButtonDriver.h"
#include "./SHT85Driver.h"
#include "./TFLP01Driver.h"
#include "./DS3231Driver.h"
#include "./MQTTConnection.h"
#include "./SDcardDriver.h"
#include "./MQ131Driver.h"
#include "./NextionDriver.h"
#include "./ButtonDriver.h"


//========================== 			Tasks		========================

TaskHandle_t WIFI_SmartConfig_Handle = NULL;
TaskHandle_t Sensors_getData_Handle = NULL;
TaskHandle_t Screen_display_Handle = NULL;
TaskHandle_t MQTT_sendData_Handle = NULL;
TaskHandle_t SD_writeData_Handle = NULL;


void SmartConfig_Task(void * parameters)
{
	// check button de smartConfig
	for(;;)
	{
		if (Button_isLongPressed())
		{
			uint8_t wifi_connectTrialCount_u8 = 0;
			while (!WiFi.isSmartConfigDone() && wifi_connectTrialCount < WIFI_MAX_CONNECT_TRIAL)
			{
				Serial.println(".");
				TFT_wifiStatus = WIFI_Status_et::WIFI_SCANNING;
				myNex.writeNum("dl.wifi.val", TFT_WifiStatus);
				wifi_connectTrialCount_u8++;
			}
		}
		vTaskDelay(TASK_DELAY);
	}
}


void Wifi_checkStatus_Task(void *parameters)
{
	for(;;)
	{
		if (WiFi.status() == wl_status_t::WL_CONNECTED)
		{
			TFT_wifiStatus = WIFI_Status_et::WIFI_CONNECTED;
		}
		else
		{
			TFT_wifiStatus = WIFI_Status_et::WIFI_DISCONNECT;
		}
		vTaskDelay(WIFI_DELAY);
	}
}

void Sensors_getData_Task(void *parameters)
{
	for(;;)
	{
		#ifdef O3_SENSOR
			O3_getData();
		#endif
		
		SHT_getData();
		TFLP01_getData();
		DS3231_getData();
		vTaskDelay(TASK_DELAY);
	}
}

void Screen_display_Task(void *parameters) 
{
	for(;;)
	{
		Screen_displayData();
		vTaskDelay(TASK_DELAY);
	}
}

void MQTT_sendData_Task(void *parameters)
{
	for (;;)
	{
		MQTT_postData(TFT_humi, TFT_temp, TFT_pm1, TFT_pm25, TFT_pm10, TFT_o3_ppb);
		mqttClient.loop();

		vTaskDelay(MQTT_TASK_DELAY);
	}
}

void SD_writeData_Task(void *parameters)
{
	for(;;)
	{
		SDcard_SaveDataFile(TFT_humidity_percent, TFT_temperature_C, TFT_pm1, TFT_pm25, TFT_pm10, TFT_o3_ppb, TFT_o3_ppm, TFT_o3_ug, pm25_min, pm25_max);
		runProgramWithSD();

		vTaskDelay(SD_TASK_DELAY);
	}
}



//==========================     SETUP       ========================

void setup() {
	myNex.NextionListen();
	Serial.begin(SERIAL_DEBUG_BAUDRATE);
	pinMode(PIN_BUTTON_1, INPUT);
	Wire.begin(PIN_SDA_GPIO, PIN_SCL_GPIO, I2C_CLOCK_SPEED);
	WiFi.begin();
	WiFi.macAddress(MacAddress1);


	// khoi tao cac cam bien

	Serial.println("Check Dusty Sensor.");
	TFLP01_Init();
	delay(10);

	Serial.println("Check Temperature and Humidity Sensor.");
	SHT_Init();
	delay(10);

	Serial.println("Check RTC Module.");
	DS3231_Init();
	delay(10);

	Serial.println("Check Screen.");
	Screen_Init();

#ifdef O3_SENSOR
	Serial.println("Check Ozone Sensor.");
	O3_init();
	delay(10);
#endif

///

#ifdef USING_MQTT
	MQTT_InitClient(topic, espID, mqttClient);
	timeClient.begin();
#endif

///

#ifdef USING_SD_CARD
	Serial.println("Check SD");
	SDcard_Init();
	delay(10);
	sprintf(nameFileCalib, "/calib-%d.txt", yearCalib_u32);
#endif

///
	// core 0
	xTaskCreatePinnedToCore(SmartConfig_Task,
							"smart config",
							STACK_SIZE,
							NULL,
							1,
							&WIFI_SmartConfig_Handle,
							0 			// core 0
							);


	xTaskCreatePinnedToCore(Wifi_checkStatus_Task,
							"wifi status",
							STACK_SIZE,
							NULL,
							1,
							NULL,
							0 			// core 0
							);
	
#ifdef USING_MQTT

	xTaskCreatePinnedToCore(MQTT_sendData_Task,
							"SendDatatoMQTT",
							STACK_SIZE,
							NULL,
							1,
							&MQTT_sendData_Handle,
							0 			// core 0
							);

#endif

#ifdef USING_SD_CARD
	xTaskCreatePinnedToCore(SD_writeData_Task,
							"WtiteDatatoSD",
							STACK_SIZE,
							NULL,
							1,
							&SD_writeData_Handle,
							0			// core 0 
							);

#endif


	// core 1
	xTaskCreatePinnedToCore(Sensors_getData_Task,
							"SensorGetData",
							STACK_SIZE,
							NULL,
							1,
							&Sensors_getData_Handle,
							1 			// core 1
							);


	xTaskCreatePinnedToCore(Screen_display_Task,
							"Screen dislay",
							STACK_SIZE,
							NULL,
							1,
							&Screen_Display_Handle,
							1 			// core 1
							);


}

//==========================     LOOP       ========================

void loop()
{
	// nothing
}
