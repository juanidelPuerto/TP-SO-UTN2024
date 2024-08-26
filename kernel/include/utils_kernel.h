#ifndef UTILS_KERNEL_H_
#define UTILS_KERNEL_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/temporal.h>
#include <readline/readline.h>
#include "consola.h"
#include "global.h"
#include "logs_kernel.h"
#include "config_kernel.h"
#include "../../utils/src/utils/utils.h"
#include "../../utils/src/utils/socket.h"

typedef struct {
    char* nombre_recurso;
    int pid;
} t_recurso_proceso;

// Agregar al diccionario recursos la lista de procesos que hicieron wait

typedef struct {
    int socket;
    char* nombre;
} t_args_manejar_io;

typedef struct {
    int termino_el_proceso;
    pthread_mutex_t mutex_quantum;
    int quantum_restante;
    int pid;
} t_args_hilo_quantum;

typedef struct {
    t_list* cola_bloq;
    sem_t* sem_cola_bloq;
} t_cola_bloq;

void iniciar_proceso(char*);
void finalizar_proceso(char*);
void iniciar_planificacion(char*);
void detener_planificacion(char*);
void planificador_largo_plazo();
void iniciar_planificador();
void planificador_corto_plazo_fifo();
void planificador_corto_plazo_rr();
void ejecutar_fifo(t_pcb*);
void ejecutar_rr(t_pcb*);
void manejar_proceso(t_pcb*);
void manejar_recurso(t_pcb* pcb, t_recurso* recurso);
void ejecutar_segun_algoritmo(t_pcb* pcb);
//
t_pcb* obtener_pcb_vrr();
bool comparar_quantum_restante(t_pcb* pcb1, t_pcb* pcb2); 
void ejecutar_vrr(t_pcb* pcb); 
void iniciar_quantum_vrr(void* _args);
void planificador_corto_plazo_vrr();
// Funciones de consola
void proceso_estado();
//const char* estado_to_string(t_estado* estado);
t_list *leer_instrucciones(char *path);
//void ejecutar_script(char *path);
void multiprogramacion(char *inst);
//
int manejar_signal(t_pcb* pcb, t_recurso* recurso);
void manejar_new();
void manejar_exit();

void esperar_io();
void manejar_io(void*);
bool ya_existe_esa_interfaz(char*);

t_pcb* eliminar_primer_elemento(t_list* cola);

t_pcb* pop_cola_new();
t_pcb* pop_cola_ready();
t_pcb* pop_cola_ready_aux();
t_pcb* pop_cola_exec();
t_pcb* pop_cola_exit();
t_pcb* pop_cola_bloq(t_list* cola);
void push_cola_new(t_pcb*);
void push_cola_ready(t_pcb*);
void push_cola_ready_aux(t_pcb*);
void push_cola_exec(t_pcb*);
void push_cola_exit(t_pcb*);
void push_cola_bloq_io(char*, t_pcb*);
void push_cola_bloq_rec(t_list* cola, t_pcb* pcb);

void buscar_proceso_y_eliminar(int pid);
void remover_proceso_en_cola(t_list* cola, int pid);
bool eliminar_de_lista_por_pid(t_list* lista, int pid);
t_pcb* buscar_proceso(int pid);

#endif