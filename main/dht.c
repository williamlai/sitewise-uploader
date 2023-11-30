#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

#include "dht.h"

static int waitUntilTimeout(gpio_num_t dht11_gpio, int usTimeout, int level, int *pUsTick)
{
    int result = DHT11_ERROR_NONE;
    int usTick = 0;

    while (gpio_get_level(dht11_gpio) == level)
    {
        esp_rom_delay_us(1);

        usTick++;
        if (usTick > usTimeout)
        {
            result = DHT11_ERROR_TIMEOUT;
            break;
        }
    }

    if (pUsTick != NULL)
    {
        *pUsTick = usTick;
    }

    return result;
}

static void sendStartSignal(gpio_num_t dht11_gpio)
{
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 0);
    esp_rom_delay_us(20 * 1000);
    gpio_set_level(dht11_gpio, 1);
    esp_rom_delay_us(40);
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
}

static int readByte(gpio_num_t dht11_gpio, uint8_t *pByte)
{
    int result = DHT11_ERROR_NONE;
    uint8_t b = 0;
    int usTick = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        if ((result = waitUntilTimeout(dht11_gpio, 50, 0, NULL)) != DHT11_ERROR_NONE)
        {
            /* nop, propagate the error */
            break;
        }

        b <<= 1;
        if ((result = waitUntilTimeout(dht11_gpio, 70, 1, &usTick)) != DHT11_ERROR_NONE)
        {
            break;
        }
        else
        {
            if (usTick > 28)
            {
                b |= 1;
            }
        }
    }

    if (result == DHT11_ERROR_NONE)
    {
        *pByte = b;
    }

    return result;
}

int DHT_read(DhtType_t type, gpio_num_t dht11_gpio, float *pTemperature, float *pHumidity)
{
    int result = DHT11_ERROR_NONE;
    uint8_t data[5] = { 0 };
    float temperature = 0.0f;
    float humidity = 0.0f;

    sendStartSignal(dht11_gpio);

    if ((result = waitUntilTimeout(dht11_gpio, 80, 0, NULL)) != DHT11_ERROR_NONE)
    {
        /* nop, propagate the error */
    }
    else if ((result = waitUntilTimeout(dht11_gpio, 80, 1, NULL)) != DHT11_ERROR_NONE)
    {
        /* nop, propagate the error */
    }
    else
    {
        for (int i = 0; i < 5; i++)
        {
            if ((result = readByte(dht11_gpio, &(data[i]))) != DHT11_ERROR_NONE)
            {
                break;
            }
        }

        /* Check CRC */
        if (data[4] != (data[0] + data[1] + data[2] + data[3]))
        {
            result = DHT11_ERROR_CRC;
        }
        else
        {
            if (type == DHT11)
            {
                humidity = ((uint16_t)data[0]) << 8 | data[1];
                humidity *= 0.1;

                temperature = data[2];
                if (data[3] & 0x80)
                {
                    temperature = -1 - temperature;
                }
                temperature += (data[3] & 0x0F) * 0.1;
            }
            else if (type == DHT22)
            {
                humidity = ((uint16_t)data[0]) << 8 | data[1];
                humidity *= 0.1;

                temperature = ((uint16_t)data[2] & 0x7F) << 8 | data[3];
                temperature *= 0.1;
                if (data[2] & 0x80)
                {
                    temperature *= -1;
                }
            }

            if (pTemperature != NULL)
            {
                *pTemperature = temperature;
            }
            if (pHumidity != NULL)
            {
                *pHumidity = humidity;
            }
        }
    }

    return result;
}
