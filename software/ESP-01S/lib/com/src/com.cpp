#include "com.h"
#include <WebSerial.h>

// expects 11 bytes
uint8_t sendByte(uint8_t data, uint8_t* buf)
{
    uint8_t send_buf[11];
    memset(send_buf, 0, 11);
    send_buf[0] = data;
    Serial.write(send_buf, 11);
    Serial.flush();

    uint32_t waiter = millis();
    while (Serial.available() < 11)
    {
        if (millis() - waiter > 500)
            return NORESPONSE;
    }

    Serial.read(buf, 11);
    Serial.flush();
    return buf[0];
}


void sendByteNoResponse(uint8_t data)
{
    uint8_t send_buf[10];
    memset(send_buf, 0, 10);
    send_buf[0] = data;
    Serial.write(send_buf, 10);
    Serial.flush();
}


uint8_t sendByteRetry(uint8_t data, uint8_t* buf, size_t size, uint32_t attempts, uint32_t wait)
{
    uint32_t attempt = 0;
    uint32_t last_time = 0;
    while (true)
    {
        if (millis() - last_time < wait)
            continue;

        if (attempt == wait)
            break;

        if (Serial.available())
        {
            Serial.read(buf, size);
            Serial.flush();

            if (buf[0] != 0x00)
                return buf[0];
        }

        Serial.write(data);
        Serial.flush();
        delayMicroseconds(500);

        if (Serial.available())
        {
            Serial.read(buf, size);
            Serial.flush();

            if (buf[0] != 0x00)
                return buf[0];
        }

        attempt++;
        last_time = millis();
    }

    return TIMEOUT;
}
