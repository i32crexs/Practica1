// libc 
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// espidf
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>

// propias

#include "therm.h"

// funciones
esp_err_t get_adc_info_from_gpio(int gpio_num, adc_unit_t *unit, adc_channel_t *channel) {
    switch(gpio_num) {
        // ADC 1
        case 36: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_0; return ESP_OK;
        case 39: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_3; return ESP_OK;
        case 32: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_4; return ESP_OK;
        case 33: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_5; return ESP_OK;
        case 34: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_6; return ESP_OK;
        case 35: *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_7; return ESP_OK;
        
        // ADC 2
        case 4:  *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_0; return ESP_OK;
        case 0:  *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_1; return ESP_OK;
        case 2:  *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_2; return ESP_OK;
        case 15: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_3; return ESP_OK;
        case 13: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_4; return ESP_OK;
        case 12: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_5; return ESP_OK;
        case 14: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_6; return ESP_OK;
        case 27: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_7; return ESP_OK;
        case 25: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_8; return ESP_OK;
        case 26: *unit = ADC_UNIT_2; *channel = ADC_CHANNEL_9; return ESP_OK;
        
        // Pin no válido para ADC en este mapeo
        default: return ESP_ERR_NOT_FOUND;
    }
}



// esp_err_t therm_config(therm_t* thermistor,
//                        adc_oneshot_unit_handle_t adc,
//                        adc_channel_t channel)
// {
//     thermistor->adc_channel=channel;
//     thermistor->adc_hdlr=adc;
//     return ESP_OK;
// }

esp_err_t therm_config(therm_t* thermistor,
                       int gpio,
                       gpio_num_t power_pin)
{
    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t ret_v;
    adc_oneshot_unit_handle_t hdlr;
    ret_v = get_adc_info_from_gpio(gpio, &unit, &channel);

	// Estructura de datos para configurar la unidad ADC
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = unit,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
	// Establecimiento de la configuración en ADC
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &hdlr));

	// Estructura de datos para configurar el canal de la unidad ADC
    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11, // Rango de 0 a 3.3V
        .bitwidth = ADC_BITWIDTH_12, // Resolución de 12 bits
    };
	// Establecimiento de la configuración del canal
    ESP_ERROR_CHECK(adc_oneshot_config_channel(hdlr, channel, &channel_cfg));

    thermistor->adc_channel=channel;
    thermistor->adc_hdlr=hdlr;
    thermistor->power_pin=power_pin;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << power_pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    
    // Por defecto, lo dejamos apagado para ahorrar energía y desgaste
    gpio_set_level(power_pin, 0);

    return ret_v;
}

float therm_read_t(therm_t thermistor)
{
    float ret_v, therm_v;
    uint16_t lsb;

    lsb = therm_read_lsb(thermistor);

    therm_v = _therm_lsb2v(lsb);

    ret_v = _therm_v2t(therm_v);

    return ret_v;
}


float therm_read_v(therm_t thermistor)
{
    float ret_v;
    uint16_t lsb;

    lsb = therm_read_lsb(thermistor);

    ret_v = _therm_lsb2v(lsb);

    return ret_v;
}

uint16_t therm_read_lsb(therm_t thermistor)
{
    uint16_t ret_v = 0;

    therm_power_on(thermistor);

    ESP_ERROR_CHECK(adc_oneshot_read(thermistor.adc_hdlr, thermistor.adc_channel, (int*)&ret_v));

    therm_power_off(thermistor);

    return ret_v;
}

// funciones útiles de conversión
float _therm_v2t(float v)
{
    float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;
    float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));

    return (t_kelvin-273.15f);
}


float _therm_lsb2v(uint16_t lsb)
{
    float ret_v;

    ret_v = ((float) lsb * 3.3f / 4095.0f);

    return ret_v;
}


esp_err_t therm_power_on(therm_t thermistor) {
    return gpio_set_level(thermistor.power_pin, 1);
}

esp_err_t therm_power_off(therm_t thermistor) {
    return gpio_set_level(thermistor.power_pin, 0);
}
