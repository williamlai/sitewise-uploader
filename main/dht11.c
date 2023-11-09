#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

#include "dht11.h"

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

int DHT11_read(gpio_num_t dht11_gpio, int *pTemperature, int *pHumidity)
{
    int result = DHT11_ERROR_NONE;
    uint8_t data[5] = { 0 };

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
            if (pTemperature != NULL)
            {
                *pTemperature = data[2];
            }
            if (pHumidity != NULL)
            {
                *pHumidity = data[0];
            }
        }
    }

    return result;
}
