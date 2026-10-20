#ifndef __CONFIG_H__
#define __CONFIG_H__

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

// esp
#include <hal/adc_types.h>

// propias
#include "system.h"

// Abstracciones para facilitar la legibilidad
#define CORE0 0
#define CORE1 1

// Nombre y estados de la máquina
#define SYS_NAME "STF P1 System"
enum{
    INIT,
    SENSOR_LOOP
};

// Configuración del buffer cíclico
#define BUFFER_SIZE  2048
#define BUFFER_TYPE  RINGBUF_TYPE_NOSPLIT

// =========================================================================
// ESTRUCTURAS DE MENSAJES PARA LOS RINGBUFFERS (DIAGRAMA B)
// =========================================================================

// Campo inicial para discernir qué tarea envía el mensaje al Monitor
typedef enum {
    SRC_SENSOR,
    SRC_COMPROBADOR
} msg_source_t;

// Ítem que circula por el RingBuffer Monitor
typedef struct {
    msg_source_t source; // Fuente del mensaje (Sensor o Comprobador)
    float value;         // Temperatura T1 o Desviación calculada
} monitor_msg_t;

// Ítem que circula por el RingBuffer Comprobador
typedef struct {
    float t1;            // Lectura del termistor principal
    float t2;            // Lectura del termistor réplica
} comprobador_msg_t;

// =========================================================================
// CONFIGURACIÓN DE LAS TAREAS
// =========================================================================

// 1. TAREA SENSOR
SYSTEM_TASK(TASK_SENSOR);
typedef struct 
{
    RingbufHandle_t* rbuf_monitor;     // Puntero al RingBuffer Monitor
    RingbufHandle_t* rbuf_comprobador; // Puntero al RingBuffer Comprobador
    uint8_t freq;                      // Frecuencia de muestreo (1/Ts)
    uint8_t N;                         // Nº de periodos para activar el comprobador
} task_sensor_args_t;

#define TASK_SENSOR_TIMEOUT_MS 2000 
#define TASK_SENSOR_STACK_SIZE 4096

// 2. TAREA COMPROBADOR
SYSTEM_TASK(TASK_COMPROBADOR);
typedef struct 
{
    RingbufHandle_t* rbuf_comprobador; // Buffer de entrada (desde Sensor)
    RingbufHandle_t* rbuf_monitor;     // Buffer de salida (hacia Monitor)
} task_comprobador_args_t;
#define TASK_COMPROBADOR_TIMEOUT_MS 2000 
#define TASK_COMPROBADOR_STACK_SIZE 4096

// 3. TAREA MONITOR
SYSTEM_TASK(TASK_MONITOR);
typedef struct 
{
    RingbufHandle_t* rbuf_monitor;     // Puntero al RingBuffer Monitor
} task_monitor_args_t;

#define TASK_MONITOR_TIMEOUT_MS 2000 
#define TASK_MONITOR_STACK_SIZE 4096

#endif