// libc
#include <stdio.h>
#include <string.h>
#include <math.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>

// propias
#include "config.h"

static const char *TAG = "STF_P1:task_comprobador";

// Tarea COMPROBADOR
SYSTEM_TASK(TASK_COMPROBADOR)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Comprobador running");

    // Desempaqueta los argumentos
    task_comprobador_args_t* ptr_args = (task_comprobador_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_comp = ptr_args->rbuf_comprobador;
    RingbufHandle_t* rbuf_mon = ptr_args->rbuf_monitor;

    size_t length;
    void *ptr_rx;
    void *ptr_tx;

    TASK_LOOP()
    {
        // 1. Espera a recibir T1 y T2 desde RingBuffer Comprobador
        ptr_rx = xRingbufferReceive(*rbuf_comp, &length, pdMS_TO_TICKS(1000));

        if (ptr_rx != NULL)
        {
            comprobador_msg_t msg_in = *((comprobador_msg_t *) ptr_rx);
            vRingbufferReturnItem(*rbuf_comp, ptr_rx);

            // 2. Calcula la desviación relativa respecto a T1
            float desv = 0.0f;
            if (msg_in.t1 != 0.0f) {
                desv = fabs((msg_in.t1 - msg_in.t2) / msg_in.t1);
            }

            // 3. Envía la desviación al RingBuffer Monitor
            if (xRingbufferSendAcquire(*rbuf_mon, &ptr_tx, sizeof(monitor_msg_t), pdMS_TO_TICKS(100)) != pdTRUE)
            {
                ESP_LOGW(TAG, "Buffer Monitor lleno al enviar desviación");
            }
            else
            {
                monitor_msg_t msg_out = {
                    .source = SRC_COMPROBADOR,
                    .value = desv
                };
                memcpy(ptr_tx, &msg_out, sizeof(monitor_msg_t));
                xRingbufferSendComplete(*rbuf_mon, ptr_tx);
            }
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea Comprobador...");
    TASK_END();
}