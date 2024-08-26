#ifndef CPU_H_
#define CPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <netdb.h>
#include <semaphore.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/temporal.h>
#include <commons/memory.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <readline/readline.h>
#include "../../utils/src/utils/socket.h"
#include "../../utils/src/utils/utils.h"

typedef struct {
    int nro_pagina;
    int desplazamiento;
    int tamanio;
} t_dir;


bool control_key;

t_log* logger_cpu;
t_log* logger_cpu_obligatorio;
t_config* config_cpu;

t_list* tlb;

typedef struct {
    int pid;
    int nro_pagina;
    int nro_marco;
    int ultimo_acceso;
} t_entrada_tlb;

t_dictionary* registros_cpu;

sem_t hay_interrupcion;

t_pcb* pcb_actual;

pthread_mutex_t mutex_registros;
pthread_mutex_t mutex_interrupcion_quantum;
pthread_mutex_t mutex_interrupcion_usuario;
pthread_mutex_t mutex_pcb;

int fd_memoria;
int fd_kernel_dispatch;
int fd_kernel_interrupt;
int fd_cpu_dispatch;
int fd_cpu_interrupt;

int tam_pagina;

typedef struct {
    char* IP_MEMORIA;
    char* PUERTO_MEMORIA;
    char* PUERTO_ESCUCHA_DISPATCH;
    char* PUERTO_ESCUCHA_INTERRUPT;
    int CANTIDAD_ENTRADAS_TLB;
    char* ALGORITMO_TLB;
} t_config_cpu;

t_config_cpu CONFIG_CPU;

t_config* iniciar_config(char* path);

void pedir_tam_pagina();

void atender_cpu_kernel_dispatch(void);
void atender_cpu_kernel_interrupt(void);

bool hay_una_interrupcion_quantum;
bool hay_una_interrupcion_usuario;

void incrementar_el_pc();

t_list* argumentos_instr_io(char** instruccion);

void ejecutar_proceso(t_pcb*);
char* fetch(t_pcb*);
char** decode(char*);
void execute(char**, t_pcb*);
void check_interrupt(t_pcb*);

t_entrada_tlb* devolver_entrada_tlb(int, int);

int pedir_marco_a_memoria(int, int);

int consultar_mmu(int, int, int);

void agregar_fifo_tlb(t_entrada_tlb* entrada);
void agregar_lru_tlb(t_entrada_tlb* entrada);

t_entrada_tlb* entrada_menos_usada_ultimamente();

#endif