/**********************************************************************
* FILENAME : task_sensor.c       
*
* DESCRIPTION : 
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/


/*
	Circuito del termistor 1. 

   3.3V
     |
     |
  [ NTC ]  <-- Termistor 10K 
     |
     |-----------> ADC IN (GPIO34 por defecto. Ver config.h)
     |
  [ 10K ]  <-- R fija 10K
     |
    GND
**/

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
#include "config.h"
#include "therm.h"

static const char *TAG = "STF_P1:task_sensor";

// esta tarea temporiza la lectura de un termistor. 
// Para implementar el periodo de muestreo, se utiliza un semáforo declarado de forma global 
// para poder ser utilizado desde esta rutina de expiración del timer.
// Cada vez que el temporizador expira, libera el semáforo
// para que la tarea realice una iteración. 
static SemaphoreHandle_t semSample = NULL;
static void tmrSampleCallback(void* arg)
{
	xSemaphoreGive(semSample);
}

// ESP32 tiene unidad de conversión analógica digital, que debe configurarse 
// previamente a las lecturas mediante esta estructura manejador. Se define global
// porque únicamente usaremos un ADC. Se configura en la tarea antes del loop
// adc_oneshot_unit_handle_t adc_hdlr; 


// Funciones de lectura del sensor.

// Se trata de un termistor, un sensor analógico activo (require alimentación)
// cuya salida es voltaje. El circuito está en la cabecera de este fichero. 
//
// Pasos para adquirir un valor: 
// 1) El ADC del SoC devuelve un valor binario crudo (LSB), que corresponde a un valor
// de voltaje en la entrada del canal.

// 2) Convertir este valor en voltaje. Para esto, es necesario conocer el número 
// de bits que tiene el ADC (para conocer el máximo valor binario que
// se puede obtener) y el voltaje de referencia (el máximo voltaje que es 
// capaz de medir)
/*
// LSB crudo
int read_adc() 
{
    int raw_value = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_hdlr, THERMISTOR_ADC_CHANNEL, &raw_value));
    return raw_value;
}

// LSB -> V
// 12 bits -> 2^12 = 4096 -> rango de 0 a 4095. 
// V de referencia interno del ESP32 -> 3.3V
#define lsb_to_v(x) (float) ((x) * 3.3f / 4095.0f)

// V -> Temperatura en C
float v_to_temperature(float v)
{
	// resistencia del termistor, obtenida por el voltaje medido en el adc. 
    float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;

	// Ecuación de Steinhart-Hart, que relaciona la resistencia que ofrece un material semiconductor 
	// con la variación de la temperatura en Kelvin, de acuerdo a unos coeficientes que caracterizan
	// al semiconductor en cuestión (están definidos en config.h) 
	float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));

	// Resultado en grados centígrados
    return(t_kelvin - 273.15f);  
}
*/
// Tarea SENSOR
// Tarea SENSOR
SYSTEM_TASK(TASK_SENSOR)
{   
    TASK_BEGIN();
    ESP_LOGI(TAG,"Task Sensor running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_mon = ptr_args->rbuf_monitor;
    RingbufHandle_t* rbuf_comp = ptr_args->rbuf_comprobador;
    uint8_t frequency = ptr_args->freq;
    uint8_t N = ptr_args->N;
    uint64_t period_us = 1000000 / frequency;

    // Inicializa el semáforo y el temporizador periódico
    semSample = xSemaphoreCreateBinary();
    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .name = "Timer Configuration"
    };
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));
    
    // Variables para reutilizar en el bucle
    void *ptr;
    float v, v2;
    int i = 0; // Contador de periodos según Diagrama B

    therm_t thermistor;
    therm_t t2;

    therm_config(&thermistor, 35, 15);
    therm_config(&t2, 26, 14);

    // Encendemos el termistor principal T1 y aseguramos que T2 empiece apagado
    // therm_power_on(thermistor);
    // therm_power_off(t2);

    // Loop (sin while(1), usando la macro de system.h)
    TASK_LOOP()
    {
        if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
        {   
            // 1. Lee T1 e incrementa i
            v = therm_read_t(thermistor);
            i++;

            // 2. Envía T1 a tarea Monitor a través de rbuf_mon
            if (xRingbufferSendAcquire(*rbuf_mon, &ptr, sizeof(monitor_msg_t), pdMS_TO_TICKS(100)) != pdTRUE)
            {
                ESP_LOGW(TAG,"Buffer Monitor lleno. Espacio: %d", xRingbufferGetCurFreeSize(*rbuf_mon));
            }
            else 
            {
                monitor_msg_t msg_mon = {
                    .source = SRC_SENSOR,
                    .value = v
                };
                memcpy(ptr, &msg_mon, sizeof(monitor_msg_t));
                xRingbufferSendComplete(*rbuf_mon, ptr);
            }

            // 3. Comprueba si toca activar el termistor réplica (i % N == 0)
            if (i % N == 0)
            {
                // T2 se enciende por la marcha cuando lo leemos
                // therm_power_on(t2);
                // vTaskDelay(pdMS_TO_TICKS(10)); // Breve estabilización eléctrica
                v2 = therm_read_t(t2);
                // therm_power_off(t2);

                ESP_LOGI(TAG, "Comprobación periódica (i=%d) -> T1: %.2f, T2: %.2f", i, v, v2);

                // Envía T1 y T2 a tarea Comprobador a través de rbuf_comp
                if (xRingbufferSendAcquire(*rbuf_comp, &ptr, sizeof(comprobador_msg_t), pdMS_TO_TICKS(100)) != pdTRUE)
                {
                    ESP_LOGW(TAG,"Buffer Comprobador lleno. Espacio: %d", xRingbufferGetCurFreeSize(*rbuf_comp));
                }
                else
                {
                    comprobador_msg_t msg_comp = {
                        .t1 = v,
                        .t2 = v2
                    };
                    memcpy(ptr, &msg_comp, sizeof(comprobador_msg_t));
                    xRingbufferSendComplete(*rbuf_comp, ptr);
                }
            }
        }
        else
        {
            ESP_LOGE(TAG,"Watchdog (soft) failed");
            esp_restart();
        }
    }
    
    ESP_LOGI(TAG,"Deteniendo la tarea Sensor...");
    // therm_power_off(thermistor);
    // therm_power_off(t2);
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    TASK_END();
}