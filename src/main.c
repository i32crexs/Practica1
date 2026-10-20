/******************************************************************************
* FILENAME : main.c
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

// libc
#include <stdio.h>
#include <assert.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

// esp-idf
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

// propias
#include "config.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

void app_main(void)
{
    system_t sys_stf_p1;
    ESP_LOGI(TAG, "Starting STF_P1 system");
    system_create(&sys_stf_p1, SYS_NAME);
    
    // Registro de todos los estados de la máquina resiliente
    system_register_state(&sys_stf_p1, INIT);
    system_register_state(&sys_stf_p1, NORMAL_MODE);
    system_register_state(&sys_stf_p1, DEGRADED_MODE);
    system_register_state(&sys_stf_p1, ERROR);
    system_set_default_state(&sys_stf_p1, INIT);

    // Manejadores de las 3 tareas
    system_task_t task_sensor;
    system_task_t task_comprobador;
    system_task_t task_monitor;

    // Creación de los dos RingBuffers
    RingbufHandle_t rbuf_monitor = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);
    RingbufHandle_t rbuf_comprobador = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

    // Argumentos de las tareas
    task_sensor_args_t task_sensor_args = {
        .rbuf_monitor = &rbuf_monitor,
        .rbuf_comprobador = &rbuf_comprobador,
        .freq = 1, // Ts = 1 segundo
        .N = 5     // Comprueba cada 5 periodos
    };
    task_comprobador_args_t task_comprobador_args = {
        .rbuf_comprobador = &rbuf_comprobador,
        .rbuf_monitor = &rbuf_monitor
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

            // Lanza las tareas Sensor, Comprobador y Monitor
            ESP_LOGI(TAG, "starting sensor task...");
            system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);

            ESP_LOGI(TAG, "starting comprobador task...");
            system_task_start_in_core(&sys_stf_p1, &task_comprobador, TASK_COMPROBADOR, "TASK_COMPROBADOR", TASK_COMPROBADOR_STACK_SIZE, &task_comprobador_args, 0, CORE0);

            ESP_LOGI(TAG, "starting monitor task...");
            system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);

            // Al finalizar la configuración, transiciona a NORMAL_MODE
            SWITCH_ST(&sys_stf_p1, NORMAL_MODE);
            STATE_END();
        }

        STATE(NORMAL_MODE)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: NORMAL_MODE");
            STATE_END();
        }

        STATE(DEGRADED_MODE)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: DEGRADED_MODE");
            STATE_END();
        }

        STATE(ERROR)
        {
            STATE_BEGIN();
            ESP_LOGE(TAG, "State: ERROR -> Deteniendo tareas Sensor y Comprobador...");

            // Detiene de forma controlada las tareas Sensor y Comprobador (Monitor se detiene por sí misma)
            system_task_stop(&sys_stf_p1, &task_sensor, TASK_SENSOR_TIMEOUT_MS);
            system_task_stop(&sys_stf_p1, &task_comprobador, TASK_COMPROBADOR_TIMEOUT_MS);

            STATE_END();
        }

        STATE_MACHINE_END();
    }
}