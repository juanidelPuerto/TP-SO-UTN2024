#ifndef CONFIG_KERNEL_H_
#define CONFIG_KERNEL_H_

#include <stdint.h>

typedef enum {
    FIFO,
    RR,
    VRR
} t_algoritmo;

typedef struct {
    char* PUERTO_ESCUCHA;
    char* IP_MEMORIA;
    char* PUERTO_MEMORIA;
    char* IP_CPU;
    char* PUERTO_CPU_DISPATCH;
    char* PUERTO_CPU_INTERRUPT;
    t_algoritmo ALGORITMO_PLANIFICACION;
    int QUANTUM;
    int GRADO_MULTIPROGRAMACION;
    char** RECURSOS;
    char** INSTANCIAS_RECURSOS;
} t_config_kernel;

extern t_config_kernel* KERNEL_CONFIG;

#endif