#ifndef __THERM_H__
#define __THERM_H__

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#include "driver/gpio.h"

#define SERIES_RESISTANCE 10000 // 10K ohms
#define NOMINAL_RESISTANCE 10000 // 10K ohms
#define NOMINAL_TEMPERATURE 298.15 // 25°C en Kelvin
#define BETA_COEFFICIENT 3950 // Constante B

// Tabla de conversion de GPIO a adc channel
esp_err_t get_adc_info_from_gpio(int gpio_num, adc_unit_t *unit, adc_channel_t *channel);

typedef struct therm_conf_t
{
    adc_oneshot_unit_handle_t adc_hdlr;
    adc_channel_t adc_channel;
    gpio_num_t power_pin;

} therm_t;

// funciones
esp_err_t therm_config(therm_t* thermistor,
                       int gpio,
                       gpio_num_t power_pin);

float therm_read_t(therm_t thermistor);
float therm_read_v(therm_t thermistor);
uint16_t therm_read_lsb(therm_t thermistor);

// funciones útiles de conversión
float _therm_v2t(float v);
float _therm_lsb2v(uint16_t lsb);

esp_err_t therm_power_on(therm_t thermistor);

esp_err_t therm_power_off(therm_t thermistor);

#endif