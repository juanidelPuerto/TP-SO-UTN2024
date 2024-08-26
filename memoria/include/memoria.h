#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/memory.h>
#include <commons/collections/list.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <readline/readline.h>
#include "../../utils/src/utils/socket.h"
#include "../../utils/src/utils/utils.h"

t_log* logger_memoria_obligatorio;
t_log* logger_memoria;
t_config* config_memoria;

t_list* lista_procesos;

pthread_mutex_t mutex_proceso;
pthread_mutex_t mutex_memoria;
pthread_mutex_t mutex_cpu;

void* memoria_principal;

typedef struct{
    int numero_pagina;
    int numero_marco;
} t_nodo_tabla;

typedef struct {
    int pid;
    int tamanio;
    t_list* instrucciones;
    t_list* tabla_paginas; // Lista de t_pagina
} t_datos_proceso;

uint8_t* marcos;

int cant_marcos;

int fd_memoria;
int fd_kernel;
int fd_cpu;

int esta_activo;

typedef struct {
    char* PUERTO;
    int TAM_MEMORIA;
    int TAM_PAGINA;
    char* PATH_INSTRUCCIONES;
    int RETARDO_RESPUESTA;
} t_config_memoria;

t_config_memoria* CONFIG_MEMORIA;

char* puerto;
int tam_memoria;

void esperar_io();
void atender_io(void* _socket);

t_config* iniciar_config(char* path);
void iniciar_consola();
void atender_cpu();
void atender_kernel();

char* buscar_instruccion(int pid, int pc);

void mostrar(int pid);

void iniciar_estrucuturas();

void retardo_memoria();

void manejar_resize();
// bool es_marco_libre(void*);
int buscar_proximo_marco_libre();
void asingar_paginas(t_datos_proceso*, int);
void eliminar_paginas(t_list*, int);
void* elimnar_ultimo_elemento(t_list*);

void buscar_marco_pedido();

// int recibir_direccion_fisica(int fd_cpu);
// char* buscar_dato(int direccionFisica);

void leer_elemento_y_mandarlo(int);

void escribir_elemento_recibido(int socket);

void realizar_copy_string();

int buscar_pagina_en_tabla(t_datos_proceso* proceso, int num_marco);
int buscar_marco_en_tabla(t_datos_proceso* proceso, int num_pagina);

void finalizar_proceso();

#endif