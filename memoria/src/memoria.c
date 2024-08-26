#include "../include/memoria.h"

int main(int argc, char *argv[]) {

    logger_memoria = log_create("memoria.log", "MEMORIA", 1, LOG_LEVEL_DEBUG);
    logger_memoria_obligatorio = log_create("memoria_obligatorio.log", "MEMORIA", 1, LOG_LEVEL_DEBUG);

	if(argc != 2) {
        log_error(logger_memoria, "Uso correcto: %s [Path_Config]\n", argv[0]);
        return EXIT_FAILURE;
    }

    config_memoria = iniciar_config(argv[1]);

	lista_procesos = list_create();

	pthread_mutex_init(&mutex_proceso, NULL);
	pthread_mutex_init(&mutex_memoria, NULL);
	pthread_mutex_init(&mutex_cpu, NULL);

	esta_activo = 1;

	CONFIG_MEMORIA = malloc(sizeof(t_config_memoria));

	CONFIG_MEMORIA->PUERTO = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
	CONFIG_MEMORIA->TAM_MEMORIA = config_get_int_value(config_memoria, "TAM_MEMORIA");
	CONFIG_MEMORIA->TAM_PAGINA = config_get_int_value(config_memoria, "TAM_PAGINA");
	CONFIG_MEMORIA->PATH_INSTRUCCIONES = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
	CONFIG_MEMORIA->RETARDO_RESPUESTA = config_get_int_value(config_memoria, "RETARDO_RESPUESTA");

	fd_memoria = iniciar_servidor(logger_memoria, "MEMORIA", CONFIG_MEMORIA->PUERTO);

	// Se queda esperando a que se conecte la cpu
    log_info(logger_memoria, "Esperando CPU...");
    fd_cpu = esperar_cliente(logger_memoria, "CPU", fd_memoria);

	int cod = recibir_operacion(fd_cpu);
	if(cod == TAM_PAGINA){
		int size;
		free(recibir_buffer(&size, fd_cpu));
		enviar_tam_pag(fd_cpu, CONFIG_MEMORIA->TAM_PAGINA);
		log_info(logger_memoria, "Tamanio de pagina enviado a cpu");
	}
	else {
		log_error(logger_memoria, "Error al mandar el tamanio de pagina a CPU");
		return EXIT_FAILURE;
	}
    
    // Se queda esperando a que se conecte la kernel
    log_info(logger_memoria, "Esperando KERNEL...");
    fd_kernel = esperar_cliente(logger_memoria, "KERNEL", fd_memoria);

    // Se queda esperando a que se conecte la i/o
    // log_info(logger_memoria, "Esperando I/O...");
    // fd_io = esperar_cliente(logger_memoria, "I/O", fd_memoria);
		
	iniciar_estrucuturas();

    // Atender mensajes del cpu
    pthread_t hilo_cpu;
    pthread_create(&hilo_cpu, NULL, (void*)atender_cpu, NULL);
    pthread_detach(hilo_cpu);

    // Atender mensajes de i/o
    pthread_t hilo_io;
    pthread_create(&hilo_io, NULL, (void*)esperar_io, NULL);
    pthread_detach(hilo_io);

    // Atender mensajes del kernel
    pthread_t hilo_kernel;
    pthread_create(&hilo_kernel, NULL, (void*)atender_kernel, NULL);
    pthread_join(hilo_kernel, NULL);

	log_destroy(logger_memoria);
	config_destroy(config_memoria);

	free(CONFIG_MEMORIA);

	close(fd_memoria);
	close(fd_cpu);
	close(fd_kernel);

    return EXIT_SUCCESS;
}

t_config* iniciar_config(char* path) {
    t_config* config = config_create(path);
    if (config == NULL) {
        printf("No se pudo acceder");
        exit(-1);
    }
    return config;
}

void atender_io(void* _socket) {
	int socket = *(int*)_socket;
	int conectado = 1;
	while(conectado && esta_activo) {
		int cod_op = recibir_operacion(socket);
		switch (cod_op) {
			case READ:
				leer_elemento_y_mandarlo(socket);
				break;
			case WRITE:
				escribir_elemento_recibido(socket);
				break;
			case DESCONEXION:
				log_error(logger_memoria, "El kernel se desconecto.");
				conectado = 0;
				break;
			default:
				log_warning(logger_memoria, "Operacion desconocida. No quieras meter la pata");
				break;
		}
	}
	close(socket);
}

void esperar_io() {
	while(esta_activo) {
		int fd_io = accept(fd_memoria, NULL, NULL);
		pthread_t hilo_manejar_io;
		pthread_create(&hilo_manejar_io, NULL, (void*)atender_io, (void*)&fd_io);
		pthread_detach(hilo_manejar_io);
	}
}

t_datos_proceso* buscar_proceso(int pid) {
	pthread_mutex_lock(&mutex_proceso);

	for(int i = 0; i < list_size(lista_procesos); i++) {
        t_datos_proceso* pcb = list_get(lista_procesos, i);
        if(pcb->pid == pid) {
			pthread_mutex_unlock(&mutex_proceso);
			return pcb;
		}
    }

	pthread_mutex_unlock(&mutex_proceso);
    return NULL;
}

void retardo_memoria() {
	dormir(CONFIG_MEMORIA->RETARDO_RESPUESTA);
}

void atender_cpu() {
    while (esta_activo) {
		pthread_mutex_lock(&mutex_cpu);
		int cod_op = recibir_operacion(fd_cpu);
		switch (cod_op) {
			case BUSCAR_INSTRUCCION:
				int pid;
				int pc;
				recibir_pc(fd_cpu, &pid, &pc);

				pthread_mutex_lock(&mutex_proceso); // Mutex ---

				char* instruccion = buscar_instruccion(pid, pc);

				pthread_mutex_unlock(&mutex_proceso); // ---------

				printf("Instruccion solicitada: %s (PID %d)\n", instruccion, pid);

				retardo_memoria();

				send_instruccion_cpu(instruccion, fd_cpu);
				break;
			case OP_RESIZE:
				manejar_resize();
				break;
			case MARCO:
				buscar_marco_pedido();
				break;
			case READ:
				leer_elemento_y_mandarlo(fd_cpu);
				break;
			case WRITE:
				escribir_elemento_recibido(fd_cpu);
				break;
			case DESCONEXION:
				log_error(logger_memoria, "La CPU se desconecto.");
				esta_activo = 0;
				break;
			default:
				//log_warning(logger_memoria,"Operacion desconocida. No quieras meter la pata");
				break;
		}
		pthread_mutex_unlock(&mutex_cpu);
	}
}

void atender_kernel() {
    while (esta_activo) {
		op_code cod_op = recibir_operacion(fd_kernel);
		switch (cod_op) {
			case PROCESO_NUEVO:
				pthread_mutex_lock(&mutex_proceso);
				int pid;
				char* path;
				recibir_proceso_memoria(fd_kernel, &pid, &path);

				char path_final[40];
				strcpy(path_final, CONFIG_MEMORIA->PATH_INSTRUCCIONES);
				strcat(path_final, path);

				free(path);

				log_info(logger_memoria_obligatorio, "PID: %d - Tamanio: %d", pid, 0);

				t_datos_proceso* proceso = malloc(sizeof(t_datos_proceso));
				proceso->pid = pid;
				proceso->instrucciones = obtener_instrucciones(path_final);
				proceso->tamanio = 0;
				proceso->tabla_paginas = list_create();
				list_add(lista_procesos, proceso);
				pthread_mutex_unlock(&mutex_proceso);
				break;
			case ELIMINAR_PROCESO:
				finalizar_proceso();
				break;
			case DESCONEXION:
				log_error(logger_memoria, "El kernel se desconecto.");
				esta_activo = 0;
				break;
			default:
				printf("Cod: %d\n", cod_op);
				log_warning(logger_memoria,"Operacion desconocida. No quieras meter la pata");
				break;
		}
	}
}

char* buscar_instruccion(int pid, int pc) {
	int tam = list_size(lista_procesos);
	// printf("PID: %d y PC: %d\n", pid, pc);
	for(int i = 0; i < tam; i++) {
		t_datos_proceso* proc = (t_datos_proceso*)list_get(lista_procesos, i);
		//if(proc->pid == pid && pc > list_size(proc->instrucciones)) return NULL;
		if(proc->pid == pid && pc < list_size(proc->instrucciones)) {
			return list_get(proc->instrucciones, pc);
		}
		// else if (pc > list_size(proc->instrucciones))
		// 	log_error(logger_memoria, "No existe la instruccion");
		//free(proc);
	}
	log_error(logger_memoria, "No existe ese pid");
	exit(-1);
}

void mostrar(int pid) {
	int tam = list_size(lista_procesos);
	// printf("PID: %d y PC: %d\n", pid, pc);
	for(int i = 0; i < tam; i++) {
		t_datos_proceso* proc = (t_datos_proceso*)list_get(lista_procesos, i);
		if(proc->pid == pid) {
			for(int j = 0; j < list_size(proc->instrucciones); j++) {
				printf("Elemento: %s\n", strdup(list_get(proc->instrucciones, j)));
			}
		}
	}
}

void iniciar_estrucuturas() {
	memoria_principal = (void*)malloc(CONFIG_MEMORIA->TAM_MEMORIA);
	memset(memoria_principal, 0, CONFIG_MEMORIA->TAM_MEMORIA);

	// marcos = list_create();

	cant_marcos = CONFIG_MEMORIA->TAM_MEMORIA/CONFIG_MEMORIA->TAM_PAGINA;

	marcos = malloc(cant_marcos);
	memset(marcos, 0, cant_marcos); 

	// for(int i = 0; i < cant_marcos; i++) {
	// 	t_marco* marco = (t_marco*)malloc(sizeof(t_marco));
	// 	marco->numero_marco = i;
	// 	marco->id_proceso = -1;
	// 	marco->pagina_asociada = NULL;
	// 	list_add(marcos, marco);
	// }
}

void manejar_resize() {
	t_list* instruccion_recibida = recibir_paquete(fd_cpu);
	int pid = *(int*)list_get(instruccion_recibida, 0);
	int nuevo_tamanio = *(int*)list_get(instruccion_recibida, 1);
	list_destroy_and_destroy_elements(instruccion_recibida, &free);
	t_datos_proceso* proceso = buscar_proceso(pid);
	int cant_pags_anterior = list_size(proceso->tabla_paginas);
	int cant_pags_actual = (int) ceil((double)(nuevo_tamanio)/(double)(CONFIG_MEMORIA->TAM_PAGINA));
	if(nuevo_tamanio > proceso->tamanio) {
		log_info(logger_memoria_obligatorio, "PID: %d - Tamaño Actual: %d - Tamaño a Ampliar: %d", pid, proceso->tamanio, nuevo_tamanio-proceso->tamanio);
		proceso->tamanio = nuevo_tamanio;
		int paginas_a_agregar = cant_pags_actual - cant_pags_anterior;
		if(paginas_a_agregar > 0) // Si es cero significa que el tamanio no es suficientemente mayor para agregar otra
			asingar_paginas(proceso, paginas_a_agregar);
	}
	else if(nuevo_tamanio < proceso->tamanio) {
		log_info(logger_memoria_obligatorio, "PID: %d - Tamaño Actual: %d - Tamaño a Reducir: %d", pid, proceso->tamanio, proceso->tamanio-nuevo_tamanio);
		proceso->tamanio = nuevo_tamanio;
		int paginas_a_eliminar = cant_pags_anterior - cant_pags_actual;
		if(paginas_a_eliminar > 0) 
			eliminar_paginas(proceso->tabla_paginas, paginas_a_eliminar);
	}
	retardo_memoria();
	send_cod(fd_cpu, OK);
}

int buscar_proximo_marco_libre() {
	for(int i = 0; i < cant_marcos; i++) {
		if(marcos[i] == 0) return i;
	}
	return -1;
}

void asingar_paginas(t_datos_proceso* proceso, int cant_paginas) {
	int cont_paginas = list_size(proceso->tabla_paginas);
	for(int i = 0; i < cant_paginas; i++) {
		int marco_libre = buscar_proximo_marco_libre();
		if(marco_libre == -1) {
			send_cod(fd_cpu, OUT_OF_MEMORY_AVISO);
			return;
		}
		t_nodo_tabla* pagina_nueva = malloc(sizeof(t_nodo_tabla));
		pagina_nueva->numero_pagina = cont_paginas;
		pagina_nueva->numero_marco = marco_libre;
		list_add(proceso->tabla_paginas, pagina_nueva);
		marcos[marco_libre] = 1;
		printf("Pid: %d - Pagina: %d - Marco: %d\n", proceso->pid, cont_paginas, marco_libre);
		cont_paginas++;
	}
}

void eliminar_paginas(t_list* tabla_paginas, int cant_paginas) {
	for(int i = 0; i < cant_paginas; i++) {
		t_nodo_tabla* nodo_tabla = (t_nodo_tabla*)elimnar_ultimo_elemento(tabla_paginas);
		marcos[nodo_tabla->numero_marco] = 0;
		free(nodo_tabla);
	}
}

void* elimnar_ultimo_elemento(t_list* lista) {
	return list_remove(lista, list_size(lista) - 1);
}

void log_acceso_a_tabla(int pid, int num_pagina, int num_marco) {
	log_info(logger_memoria, "PID: %d - Pagina: %d - Marco: %d", pid, num_pagina, num_marco);
}

void buscar_marco_pedido() {
	t_list* lista = recibir_paquete(fd_cpu);
	int pid = *(int*)list_get(lista, 0);
	int num_pagina = *(int*)list_get(lista, 1);
	t_datos_proceso* proceso = buscar_proceso(pid);
	int marco = buscar_marco_en_tabla(proceso, num_pagina);
	t_paquete* paquete = crear_paquete(MARCO);
	agregar_a_paquete(paquete, &marco, sizeof(int));

	retardo_memoria();

	enviar_paquete(paquete, fd_cpu);
	list_destroy_and_destroy_elements(lista, &free);
	eliminar_paquete(paquete);
}

int buscar_pagina_en_tabla(t_datos_proceso* proceso, int num_marco) {
	for(int i = 0; i < list_size(proceso->tabla_paginas); i++) {
		t_nodo_tabla* entrada = list_get(proceso->tabla_paginas, i);
		if(entrada->numero_marco == num_marco) {
			log_acceso_a_tabla(proceso->pid, entrada->numero_pagina, num_marco);
			return entrada->numero_pagina;
		}
	}
	log_error(logger_memoria, "Error al buscar marco en la tabla");
	exit(-1);
}

int buscar_marco_en_tabla(t_datos_proceso* proceso, int num_pagina) {
	for(int i = 0; i < list_size(proceso->tabla_paginas); i++) {
		t_nodo_tabla* entrada = list_get(proceso->tabla_paginas, i);
		if(entrada->numero_pagina == num_pagina) {
			log_acceso_a_tabla(proceso->pid, num_pagina, entrada->numero_marco);
			return entrada->numero_marco;
		}
	}
	log_error(logger_memoria, "Error al buscar marco en la tabla");
	exit(-1);
}

void* leer_elemento(int pid, int dir, int tam) {
	void* dato_a_leer = malloc(tam);

	pthread_mutex_lock(&mutex_memoria);

	memcpy(dato_a_leer, (char*)(memoria_principal + dir), tam);
	mem_hexdump((char*)(memoria_principal + dir), 16);

	pthread_mutex_unlock(&mutex_memoria);

	return dato_a_leer;
} 

void leer_elemento_y_mandarlo(int socket) {
	t_list* lista = recibir_paquete(socket);
	int dir_fisica = *(int*)list_get(lista, 0);
	int tamanio = *(int*)list_get(lista, 1);
	int pid = *(int*)list_get(lista, 2);
	list_destroy_and_destroy_elements(lista, &free);

	log_info(logger_memoria_obligatorio, "PID: %d - Accion: LEER - Direccion fisica: %d - Tamaño: %d", pid, dir_fisica, tamanio);

	void* dato_leido = leer_elemento(pid, dir_fisica, tamanio);

	retardo_memoria();

	t_paquete* paquete = crear_paquete(OK);
	agregar_a_paquete(paquete, dato_leido, tamanio);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
	free(dato_leido);
}

void escribir_elemento(int pid, void* dato, int dir_fisica, int tam_dato) {
	pthread_mutex_lock(&mutex_memoria);

	memcpy((char*)(memoria_principal + dir_fisica), dato, tam_dato);
	mem_hexdump((char*)(memoria_principal + dir_fisica), 16);

	pthread_mutex_unlock(&mutex_memoria);
}

void escribir_elemento_recibido(int socket) {
	t_list* lista = recibir_paquete(socket);
	int pid = *(int*)list_get(lista, 0);
	int dir_fisica = *(int*)list_get(lista, 1);
	int tam_dato = *(int*)list_get(lista, 3);
	void* dato = malloc(tam_dato);
	memcpy(dato, list_get(lista, 2), tam_dato);
	list_destroy_and_destroy_elements(lista, &free);

	log_info(logger_memoria_obligatorio, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d - Tamaño: %d", pid, dir_fisica, tam_dato);

	escribir_elemento(pid, dato, dir_fisica, tam_dato);

	retardo_memoria();

	send_cod(socket, OK);
	free(dato);
}

void finalizar_proceso() {
	t_list* lista = recibir_paquete(fd_kernel);
	int pid = *(int*)list_get(lista, 0);

	t_datos_proceso* proceso = buscar_proceso(pid);

	list_remove_element(lista_procesos, proceso);
	
	log_info(logger_memoria, "Finalizacion del proceso %d", proceso->pid);
	
	t_list* tabla_paginas = proceso->tabla_paginas;

	int cant_paginas = list_size(tabla_paginas);

	log_info(logger_memoria_obligatorio, "PID: %d - Tamanio: %d", pid, cant_paginas);

	eliminar_paginas(tabla_paginas, cant_paginas);

	list_destroy(tabla_paginas);

	list_destroy_and_destroy_elements(proceso->instrucciones, &free);

	list_destroy_and_destroy_elements(lista, &free);

	free(proceso);
}


// iniciar_proceso proceso1.txt
// iniciar_proceso ej1.txt
// iniciar_proceso ej2.txt