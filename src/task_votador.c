// libc
#include <stdio.h>
#include <string.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>

// propias
#include "config.h"

static const char *TAG = "STF_P1:task_votador";

SYSTEM_TASK(TASK_VOTADOR)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Votador running (Punto 6 - TMR con Diagnóstico)");

    task_votador_args_t* ptr_args = (task_votador_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf_vot = ptr_args->rbuf_votador;
    RingbufHandle_t* rbuf_mon = ptr_args->rbuf_monitor;
    uint16_t mask = ptr_args->mask;
    uint8_t N = ptr_args->N;

    size_t length;
    void *ptr_rx;
    void *ptr_tx;

    int i = 0;
    uint8_t faulty_sensor = 0; // 0 = todos OK, 1 = falla T1, 2 = falla T2, 3 = falla T3

    TASK_LOOP()
    {
        ptr_rx = xRingbufferReceive(*rbuf_vot, &length, pdMS_TO_TICKS(1000));

        if (ptr_rx != NULL) 
        {
            votador_msg_t msg_in = *((votador_msg_t *) ptr_rx);
            vRingbufferReturnItem(*rbuf_vot, ptr_rx);

            uint16_t A = msg_in.lsb1;
            uint16_t B = msg_in.lsb2;
            uint16_t C = msg_in.lsb3;
            i++;

            // 1. Votación lógica mayoritaria aplicando la máscara
            uint16_t R = (((A & B) | (A & C) | (B & C)) & mask);

            // 2. Comprobación del estado de los sensores cada N periodos
            if (i % N == 0)
            {
                uint16_t Am = A & mask;
                uint16_t Bm = B & mask;
                uint16_t Cm = C & mask;
                uint8_t current_st = GET_ST_FROM_TASK();

                if (Am == Bm && Bm == Cm) 
                {
                    // Caso i: Los tres sensores coinciden dentro de la máscara
                    faulty_sensor = 0;
                    if (current_st != NORMAL_MODE) {
                        SWITCH_ST_FROM_TASK(NORMAL_MODE);
                    }
                }
                else if (Am == Bm && Am != Cm)
                {
                    // Caso ii: A y B coinciden, pero C (Sensor 3) discrepa
                    faulty_sensor = 3;
                    if (current_st != WARNING_MODE) {
                        SWITCH_ST_FROM_TASK(WARNING_MODE);
                    }
                }
                else if (Am == Cm && Am != Bm)
                {
                    // Caso ii: A y C coinciden, pero B (Sensor 2) discrepa
                    faulty_sensor = 2;
                    if (current_st != WARNING_MODE) {
                        SWITCH_ST_FROM_TASK(WARNING_MODE);
                    }
                }
                else if (Bm == Cm && Bm != Am)
                {
                    // Caso ii: B y C coinciden, pero A (Sensor 1) discrepa
                    faulty_sensor = 1;
                    if (current_st != WARNING_MODE) {
                        SWITCH_ST_FROM_TASK(WARNING_MODE);
                    }
                }
                else 
                {
                    // Caso iii: Discrepancias significativas entre los tres sensores
                    if (current_st != CRITICAL_ERROR) {
                        SWITCH_ST_FROM_TASK(CRITICAL_ERROR);
                    }
                }
            }

            // 3. Envía el valor votado y el sensor defectuoso (si lo hay) a Monitor
            if (xRingbufferSendAcquire(*rbuf_mon, &ptr_tx, sizeof(monitor_msg_t), pdMS_TO_TICKS(100)) == pdTRUE)
            {
                monitor_msg_t msg_out = {
                    .voted_lsb = R,
                    .faulty_id = faulty_sensor
                };
                memcpy(ptr_tx, &msg_out, sizeof(monitor_msg_t));
                xRingbufferSendComplete(*rbuf_mon, ptr_tx);
            }
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea Votador...");
    TASK_END();
}