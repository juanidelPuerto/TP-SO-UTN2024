#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/memory.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <string.h>
#include <assert.h>
#include <readline/readline.h>
#include <pthread.h>
#include "../../utils/src/utils/utils.h"
#include "../../utils/src/utils/socket.h"
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <dirent.h>		
#include <fcntl.h> 

char* nombre_interfaz;
char* path_config;

t_log* logger_io;
t_log* logger_io_obligatorio;
t_config* config_io;

int fd_memoria;
int fd_kernel;
int fd_io;

t_list* lista_archivos;

t_bitarray* bitmap_bloques;

void* buffer_bloques;
void* buffer_bitmap;

typedef struct {
    char* nombre;
    int bloque_inicial;
    int tamanio;
    t_config* archivo;
} t_fcb;

typedef enum {
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
} t_tipo_interfaz;


t_tipo_interfaz TIPO_INTERFAZ;
int TIEMPO_UNIDAD_TRABAJO;
char* IP_KERNEL;
char* PUERTO_KERNEL;
char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* PATH_BASE_DIALFS;
int BLOCK_SIZE;
int BLOCK_COUNT;
int RETRASO_COMPACTACION;

void terminar_programa();
t_config* iniciar_config();
void iniciar_estructuras_fs();
t_tipo_interfaz tipo_interfaz_a_enum(char* tipo_interfaz);
void cargar_configs();
void send_nombre_interfaz(char* nombre_interfaz, int fd_kernel); 
void atender_kernel(void);

void manejar_operacion_generica(int pid, t_instr_cpu instruccion, t_list* parametros);
void manejar_operacion_stdin(int pid, t_instr_cpu instruccion, t_list* dirs);
void manejar_operacion_stdout(int pid, t_instr_cpu instruccion, t_list* dirs);
void manejar_operacion_fs(int pid, t_instr_cpu instruccion, t_list* parametros, t_list* dirs);
void compactar_dialfs();
void manejar_fs_read(int pid, t_list* parametros, t_list* dirs);
void manejar_fs_write(int pid, t_list *parametros, t_list* dirs);
void manejar_fs_truncate(int pid, t_list *parametros);
void manejar_fs_delete(int pid, t_list *parametros);
void manejar_fs_create(int pid, t_list *parametros);
void manejar_gen_sleep(int pid, t_list* parametros); 
//logs
void log_operacion(int pid, char* operacion);
void log_fs_fin_compactacion(int pid);
void log_fs_inicio_compactacion(int pid);
void log_fs_read(int pid, char *nombre_archivo, int tamanio, int puntero_archivo);
void log_fs_write(int pid, char *nombre_archivo, int tamanio, int ptr);
void log_fs_truncate(int pid, char *nombre_archivo, int tamanio);
void log_fs_delete(int pid, char *nombre_archivo);
void log_fs_create(int pid, char *nombre_archivo);

void actualizar_bloque_inicial_archivo(t_fcb* fcb, int bloque_inicial);
void actualizar_tamanio_archivo(t_fcb* fcb, int bloque_inicial);


char* get_path(char* nombre_archivo);


#endif