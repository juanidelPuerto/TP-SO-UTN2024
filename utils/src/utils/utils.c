#include "utils.h"

char* array_registros[11] = {
    "AX",
    "BX",
    "CX",
    "DX",
    "PC",
    "EAX",
    "EBX",
    "ECX",
    "EDX",
    "SI",
    "DI"
};

/* int tamanio_x_registro[11] = {
    4, // PC
    1, // AX
    1, // 
    1,
    1,
    4,
    4,
    4,
    4
} */

t_registro_8* inicializar_reg_8() {
    t_registro_8* reg = malloc(sizeof(t_registro_8));
    reg->valor = 0;
    reg->tamanio = 1;
    return reg;
}

t_registro_32* inicializar_reg_32() {
    t_registro_32* reg = malloc(sizeof(t_registro_32));
    reg->valor = 0;
    reg->tamanio = 4;
    return reg;
}

int es_de_8_bits(char* key) {
    for(int i = 0; i < 4; i++) {
        if(string_equals_ignore_case(array_registros[i], key))
            return 1;
    }
    return 0;
}

int generar_pid() {
    static int ultimo_pid = 0;
    return ultimo_pid++;
}

bool es_un_solo_param(char* params) {
    int flag = 1;
    char** split = string_split(params, " ");
    if(params == NULL || split[1] != NULL) flag = 0;
    if(params != NULL) string_array_destroy(split);
    return flag;
}

t_dictionary* inicializar_registros() {
    t_dictionary* registros = dictionary_create();
    for (int i = 0; i < CANT_REGISTROS; i++) {
        if (i < 4) {
            dictionary_put(registros, array_registros[i], (void*)inicializar_reg_8());
        }
        else {
            dictionary_put(registros, array_registros[i], (void*)inicializar_reg_32());
        }
    }
    return registros;
}

t_pcb* crear_pcb() {
    t_pcb* pcb = malloc(sizeof(t_pcb));
    pcb->pid = generar_pid();
    pcb->quantum = 0;
    pcb->datos_bloqueo = malloc(sizeof(t_bloqueo));
    pcb->datos_bloqueo->nombre = string_new();
    pcb->datos_bloqueo->instruccion = -1;
    pcb->datos_bloqueo->argumentos = list_create();
    pcb->datos_bloqueo->dir_fisicas = list_create();
    pcb->motivo_salida = -1;
    pcb->registros = inicializar_registros();
    pcb->estado = LISTO;
    return pcb;
}

void send_proceso_memoria(int fd_memoria, int pid, char* path) {
    t_paquete* paquete = crear_paquete(PROCESO_NUEVO);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, (void*)path, strlen(path) + 1);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    free(path);
}

t_queue* obtener_paquete_como_cola(int socketCliente) {
  int tamanioBuffer;
  int tamanioContenido;
  int desplazamiento = 0;

  t_queue* contenido = queue_create();
  void* buffer = recibir_buffer(&tamanioBuffer, socketCliente);

  while (desplazamiento < tamanioBuffer) {
    memcpy(&tamanioContenido, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);

    void *valor = malloc(tamanioContenido);
    memcpy(valor, buffer + desplazamiento, tamanioContenido);
    desplazamiento += tamanioContenido;

    queue_push(contenido, valor);
  }

  free(buffer);
  return contenido;
}

t_list* recibir_paquete(int socket_cliente) {
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size) {
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		void* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void recibir_proceso_memoria(int fd_kernel, int* pid, char** path) {
    t_list* lista = recibir_paquete(fd_kernel);
    *pid = *(int*)list_get(lista, 0);
    *path = strdup(list_get(lista, 1));
    list_destroy_and_destroy_elements(lista, &free);
}

void send_pcb(t_pcb* pcb, int socket) {
    t_paquete* paquete = crear_paquete(PCB);
    serializar_pcb(paquete, pcb);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void serializar_pcb(t_paquete* paquete, t_pcb* pcb) {
    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));
    agregar_a_paquete(paquete, &(pcb->quantum), sizeof(int));
    agregar_a_paquete(paquete, &(pcb->estado), sizeof(t_estado));
    agregar_a_paquete(paquete, (pcb->datos_bloqueo->nombre), strlen(pcb->datos_bloqueo->nombre) + 1);
    agregar_a_paquete(paquete, &(pcb->datos_bloqueo->instruccion), sizeof(t_instr_cpu));
    agregar_a_paquete(paquete, &(pcb->motivo_salida), sizeof(t_motivo_salida));

    for (int i = 0; i < CANT_REGISTROS; i++) {
        char* key = array_registros[i];
        if(i < 4) {
            t_registro_8* reg = (t_registro_8*)dictionary_get(pcb->registros, key);
            agregar_a_paquete(paquete, &(reg->valor), sizeof(uint8_t));
        }
        else {
            t_registro_32* reg = (t_registro_32*)dictionary_get(pcb->registros, key);
            agregar_a_paquete(paquete, &(reg->valor), sizeof(uint32_t));
        }
    }

    if(pcb->motivo_salida == BLOQUEO_IO) {
        serializar_lista_args_io(paquete, pcb->datos_bloqueo->argumentos);
        serializar_lista_dir_fisicas(paquete, pcb->datos_bloqueo->dir_fisicas);
    }
    // serializar_diccionario_registros(paquete, pcb->registros);
}

void serializar_lista_args_io(t_paquete* paquete, t_list* lista) {
    int tamLista = list_size(lista);

    agregar_a_paquete(paquete, &tamLista, sizeof(int));

    for (int i = 0; i < tamLista; i++) {
        char* arg = list_get(lista, i);
        agregar_a_paquete(paquete, arg, strlen(arg) + 1);
    }
}

void serializar_lista_dir_fisicas(t_paquete* paquete, t_list* lista) {
    int tamLista = list_size(lista);

    agregar_a_paquete(paquete, &tamLista, sizeof(int));

    for (int i = 0; i < tamLista; i++) {
        t_dir_fisica* dir = list_get(lista, i);
        agregar_a_paquete(paquete, &(dir->dir), sizeof(int));
        agregar_a_paquete(paquete, &(dir->tam), sizeof(int));
    }
}

t_pcb* deserializar_pcb(int socket) {
    t_list* lista = recibir_paquete(socket);
    t_pcb* pcbNuevo = malloc(sizeof(t_pcb));

    pcbNuevo->pid = *(int*)list_get(lista, 0);
    pcbNuevo->quantum = *(int*)list_get(lista, 1);
    pcbNuevo->estado = *(t_estado*)list_get(lista, 2);
    pcbNuevo->datos_bloqueo = malloc(sizeof(t_bloqueo));
    pcbNuevo->datos_bloqueo->nombre = strdup(list_get(lista, 3));
    pcbNuevo->datos_bloqueo->instruccion = *(t_instr_cpu*)list_get(lista, 4);
    pcbNuevo->motivo_salida = *(t_motivo_salida*)list_get(lista, 5);
    pcbNuevo->datos_bloqueo->argumentos = list_create();
    pcbNuevo->datos_bloqueo->dir_fisicas = list_create();

    int pos = 6;

    pcbNuevo->registros = inicializar_registros();

    for(int i = 0; i < CANT_REGISTROS; i++) {
        char* key = array_registros[i];
        if(es_de_8_bits(key)) {
            uint8_t element = *(uint8_t*)list_get(lista, pos);
            t_registro_8* reg = (t_registro_8*)dictionary_get(pcbNuevo->registros, key);
            reg->valor = element;
        }
        else {
            uint32_t element = *(uint32_t*)list_get(lista, pos);
            t_registro_32* reg = (t_registro_32*)dictionary_get(pcbNuevo->registros, key);
            reg->valor = element;
        }
        pos++;
    }

    if(pcbNuevo->motivo_salida == BLOQUEO_IO) {
        deserializar_lista_args_io(lista, &pos, pcbNuevo->datos_bloqueo->argumentos);
        deserializar_lista_dir(lista, pos, pcbNuevo->datos_bloqueo->dir_fisicas);
    }

    list_destroy_and_destroy_elements(lista, &free);

    return pcbNuevo;
}

void deserializar_lista_args_io(t_list* lista, int* posInicial, t_list* lista_args) {
    int tam_lista = *(int*)list_get(lista, *posInicial);
    for(int i = 1; i <= tam_lista; i++) {
        char* arg = strdup(list_get(lista, *posInicial + i));
        list_add(lista_args, arg);
        if(i == tam_lista) (*posInicial) += i;
    }
}

void deserializar_lista_dir(t_list* lista, int pos_inicial, t_list* lista_dirs) {
    pos_inicial++;
    int tam_lista = *(int*)list_get(lista, pos_inicial);
    for(int i = 0; i < tam_lista; i++) {
        t_dir_fisica* dir = malloc(sizeof(t_dir_fisica));
        dir->dir = *(int*)list_get(lista, pos_inicial + i*2 + 1);
        dir->tam = *(int*)list_get(lista, pos_inicial + i*2 + 2);
        list_add(lista_dirs, dir);
    }
}

void liberar_pcb(void* pcb_v) {
    t_pcb* pcb = (t_pcb*)pcb_v;
    dictionary_destroy_and_destroy_elements(pcb->registros, &free);
    // dictionary_destroy(pcb->registros);
    free(pcb->datos_bloqueo->nombre);
    list_destroy_and_destroy_elements(pcb->datos_bloqueo->argumentos, &free);
    list_destroy_and_destroy_elements(pcb->datos_bloqueo->dir_fisicas, &free);
    free(pcb->datos_bloqueo);
    free(pcb);
}

void send_nombre_interfaz(char* nombre, int socket) {
    t_paquete* paquete = crear_paquete(NOMBRE_INTERFAZ);
    char* nombre_a = strdup(nombre);
    agregar_a_paquete(paquete, (void*)nombre_a, strlen(nombre) + 1);
    enviar_paquete(paquete, socket);
    free(nombre_a);
    eliminar_paquete(paquete);
}

char* recibir_nombre_interfaz(int socket) {
    t_list* lista = recibir_paquete(socket);
    char* nombre = strdup(list_get(lista, 0));
    list_destroy_and_destroy_elements(lista, &free);
    return nombre;
}

void send_instruccion_io(int socket, int pid, t_instr_cpu instruccion, t_list* args, t_list* dirs) {
    t_paquete* paquete = crear_paquete(INSTRUCCION);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &instruccion, sizeof(t_instr_cpu));
    serializar_lista_args_io(paquete, args);
    serializar_lista_dir_fisicas(paquete, dirs);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void send_cod(int socket, op_code cod) {
    t_paquete* paquete = crear_paquete(cod);
    int basura = 0;
    agregar_a_paquete(paquete, &basura, sizeof(int)); // Provisorio, si no le mando nada me tira error
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void send_memoria_pc(int pid, int pc, int socket) {
    t_paquete* paquete = crear_paquete(BUSCAR_INSTRUCCION);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &pc, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void recibir_pc(int socket, int* pid, int* pc) {
    t_list* lista = recibir_paquete(socket);
    *pid = *(int*) list_get(lista, 0);
    *pc = *(int*) list_get(lista, 1);
    list_destroy_and_destroy_elements(lista, &free);
}

char* recibir_instruccion_memoria(int socket) {
    t_list* lista = recibir_paquete(socket);
    char* instruccion = strdup(list_get(lista, 0));
    list_destroy_and_destroy_elements(lista, &free);
    return instruccion;
}

void send_instruccion_cpu(char* instruccion, int socket) {
    t_paquete* paquete = crear_paquete(INSTRUCCION_A_EJECUTAR);
    agregar_a_paquete(paquete, (void*)instruccion, strlen(instruccion) + 1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void mostrar_registros(t_pcb* pcb) {
    printf("\nRegistros del proceso: %d\n", pcb->pid);
    for (int i = 0; i < CANT_REGISTROS; i++) {
        char* key = array_registros[i];
        printf("%s: %d\n", key, devolver_valor_registro(pcb->registros, key));
    }
}

void send_resize(int socket, int pid, int tamanio) {
    t_paquete* paquete = crear_paquete(OP_RESIZE);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void enviar_tam_pag(int socket, int tam_pag) {
    t_paquete* paquete = crear_paquete(TAM_PAGINA);
    agregar_a_paquete(paquete, &tam_pag, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void* pedir_leer_memoria(int socket, int direccion_fisica, int tamanio, int pid) {
    t_paquete* paquete = crear_paquete(READ);
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    op_code cod = recibir_operacion(socket);

    if(cod == OK) {
        t_list* lista = recibir_paquete(socket);
        void* valor = malloc(tamanio);
        memcpy(valor, list_get(lista, 0), tamanio);
        list_destroy_and_destroy_elements(lista, &free);
        return valor;
    }
        
    return NULL;
}

void pedir_escribir_memoria(int socket, int dir, int tam, void* dato, int pid) {
    t_paquete* paquete = crear_paquete(WRITE);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &dir, sizeof(int));
    agregar_a_paquete(paquete, dato, tam);
    agregar_a_paquete(paquete, &tam, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    op_code cod = recibir_operacion(socket);

    if(cod != OK) exit(-1);

    int size;
    free(recibir_buffer(&size, socket));
}

void mandar_copy_str_a_memoria(int socket, int pid, int dir_si, int dir_di, int tam) {
    t_paquete* paquete = crear_paquete(COPY_STR);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &dir_si, sizeof(int));
    agregar_a_paquete(paquete, &dir_di, sizeof(int));
    agregar_a_paquete(paquete, &tam, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    op_code cod = recibir_operacion(socket);

    if(cod != OK) exit(-1);

    int size;
    free(recibir_buffer(&size, socket));
}


void modificar_registro(t_dictionary* registros, char* key, int nuevo_valor) {
    if(es_de_8_bits(key)) {
        t_registro_8* reg = (t_registro_8*)dictionary_get(registros, key);
        reg->valor = (uint8_t)nuevo_valor;
    }
    else {
        t_registro_32* reg = (t_registro_32*)dictionary_get(registros, key);
        reg->valor = (uint32_t)nuevo_valor;
    }
}

int devolver_valor_registro(t_dictionary* registros, char* key) {
    if(es_de_8_bits(key)) {
        t_registro_8* reg = (t_registro_8*)dictionary_get(registros, key);
        return (int)reg->valor;
    }
    else {
        t_registro_32* reg = (t_registro_32*)dictionary_get(registros, key);
        return (int)reg->valor;
    }
}

void actualizar_registros(t_dictionary* regs_a_actualizar, t_dictionary* regs_nuevos) {
    for(int i = 0; i < CANT_REGISTROS; i++ ) {
        char* key = array_registros[i];
        modificar_registro(regs_a_actualizar, key, devolver_valor_registro(regs_nuevos, key));
    }
}

void send_fin_proceso_memoria(int socket, int pid) {
    t_paquete* paquete = crear_paquete(ELIMINAR_PROCESO);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_list* obtener_instrucciones(char* path) {
	FILE* archivo = fopen(path, "r");
	char* instruccion = (char*)malloc(50 * sizeof(char));
	t_list* lista = list_create();
	while(fgets(instruccion, 50, archivo)) {
		list_add(lista, strdup(instruccion));
		memset(instruccion, '\0', 50);
	}
	fclose(archivo);
	free(instruccion);
	return lista;
}

void dormir(int milisegundos) {
    struct timespec ts;
    ts.tv_sec = milisegundos / 1000;
    ts.tv_nsec = (milisegundos % 1000) * 1000000;
    nanosleep(&ts, NULL);
}