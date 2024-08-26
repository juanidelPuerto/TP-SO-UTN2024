#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>
#include <commons/collections/queue.h>

extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_ready_aux;
extern pthread_mutex_t mutex_cola_exec;
extern pthread_mutex_t mutex_cola_exit;
extern pthread_mutex_t mutex_kernel_io;
extern pthread_mutex_t mutex_lista_pcb;
extern pthread_mutex_t mutex_recursosXProcesos;
extern pthread_mutex_t mutex_recursos;
extern pthread_mutex_t mutex_recibir_pcb;
extern pthread_mutex_t cant_proc_multip;
extern pthread_mutex_t mutex_ignorar_signal;
extern pthread_mutex_t mutex_dicc_io;


extern t_list* lista_pcb;
extern t_list* lista_recursos_procesos; 

extern sem_t procesos_en_new;
extern sem_t procesos_en_exit;
extern sem_t procesos_en_ready;
extern sem_t planificador_activo;
extern sem_t proceso_ejecutando;
extern sem_t sem_multiprogramacion;
extern sem_t sem_planificacion_new;
extern sem_t sem_planificacion_ready;
extern sem_t sem_planificacion_exec;
extern sem_t sem_planificacion_exit;

extern t_list* COLA_NEW;
extern t_list* COLA_READY;
extern t_list* COLA_READY_AUX;
extern t_list* COLA_EXIT;
extern t_list* COLA_EXEC;

extern int fd_memoria;
extern int fd_kernel;
extern int fd_cpu_dispatch;
extern int fd_cpu_interrupt;

extern t_dictionary* interfaces_io;
extern t_dictionary* recursos;

extern int esta_activo;

#endif