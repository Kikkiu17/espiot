#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <WebSerial.h>
#include <BlockNot.h>

#include "com.h"
// CALIBRATE DEVICE HERE
#include "calibrationvalues.h"

const char* ssid = "xxxxxxxxxxxxxx";
const char* password = "xxxxxxxxxxxxxxxxxxx";

// switch;sensor:power,current
// sensor:power,voltage,current
const String capabilities = "sensor:power,voltage,current";
//const String capabilities = "sensor:power,current";
const String id = "ESPDEVICE002";

AsyncWebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success) {
  }
  // <Add your own code here>
}

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

uint32_t bufferToNumber(uint8_t *buf, size_t size)
{
  uint32_t number = 0;
  for (uint32_t i = 1; i < size; i++)
    number = number * 10 + buf[i];
  return number;
}

float current = 0.00;
uint32_t voltage = 0;
uint32_t power = 0.00;
char send_buf[4];
String own_name = "NAME_NOT_SET";

// ADD HERE CAPABILITIES RETURN
void handleSensorParams(AsyncWebServerRequest *req)
{
  String response;
  bool found = false;
  for (uint32_t i = 0; i < req->params(); i++)
  {
    AsyncWebParameter *p = req->getParam(i);

    if (p->name() == "current" && p->value() == "1")
    {
      found = true;
      if (response != "")
        response += ";";
      response += "current:" + String(current);
      continue;
    }

    if (p->name() == "voltage" && p->value() == "1")
    {
      found = true;
      if (response != "")
        response += ";";
      response += "voltage:" + String(voltage);
      continue;
    }

    if (p->name() == "power" && p->value() == "1")
    {
      found = true;
      if (response != "")
        response += ";";
      response += "power:" + String(power);
      continue;
    }
  }

  if (!found)
    req->send(404, "text/plain", "Not Found");
  else
    req->send(200, "text/plain", response);

  return;
}

void setup(void)
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(id);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  server.on("/capabilities", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", capabilities);
  });

  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    handleSensorParams(request);
  });

  server.on("/name", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    if (request->params() == 0)
      request->send(400, "text/plain", "Bad Request");

    AsyncWebParameter *p = request->getParam(0);

    if (p->name() == "set" && p->value() != "")
    {
      own_name = p->value();
      request->send(200, "text/plain", "Name set");
    }
    else
      request->send(400, "text/plain", "Bad Request");
  });

  server.on("/name", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", own_name);
  });

  server.on("/id", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", id);
  });

  server.on("/ip", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", WiFi.localIP().toString());
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  ElegantOTA.setAutoReboot(true);
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  WebSerial.begin(&server);

  server.begin();
  sendByteNoResponse(STM_RESET);
  Serial.flush();
}

uint8_t buf[11];
bool startup = false;
uint32_t timer = 0;

void loop(void)
{
  ElegantOTA.loop();

  if (millis() - timer > 250)
  {
    memset(buf, 0, 11);
    timer = millis();
    if (!startup)
    {
      if (sendByte(STARTUP, buf) != NORESPONSE)
      {
        if (buf[0] == OK)
        {
          sendByte(READY, buf);
          startup = true;
        }
        else if (buf[0] == FATAL_ERROR)
          while (true) {}
      }
    }

    if (startup)
    {
      if (sendByte(REQUEST_CURRENT, buf) == REQUEST_CURRENT)
      {
        uint32_t raw_number = bufferToNumber(buf, 11);
        current = roundf(*reinterpret_cast<float*>(&raw_number) * 100) / 100 - amps_subtracted;
        if (current < amps_deadzone)
          current = 0;
      }

      if (sendByte(REQUEST_VOLTAGE, buf) == REQUEST_VOLTAGE)
      {
        voltage = bufferToNumber(buf, 11) - volts_subtracted;
      }

      if (sendByte(REQUEST_POWER, buf) == REQUEST_POWER)
      {
        //power = voltage * current;
        power = bufferToNumber(buf, 11);
        // brings the power to zero to account for software calibration
        /*if (power > current * voltage && amps_deadzone != 0)
         power = 0;*/
      }
    }
  }
}
