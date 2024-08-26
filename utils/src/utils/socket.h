#ifndef SOCKET_H_
#define SOCKET_H_

#include <stdlib.h>
#include <stdio.h>

#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <string.h>
#include <stdlib.h>

typedef enum
{
	PAQUETE,
	MENSAJE,
	OK,
	PROCESO_NUEVO,
	PCB,
	NOMBRE_INTERFAZ,
	INSTRUCCION,
	BLOQUEO_TERMINADO,
	TAM_PAGINA,
	MARCO,
	READ,
	WRITE,
	COPY_STR,
	INTERRUPCION_QUANTUM,
	INTERRUPCION_USUARIO,
	ELIMINAR_PROCESO,
	BUSCAR_INSTRUCCION,
	OP_RESIZE,
	INSTRUCCION_A_EJECUTAR,
	OUT_OF_MEMORY_AVISO,
	ERROR,
	DESCONEXION
}op_code;

typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

int iniciar_servidor(t_log* logger, const char* name, char* puerto);
int esperar_cliente(t_log* logger, const char* name, int socket_servidor);
int crear_conexion(t_log* logger, char* ip, char* puerto);
void liberar_conexion(int* socket_cliente);
op_code recibir_operacion(int socket_cliente);

void enviar_paquete(t_paquete* paquete, int conexion);
void* serializar_paquete(t_paquete* paquete, int bytes);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_mensaje(char* mensaje, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log* logger);
void eliminar_paquete(t_paquete* paquete);
void crear_buffer(t_paquete* paquete);
void destruir_buffer(t_buffer* un_buffer);
t_paquete* crear_paquete(op_code);
t_buffer* recibir_todo_el_buffer(int conexion);
void* recibir_buffer(int* size, int socket_cliente);

#endif
