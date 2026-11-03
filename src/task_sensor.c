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

static SemaphoreHandle_t semSample = NULL;
static void tmrSampleCallback(void* arg)
{
    xSemaphoreGive(semSample);
}

SYSTEM_TASK(TASK_SENSOR)
{   
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    // Desempaqueta los argumentos de configuración
    task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_vot = ptr_args->rbuf_votador; 
    uint8_t frequency = ptr_args->freq;
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
    
    void *ptr;
    therm_t t1, t2, t3;

    // Configuración de los 3 termistores (ajusta los pines a tu protoboard si es necesario)
    therm_config(&t1, 35, 15);
    therm_config(&t2, 26, 14);
    therm_config(&t3, 25, 19);

    TASK_LOOP()
    {
        if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
        {   
            // Lectura en binario crudo (uint16_t) de los 3 termistores
            votador_msg_t msg;
            msg.lsb1 = therm_read_lsb(t1);
            msg.lsb2 = therm_read_lsb(t2);
            msg.lsb3 = therm_read_lsb(t3);

            ESP_LOGI(TAG, "LSB medidos -> T1: %u | T2: %u | T3: %u", msg.lsb1, msg.lsb2, msg.lsb3);

            // Envía la estructura con los 3 valores crudos al RingBuffer del Votador
            if (xRingbufferSendAcquire(*rbuf_vot, &ptr, sizeof(votador_msg_t), pdMS_TO_TICKS(100)) != pdTRUE)
            {
                ESP_LOGW(TAG, "Buffer Votador lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf_vot));
            }
            else 
            {
                memcpy(ptr, &msg, sizeof(votador_msg_t));
                xRingbufferSendComplete(*rbuf_vot, ptr);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Watchdog (soft) failed");
            esp_restart();
        }
    }
    
    ESP_LOGI(TAG, "Deteniendo la tarea Sensor...");
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    TASK_END();
}