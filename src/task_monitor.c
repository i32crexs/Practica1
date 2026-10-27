/**********************************************************************
* FILENAME : task_monitor.c       
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
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

// freerqtos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>

// propias
#include "config.h"

static const char *TAG = "STF_P1:task_monitor";


// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Monitor running");

    task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_mon = ptr_args->rbuf_monitor; 

    size_t length;
    void *ptr;
    monitor_msg_t msg;

    // Almacena el último valor conocido de desviación entre activaciones
    float desviacion = 0.0f;

    TASK_LOOP()
    {
        ptr = xRingbufferReceive(*rbuf_mon, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) 
        {
            msg = *((monitor_msg_t *) ptr);
            vRingbufferReturnItem(*rbuf_mon, ptr);

            // Si el mensaje viene del Comprobador, guardamos la desviación actualizada
            // (En NORMAL_MODE y DEGRADED_MODE no se imprime directamente la desviación)
            if (msg.source == SRC_COMPROBADOR) 
            {
                desviacion = msg.data.deviation;
            }

            // Estructura switch sugerida en el documento según el estado actual
            switch(GET_ST_FROM_TASK())
            {
                case NORMAL_MODE:
                    if (msg.source == SRC_SENSOR) 
                    {
                        printf("NORMAL_MODE: T1 = %.2f °C, T2 = %.2f °C, T3 = %.2f °C\n",
                               msg.data.readings.sensor1,
                               msg.data.readings.sensor2,
                               msg.data.readings.sensor3);
                    }
                    break;

                case DEGRADED_MODE:
                    if (msg.source == SRC_SENSOR) 
                    {
                        float t1_min = msg.data.readings.sensor1 * (1.0f - desviacion);
                        float t1_max = msg.data.readings.sensor1 * (1.0f + desviacion);
                        float t2_min = msg.data.readings.sensor2 * (1.0f - desviacion);
                        float t2_max = msg.data.readings.sensor2 * (1.0f + desviacion);
                        float t3_min = msg.data.readings.sensor3 * (1.0f - desviacion);
                        float t3_max = msg.data.readings.sensor3 * (1.0f + desviacion);
                        printf("DEGRADED_MODE: T1 = (%.2f - %.2f) °C, "
                               "T2 = (%.2f - %.2f) °C, T3 = (%.2f - %.2f) °C\n",
                               t1_min, t1_max, t2_min, t2_max, t3_min, t3_max);
                    }
                    break;

                case ERROR:
                    printf("Sensor ERROR. Repair and restart.\n");
                    // Para salir de TASK_LOOP() y que la tarea se detenga por sí misma:
                    goto exit_monitor_loop;

                default:
                    // Puede ocurrir al entrar la primera vez desde INIT antes de transitar a NORMAL_MODE
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
            }
        } 
        else 
        {
            // Si no llegan datos (por ejemplo, cuando Sensor ya se ha parado en ERROR)
            // comprobamos igualmente si el sistema ha entrado en estado ERROR
            if (GET_ST_FROM_TASK() == ERROR) 
            {
                printf("Sensor ERROR. Repare and restart.\n");
                break; // Sale del TASK_LOOP() y se detiene por sí misma
            }
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

exit_monitor_loop:
    ESP_LOGI(TAG, "Deteniendo la tarea Monitor por sí misma...");
    TASK_END();
}