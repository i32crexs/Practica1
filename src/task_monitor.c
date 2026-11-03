// libc
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>

// propias
#include "config.h"
#include "therm.h"

static const char *TAG = "STF_P1:task_monitor";

SYSTEM_TASK(TASK_MONITOR)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Monitor running (Punto 6)");

    task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_mon = ptr_args->rbuf_monitor; 

    size_t length;
    void *ptr;
    monitor_msg_t msg;

    TASK_LOOP()
    {
        ptr = xRingbufferReceive(*rbuf_mon, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) 
        {
            msg = *((monitor_msg_t *) ptr);
            vRingbufferReturnItem(*rbuf_mon, ptr);

            // Calibración a grados centígrados
            float temp_c = _therm_v2t(_therm_lsb2v(msg.voted_lsb));

            switch(GET_ST_FROM_TASK())
            {
                case NORMAL_MODE:
                    printf("NORMAL_MODE: T = %.2f °C\n", temp_c);
                    break;

                case WARNING_MODE:
                    printf("WARNING_MODE: T = %.2f °C | AVISO: Sustituir Sensor %u por discrepancia.\n", 
                           temp_c, msg.faulty_id);
                    break;

                case CRITICAL_ERROR:
                    printf("CRITICAL_ERROR: Discrepancia significativa entre los 3 sensores. Deteniendo sistema...\n");
                    goto exit_monitor_loop;

                default:
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
            }
        } 
        else 
        {
            if (GET_ST_FROM_TASK() == CRITICAL_ERROR) 
            {
                printf("CRITICAL_ERROR: Discrepancia significativa entre los 3 sensores. Deteniendo sistema...\n");
                break;
            }
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

exit_monitor_loop:
    ESP_LOGI(TAG, "Deteniendo la tarea Monitor por sí misma...");
    TASK_END();
}