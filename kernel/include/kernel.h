#ifndef KERNEL_H_
#define KERNEL_H_

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include<readline/readline.h>
#include "config_kernel.h"
#include "inicializar.h"
#include "logs_kernel.h"
#include "global.h"
#include "utils_kernel.h"
#include "consola.h"

t_log* logger_kernel;
t_log* logger_kernel_obligatorio;
t_config* config_kernel;
t_config_kernel* KERNEL_CONFIG;

t_dictionary* recursos;
t_dictionary* interfaces_io;

int esta_activo;

t_list* COLA_NEW;
t_list* COLA_READY;
t_list* COLA_EXIT;
t_list* COLA_EXEC;
t_list* COLA_READY_AUX;

t_list* lista_pcb;
t_list* lista_recursos_procesos; 
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_ready_aux;
pthread_mutex_t mutex_cola_exec;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t mutex_kernel_io;
pthread_mutex_t mutex_lista_pcb;
pthread_mutex_t mutex_recursosXProcesos;
pthread_mutex_t mutex_recursos;
pthread_mutex_t mutex_recibir_pcb;
pthread_mutex_t mutex_ignorar_signal;
pthread_mutex_t cant_proc_multip;
pthread_mutex_t mutex_dicc_io;



sem_t procesos_en_new;
sem_t procesos_en_ready;
sem_t procesos_en_exit;
sem_t planificador_activo;
sem_t proceso_ejecutando;
sem_t sem_multiprogramacion;
sem_t sem_planificacion_new;
sem_t sem_planificacion_ready;
sem_t sem_planificacion_exec;
sem_t sem_planificacion_exit;

int fd_memoria;
int fd_kernel;
int fd_cpu_dispatch;
int fd_cpu_interrupt;

void terminar_programa(void);

#endif