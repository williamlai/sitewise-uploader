#ifndef _DHT11_H_
#define _DHT11_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#define DHT11_ERROR_NONE        (0)
#define DHT11_ERROR_TIMEOUT     (-1)
#define DHT11_ERROR_CRC         (-2)

/**
 * Do the one-wire protocol on GPIO dht11_gpio, get the results of temperature and humidity.
 * 
 * @param[in] dht11_gpio GPIO number for reading data from DHT11
 * @param[out] pTemperature Pointer to store the temperature
 * @param[out] pHumidity Pointer to store the humidity
 * @return 0 on success, non-zero value otherwise
 */
int DHT11_read(gpio_num_t dht11_gpio, int *pTemperature, int *pHumidity);

#ifdef __cplusplus
}
#endif

#endif /* _DHT11_H_ */