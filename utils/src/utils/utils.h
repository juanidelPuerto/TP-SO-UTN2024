#ifndef UTILS_H_
#define UTILS_H_

#define CANT_REGISTROS 11

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "socket.h"

typedef enum {
    NUEVO,
    LISTO,
    EJECUTANDO,
    BLOQUEADO,
    TERMINADO
} t_estado;

typedef enum {
    BLOQUEO_IO,
    BLOQUEO_RECURSO,
    OUT_OF_MEMORY,
    FINALIZACION_USUARIO,
    FIN_QUANTUM,
    FIN_EXITOSO
} t_motivo_salida;

typedef enum instrucciones {
    SET, MOV_IN, MOV_OUT,
    SUM, SUB, JNZ,
    RESIZE, COPY_STRING, WAIT,
    SIGNAL, IO_GEN_SLEEP, IO_STDIN_READ,
    IO_STDOUT_WRITE, IO_FS_CREATE, IO_FS_DELETE,
    IO_FS_TRUNCATE, IO_FS_WRITE, IO_FS_READ,
    EXIT
}t_instr_cpu;

typedef struct {
    char* nombre;
    t_instr_cpu instruccion;
    t_list* argumentos;
    t_list* dir_fisicas;
} t_bloqueo;

typedef struct {
    uint8_t valor;
    uint8_t tamanio; // En bytes
} t_registro_8;

typedef struct {
    uint32_t valor;
    uint8_t tamanio; // En bytes
} t_registro_32;

typedef struct {
    char *nombre;
    int instancias_disponibles;
    t_list* cola_bloqueados_recursos;
} t_recurso;

typedef struct {
    int dir;
    int tam;
} t_dir_fisica;


typedef struct {
    int pid;
    int quantum;
    char* path;
    t_dictionary* registros;
    t_bloqueo* datos_bloqueo;
    t_estado estado;
    t_motivo_salida motivo_salida;
} t_pcb;

t_registro_8* inicializar_reg_8();
t_registro_32* inicializar_reg_32();

int es_de_8_bits(char*);

int generar_pid();
t_pcb* crear_pcb();
bool es_un_solo_param(char*);
t_dictionary* inicializar_registros();
void send_proceso_memoria(int, int, char*);
t_queue* obtener_paquete_como_cola(int);
void recibir_proceso_memoria(int, int*, char**);

t_list* recibir_paquete(int);

void send_pcb(t_pcb*, int);
void serializar_pcb(t_paquete*, t_pcb*);
void serializar_lista_args_io(t_paquete*, t_list*);
void serializar_lista_dir_fisicas(t_paquete* paquete, t_list* lista);
t_pcb* deserializar_pcb(int);
void deserializar_lista_args_io(t_list*, int*, t_list*);
void deserializar_lista_dir(t_list*, int, t_list*);


void send_nombre_interfaz(char*, int);
char* recibir_nombre_interfaz(int);

void send_instruccion_io(int pid, int socket, t_instr_cpu id_instruccion, t_list* parametros, t_list* dirs);

void send_memoria_pc(int, int, int);
void recibir_pc(int, int*, int*);
char* recibir_instruccion_memoria(int);
void send_instruccion_cpu(char*, int);

void send_cod(int, op_code);

void liberar_pcb(void*);

void mostrar_registros(t_pcb*);

void send_resize(int socket, int pid, int tamanio);

void enviar_tam_pag(int, int);

void* pedir_leer_memoria(int socket, int direccion_fisica, int tamanio, int pid);

void pedir_escribir_memoria(int socket, int dir, int tam, void* dato, int pid);

void mandar_copy_str_a_memoria(int socket, int pid, int dir_si, int dir_di, int tam);

void modificar_registro(t_dictionary* registros, char* key, int nuevo_valor);

int devolver_valor_registro(t_dictionary* registros, char* key);

void actualizar_registros(t_dictionary* regs_a_actualizar, t_dictionary* regs_nuevos);

void send_fin_proceso_memoria(int socket, int pid);

t_list* obtener_instrucciones(char* path);

void dormir(int milisegundos);

#endif