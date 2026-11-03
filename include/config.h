#ifndef __CONFIG_H__
#define __CONFIG_H__

// libc
#include <stdint.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

// esp
#include <hal/adc_types.h>

// propias
#include "system.h"
#include "therm.h"

// Macro para obtener el estado actual de la máquina desde una tarea
#ifndef GET_ST_FROM_TASK
#define GET_ST_FROM_TASK() __task->system->sys_state
#endif

// Abstracciones para facilitar la legibilidad
#define CORE0 0
#define CORE1 1

// Nombre y estados de la máquina (compatibles con Parte 2 y Parte 3)
#define SYS_NAME "STF P1 System"
enum{
    INIT,
    SENSOR_LOOP,
    NORMAL_MODE,
    DEGRADED_MODE,
    WARNING_MODE,
    ERROR,
    CRITICAL_ERROR
};

// Configuración por defecto del ADC
#define THERMISTOR_ADC_UNIT    ADC_UNIT_1
#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_6

// Configuración del buffer cíclico
#define BUFFER_SIZE  2048
#define BUFFER_TYPE  RINGBUF_TYPE_NOSPLIT

// =========================================================================
// ESTRUCTURAS DE DATOS PARA LOS RINGBUFFERS
// =========================================================================

// Fuente del mensaje (usado por task_comprobador.c de la Parte 2)
typedef enum {
    SRC_SENSOR,
    SRC_COMPROBADOR
} msg_source_t;

// Estructura que envía Sensor -> Votador (Puntos 4, 5 y 6: valores en binario crudo)
typedef struct {
    uint16_t lsb1;
    uint16_t lsb2;
    uint16_t lsb3;
} votador_msg_t;

// Estructura para el RingBuffer del Monitor (compatible con Parte 2 y Parte 3)
typedef struct {
    msg_source_t source;  // Compatibilidad Parte 2
    float value;          // Compatibilidad Parte 2
    uint16_t voted_lsb;   // Resultado binario votado (Puntos 4, 5 y 6)
    uint8_t faulty_id;    // 0: ninguno, 1: Sensor 1, 2: Sensor 2, 3: Sensor 3 (Punto 6)
} monitor_msg_t;

// Estructura de compatibilidad para task_comprobador.c
typedef struct {
    float t1;
    float t2;
} comprobador_msg_t;

// =========================================================================
// CONFIGURACIÓN Y ARGUMENTOS DE LAS TAREAS
// =========================================================================

// 1. TAREA SENSOR
SYSTEM_TASK(TASK_SENSOR);
typedef struct 
{
    RingbufHandle_t* rbuf;             // Puntero genérico
    RingbufHandle_t* rbuf_votador;     // Buffer hacia Votador (Parte 3)
    RingbufHandle_t* rbuf_monitor;     // Buffer hacia Monitor (Parte 2)
    RingbufHandle_t* rbuf_comprobador; // Buffer hacia Comprobador (Parte 2)
    uint8_t freq;                      // Frecuencia de muestreo
    uint8_t N;                         // Periodos de activación
} task_sensor_args_t;
#define TASK_SENSOR_TIMEOUT_MS 2000 
#define TASK_SENSOR_STACK_SIZE 4096

// 2. TAREA VOTADOR
SYSTEM_TASK(TASK_VOTADOR);
typedef struct 
{
    RingbufHandle_t* rbuf_votador;     // Buffer de entrada (desde Sensor)
    RingbufHandle_t* rbuf_monitor;     // Buffer de salida (hacia Monitor)
    uint16_t mask;                     // Máscara binaria (Puntos 5 y 6)
    uint8_t N;                         // Periodos de comprobación de fallos (Punto 6)
} task_votador_args_t;
#define TASK_VOTADOR_TIMEOUT_MS 2000 
#define TASK_VOTADOR_STACK_SIZE 4096

// 3. TAREA COMPROBADOR (Mantenida para que task_comprobador.c compile sin error)
SYSTEM_TASK(TASK_COMPROBADOR);
typedef struct 
{
    RingbufHandle_t* rbuf_comprobador;
    RingbufHandle_t* rbuf_monitor;
} task_comprobador_args_t;
#define TASK_COMPROBADOR_TIMEOUT_MS 2000 
#define TASK_COMPROBADOR_STACK_SIZE 4096

// 4. TAREA MONITOR
SYSTEM_TASK(TASK_MONITOR);
typedef struct 
{
    RingbufHandle_t* rbuf;             // Puntero genérico
    RingbufHandle_t* rbuf_monitor;     // Buffer de entrada (desde Votador)
} task_monitor_args_t;
#define TASK_MONITOR_TIMEOUT_MS 2000 
#define TASK_MONITOR_STACK_SIZE 4096

#endif