#include <stdio.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include "config.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

void app_main(void)
{
    system_t sys_stf_p1;
    ESP_LOGI(TAG, "Starting STF_P1 TMR system (Punto 6)");
    system_create(&sys_stf_p1, SYS_NAME);

    // Registro de estados del Punto 6
    system_register_state(&sys_stf_p1, INIT);
    system_register_state(&sys_stf_p1, NORMAL_MODE);
    system_register_state(&sys_stf_p1, WARNING_MODE);
    system_register_state(&sys_stf_p1, CRITICAL_ERROR);
    system_set_default_state(&sys_stf_p1, INIT);

    system_task_t task_sensor;
    system_task_t task_votador;
    system_task_t task_monitor;

    RingbufHandle_t rbuf_votador = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);
    RingbufHandle_t rbuf_monitor = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

    task_sensor_args_t task_sensor_args = {
        .rbuf_votador = &rbuf_votador,
        .freq = 1
    };

    // NOTA SOBRE LA MÁSCARA:
    // El ADC de 12 bits devuelve valores entre 0 y 4095 (0x0000 - 0x0FFF).
    // Con 0x0E00 o 0x0F00 comparamos solo los 3 o 4 bits más altos del rango de 12 bits,
    // evitando que pequeñas variaciones térmicas normales disparen WARNING_MODE.
    task_votador_args_t task_votador_args = {
        .rbuf_votador = &rbuf_votador,
        .rbuf_monitor = &rbuf_monitor,
        .mask = 0x0F00,
        .N = 5
    };

    task_monitor_args_t task_monitor_args = {
        .rbuf_monitor = &rbuf_monitor
    };

    esp_err_t ret;

    STATE_MACHINE(sys_stf_p1) 
    {
        STATE_MACHINE_BEGIN();

        STATE(INIT)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: INIT");

            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
            {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }

            ESP_LOGI(TAG, "starting sensor task...");
            system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);

            ESP_LOGI(TAG, "starting votador task...");
            system_task_start_in_core(&sys_stf_p1, &task_votador, TASK_VOTADOR, "TASK_VOTADOR", TASK_VOTADOR_STACK_SIZE, &task_votador_args, 0, CORE0);

            ESP_LOGI(TAG, "starting monitor task...");
            system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);

            SWITCH_ST(&sys_stf_p1, NORMAL_MODE);
            STATE_END();
        }

        STATE(NORMAL_MODE)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: NORMAL_MODE");
            STATE_END();
        }

        STATE(WARNING_MODE)
        {
            STATE_BEGIN();
            ESP_LOGW(TAG, "State: WARNING_MODE (Un sensor presenta discrepancias)");
            STATE_END();
        }

        STATE(CRITICAL_ERROR)
        {
            STATE_BEGIN();
            ESP_LOGE(TAG, "State: CRITICAL_ERROR -> Deteniendo tareas Sensor y Votador...");
            system_task_stop(&sys_stf_p1, &task_sensor, TASK_SENSOR_TIMEOUT_MS);
            system_task_stop(&sys_stf_p1, &task_votador, TASK_VOTADOR_TIMEOUT_MS);
            STATE_END();
        }

        STATE_MACHINE_END();
    }
}