#include "../include/cpu.h"

int main(int argc, char *argv[]) {

    logger_cpu_obligatorio = log_create("cpu_obligatorio.log", "CPU", 1, LOG_LEVEL_DEBUG);
    logger_cpu = log_create("cpu.log", "CPU", 1, LOG_LEVEL_DEBUG);

    if(argc != 2) {
        log_error(logger_cpu, "Uso correcto: %s [Path_Config]\n", argv[0]);
        return EXIT_FAILURE;
    }

    t_config* config_ips = config_create("./configs/ips.config");
    config_cpu = iniciar_config(argv[1]);

    registros_cpu = inicializar_registros();

    tlb = list_create();

    pthread_mutex_init(&mutex_registros, NULL);
    pthread_mutex_init(&mutex_interrupcion_quantum, NULL);
    pthread_mutex_init(&mutex_interrupcion_usuario, NULL);
    pthread_mutex_init(&mutex_pcb, NULL);

    sem_init(&hay_interrupcion, 0, 1);

    hay_una_interrupcion_quantum = false;
    hay_una_interrupcion_usuario = false;

    control_key = true;

    CONFIG_CPU.IP_MEMORIA = config_get_string_value(config_ips, "IP_MEMORIA");

    CONFIG_CPU.PUERTO_ESCUCHA_DISPATCH = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_DISPATCH");
    CONFIG_CPU.PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_INTERRUPT");
    CONFIG_CPU.PUERTO_MEMORIA = config_get_string_value(config_cpu, "PUERTO_MEMORIA");
    CONFIG_CPU.CANTIDAD_ENTRADAS_TLB = config_get_int_value(config_cpu, "CANTIDAD_ENTRADAS_TLB");
    CONFIG_CPU.ALGORITMO_TLB = config_get_string_value(config_cpu, "ALGORITMO_TLB");

    // Inicio servidores
    fd_cpu_dispatch = iniciar_servidor(logger_cpu, "CPU-DISPATCH", CONFIG_CPU.PUERTO_ESCUCHA_DISPATCH);
    fd_cpu_interrupt = iniciar_servidor(logger_cpu, "CPU-INTERRUPT", CONFIG_CPU.PUERTO_ESCUCHA_INTERRUPT);

    // Me conecto a memoria
    fd_memoria = crear_conexion(logger_cpu, CONFIG_CPU.IP_MEMORIA, CONFIG_CPU.PUERTO_MEMORIA);
    log_info(logger_cpu, "Se conecto a MEMORIA con exito!!");

    pedir_tam_pagina();

    // Espero que se conecte kernel
    log_info(logger_cpu, "Esperando KERNEL-DISPATCH...");
    fd_kernel_dispatch = esperar_cliente(logger_cpu, "KERNEL-DISPATCH", fd_cpu_dispatch);
    log_info(logger_cpu, "Esperando KERNEL-INTERRUPT...");
    fd_kernel_interrupt = esperar_cliente(logger_cpu, "KERNEL-INTERRUPT", fd_cpu_interrupt);

    // Atender los mensajes de Kernel - Dispatch
    pthread_t hilo_kernel_dispatch;
    pthread_create(&hilo_kernel_dispatch, NULL, (void *)atender_cpu_kernel_dispatch, NULL);
    pthread_detach(hilo_kernel_dispatch);

    // Atender los mensajes de Kernel - Interrupt
    pthread_t hilo_kernel_interrupt;
    pthread_create(&hilo_kernel_interrupt, NULL, (void *)atender_cpu_kernel_interrupt, NULL);
    pthread_join(hilo_kernel_interrupt, NULL);

    log_destroy(logger_cpu);
    log_destroy(logger_cpu_obligatorio);
    config_destroy(config_cpu);

    dictionary_destroy_and_destroy_elements(registros_cpu, &free);
    // dictionary_destroy(registros_cpu);
    list_destroy_and_destroy_elements(tlb, &free);

    close(fd_cpu_dispatch);
    close(fd_cpu_interrupt);
    close(fd_kernel_dispatch);
    close(fd_kernel_interrupt);
    close(fd_memoria);

    pthread_mutex_destroy(&mutex_registros);
    pthread_mutex_destroy(&mutex_interrupcion_quantum);
    pthread_mutex_destroy(&mutex_interrupcion_usuario);
    pthread_mutex_destroy(&mutex_pcb);

    liberar_conexion(&fd_memoria);

    config_destroy(config_ips);

    // Destruir semaforos y lo que sea necesario

    return EXIT_SUCCESS;
}

t_config *iniciar_config(char* path) {
    t_config *config = config_create(path);
    if (config == NULL) {
        printf("No se pudo acceder");
        exit(-1);
    }
    return config;
}

void pedir_tam_pagina() {
    send_cod(fd_memoria, TAM_PAGINA); // Arreglar esta parte
    log_info(logger_cpu, "Pidiendo tamanio de pagina a Memoria");
    switch (recibir_operacion(fd_memoria)) {
        case TAM_PAGINA:
            t_list *lista = recibir_paquete(fd_memoria);
            tam_pagina = *(int *)list_get(lista, 0);
            log_info(logger_cpu, "Tamanio de pagina recibido desde Memoria");
            list_destroy_and_destroy_elements(lista, &free);
            break;
        default:
            log_error(logger_cpu, "Error al recibir el tamanio de pagina desde memoria");
            exit(-1);
    }
}

void atender_cpu_kernel_dispatch(void) {
    while (control_key) {
        int cod_op = recibir_operacion(fd_kernel_dispatch);
        switch (cod_op) {
        case PCB:
            t_pcb* pcb = deserializar_pcb(fd_kernel_dispatch);
            pcb_actual = pcb;
            printf("PCB Recibido: %d\n", pcb->pid);
            ejecutar_proceso(pcb);
            send_pcb(pcb, fd_kernel_dispatch);
            pcb_actual = NULL;
            liberar_pcb(pcb);
            break;
        case DESCONEXION:
            log_error(logger_cpu, "Desconexion de Kernel - Dispatch");
            control_key = 0;
            break;
        default:
            log_warning(logger_cpu, "Operacion desconocida de Kernel - Dispatch");
            break;
        }
    }
}

// finalizar_proceso 3

void actualizar_motivo_salida(t_motivo_salida motivo) {
    if(pcb_actual->motivo_salida == FINALIZACION_USUARIO) return;
    if(motivo == FIN_QUANTUM && pcb_actual->motivo_salida != -1) return;
    pcb_actual->motivo_salida = motivo;
}

void atender_cpu_kernel_interrupt() {
    while (control_key) {
        // sem_wait(&hay_interrupcion);
        int cod_op = recibir_operacion(fd_kernel_interrupt);
        switch (cod_op) {
        case INTERRUPCION_QUANTUM:
            t_list* lista = recibir_paquete(fd_kernel_interrupt);
            int pid = *(int*)list_get(lista, 0);
            if (pid == pcb_actual->pid)
                actualizar_motivo_salida(FIN_QUANTUM);
            list_destroy_and_destroy_elements(lista, &free);
            break;
        case INTERRUPCION_USUARIO:
            t_list* lista_ = recibir_paquete(fd_kernel_interrupt);
            int pid_ = *(int*)list_get(lista_, 0);
            if (pid_ == pcb_actual->pid)
                actualizar_motivo_salida(FINALIZACION_USUARIO);
            list_destroy_and_destroy_elements(lista_, &free);
            break;
        case DESCONEXION:
            log_error(logger_cpu, "Desconexion de Kernel - Interrupt");
            control_key = 0;
            break;
        default:
            log_warning(logger_cpu, "Operacion desconocida de Kernel - Interrupt");
            break;
        }
    }
}

void incrementar_el_pc() {
    t_registro_32 *reg = (t_registro_32 *)dictionary_get(registros_cpu, "PC");
    reg->valor += 1;
}

int devolver_tamanio_registro(char *key) {
    return es_de_8_bits(key) ? 1 : 4;
}

void ejecutar_proceso(t_pcb *pcb) {
    pthread_mutex_lock(&mutex_registros);
    actualizar_registros(registros_cpu, pcb->registros);
    pthread_mutex_unlock(&mutex_registros);

    pcb->motivo_salida = -1;

    bool esta_activo = true;

    t_temporal *cronometro = temporal_create();

    while (esta_activo) {

        char *instruccion = fetch(pcb);

        char **instruccion_separada = decode(instruccion);

        execute(instruccion_separada, pcb);

        // check_interrupt(pcb);

        free(instruccion);


        pthread_mutex_lock(&mutex_pcb);
        if (pcb->motivo_salida != -1) {
            esta_activo = false;
        }
        pthread_mutex_unlock(&mutex_pcb);

        string_array_destroy(instruccion_separada);
    }
    pcb->quantum = (int)(temporal_gettime(cronometro));
    temporal_destroy(cronometro);
    pthread_mutex_lock(&mutex_registros);
    actualizar_registros(pcb->registros, registros_cpu);
    pthread_mutex_unlock(&mutex_registros);
}
// MULTIPROGRAMACION 3
char *fetch(t_pcb *pcb) {
    int pid = pcb->pid;

    pthread_mutex_lock(&mutex_registros);
    int pc = devolver_valor_registro(registros_cpu, "PC");
    pthread_mutex_unlock(&mutex_registros);

    log_info(logger_cpu_obligatorio, "PID: %d - FETCH - Program Counter: %d", pid, pc);

    send_memoria_pc(pid, pc, fd_memoria);

    op_code cod = recibir_operacion(fd_memoria);

    switch (cod) {
        case INSTRUCCION_A_EJECUTAR:
            char *instruccion = recibir_instruccion_memoria(fd_memoria);
            log_info(logger_cpu_obligatorio, "PID: %d - Ejecutando: %s", pid, instruccion);
            return instruccion;
            break;
        default:
            log_error(logger_cpu, "Error a la hora de pedir la instruccion");
            exit(-1);
            break;
    }
}

char **decode(char *instruccionRecibida) {
    return string_split(instruccionRecibida, " ");
}

void log_lectura_escritura_memoria_int(int pid, char* accion, int dir, int dato) {
    log_info(logger_cpu_obligatorio, "PID: %d - Acción: %s - Dirección Física: %d - Valor: %d", pid, accion, dir, dato);
}

void log_lectura_escritura_memoria_string(int pid, char* accion, int dir, char* dato) {
    log_info(logger_cpu_obligatorio, "PID: %d - Acción: %s - Dirección Física: %d - Valor: %s", pid, accion, dir, dato);
}

t_instr_cpu verificar_tipo_instruccion(char *inst) {
    if (string_equals_ignore_case(inst, "SET"))
        return SET;
    if (string_equals_ignore_case(inst, "SUM"))
        return SUM;
    if (string_equals_ignore_case(inst, "SUB"))
        return SUB;
    if (string_equals_ignore_case(inst, "JNZ"))
        return JNZ;
    if (string_equals_ignore_case(inst, "IO_GEN_SLEEP"))
        return IO_GEN_SLEEP;
    if (string_equals_ignore_case(inst, "RESIZE"))
        return RESIZE;
    if (string_equals_ignore_case(inst, "MOV_IN"))
        return MOV_IN;
    if (string_equals_ignore_case(inst, "MOV_OUT"))
        return MOV_OUT;
    if (string_equals_ignore_case(inst, "COPY_STRING"))
        return COPY_STRING;
    if (string_equals_ignore_case(inst, "IO_STDIN_READ"))
        return IO_STDIN_READ;
    if (string_equals_ignore_case(inst, "IO_STDOUT_WRITE"))
        return IO_STDOUT_WRITE;
    if (string_equals_ignore_case(inst, "WAIT"))
        return WAIT;
    if (string_equals_ignore_case(inst, "SIGNAL"))
        return SIGNAL;
    if (string_equals_ignore_case(inst, "IO_FS_CREATE"))
        return IO_FS_CREATE;
    if (string_equals_ignore_case(inst, "IO_FS_DELETE"))
        return IO_FS_DELETE;
    if (string_equals_ignore_case(inst, "IO_FS_TRUNCATE"))
        return IO_FS_TRUNCATE;
    if (string_equals_ignore_case(inst, "IO_FS_WRITE"))
        return IO_FS_WRITE;
    if (string_equals_ignore_case(inst, "IO_FS_READ"))
        return IO_FS_READ;
    else
        return EXIT;
}

void ejecutar_set(char *registro, char *valor_s) {
    int valor = (int)strtol(valor_s, NULL, 10);
    modificar_registro(registros_cpu, registro, valor);
    incrementar_el_pc();
}

void ejecutar_sum(char *registro_destino, char *registro_origen) {
    int valor_destino = devolver_valor_registro(registros_cpu, registro_destino);
    int valor_origen = devolver_valor_registro(registros_cpu, registro_origen);
    valor_destino += valor_origen;
    modificar_registro(registros_cpu, registro_destino, valor_destino);
    incrementar_el_pc();
}

void ejecutar_sub(char *registro_destino, char *registro_origen) {
    int valor_destino = devolver_valor_registro(registros_cpu, registro_destino);
    int valor_origen = devolver_valor_registro(registros_cpu, registro_origen);
    valor_destino -= valor_origen;
    modificar_registro(registros_cpu, registro_destino, valor_destino);
    incrementar_el_pc();
}

void ejecutar_jnz(char *registro, char *instruccion_s) {
    int instruccion = (int)strtol(instruccion_s, NULL, 10);
    int valor_jnz = devolver_valor_registro(registros_cpu, registro);
    if (valor_jnz != 0)
        modificar_registro(registros_cpu, "PC", instruccion);
    else
        incrementar_el_pc();
}

void ejecutar_io_gen_sleep(t_pcb *pcb, char** parametros) {
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_GEN_SLEEP;
    list_add(pcb->datos_bloqueo->argumentos, strdup(parametros[2]));
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_resize(t_pcb* pcb, char* nuevo_tamanio_s) {
    int nuevo_tamanio = (int)strtol(nuevo_tamanio_s, NULL, 10);
    send_resize(fd_memoria, pcb->pid, nuevo_tamanio);
    int resp_resize = recibir_operacion(fd_memoria);
    switch (resp_resize) {
        case OK:
            int size;
            free(recibir_buffer(&size, fd_memoria));
            log_info(logger_cpu, "Resize completado");
            incrementar_el_pc();
            break;
        case OUT_OF_MEMORY_AVISO:
            log_error(logger_cpu, "No hay marcos disponibles");
            pcb->motivo_salida = OUT_OF_MEMORY;
            break;
        default:
            log_error(logger_cpu, "Codigo desconocido");
            break;
    }
}

t_list* obtener_dir_logicas(int dir_inicial, int tamanio_total) {
    t_list* dirs = list_create();
    int tamanio_restante = tamanio_total;

    //
    int num_pagina = (int)floor(dir_inicial / tam_pagina);
    int desplazamiento = dir_inicial - num_pagina * tam_pagina;

    // Primer pagina
    t_dir* dir_1 = malloc(sizeof(t_dir));
    dir_1->nro_pagina = num_pagina;
    dir_1->desplazamiento = desplazamiento;
    int espacio_restante = tam_pagina - desplazamiento;
    if(espacio_restante > tamanio_total)
        dir_1->tamanio = tamanio_total;
    else dir_1->tamanio = espacio_restante;
    tamanio_restante-=dir_1->tamanio;
    list_add(dirs, dir_1);

    while(tamanio_restante >= tam_pagina) {
        t_dir* dir_n = malloc(sizeof(t_dir));
        num_pagina++;
        dir_n->nro_pagina = num_pagina;
        dir_n->desplazamiento = 0;
        dir_n->tamanio = tam_pagina;
        tamanio_restante-=dir_n->tamanio;
        list_add(dirs, dir_n);
    }

    if(tamanio_restante > 0) {
        t_dir* dir_final = malloc(sizeof(t_dir));
        num_pagina++;
        dir_final->nro_pagina = num_pagina;
        dir_final->desplazamiento = 0;
        dir_final->tamanio = tamanio_restante;
        list_add(dirs, dir_final);
    }

    return dirs;
}

void ejecutar_mov_in(t_pcb* pcb, char* registro_datos, char* registro_direccion) {
    int dir_inicial = devolver_valor_registro(registros_cpu, registro_direccion);
    int tamanio = devolver_tamanio_registro(registro_datos);
    int nro_pagina = (int)floor(dir_inicial / tam_pagina);
    int desplazamiento = dir_inicial - nro_pagina * tam_pagina;
    if(tamanio == 1) {
        int direccion_fisica = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
        void* dato = pedir_leer_memoria(fd_memoria, direccion_fisica, tamanio, pcb->pid);
        log_info(logger_cpu_obligatorio, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica, *(uint8_t*)dato);
        modificar_registro(registros_cpu, registro_datos, *(int*)dato);
        free(dato);
    }
    else {
        int espacio_restante = tam_pagina - desplazamiento;
        if(espacio_restante >= tamanio) {
            int direccion_fisica = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
            void* dato = pedir_leer_memoria(fd_memoria, direccion_fisica, tamanio, pcb->pid);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica, *(int*)dato);
            modificar_registro(registros_cpu, registro_datos, *(int*)dato);
            free(dato);
        }
        else {
            int direccion_fisica_1 = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
            void* dato_parte_1 = pedir_leer_memoria(fd_memoria, direccion_fisica_1, espacio_restante, pcb->pid);

            int direccion_fisica_2 = consultar_mmu(pcb->pid, nro_pagina + 1, 0);
            void* dato_parte_2 = pedir_leer_memoria(fd_memoria, direccion_fisica_2, tamanio - espacio_restante, pcb->pid);

            uint32_t dato_reconstruido = 0;
            void* dato_reconstruido_ptr = &dato_reconstruido;
            uint32_t dato_reconstruido_2 = 0;
            void* dato_reconstruido_ptr_2 = &dato_reconstruido_2;

            memcpy(dato_reconstruido_ptr, dato_parte_1, espacio_restante);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica_1, dato_reconstruido);
            memcpy((char*)(dato_reconstruido_ptr + espacio_restante), dato_parte_2, tamanio - espacio_restante);
            memcpy(dato_reconstruido_ptr_2, dato_parte_2, tamanio - espacio_restante);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica_2, dato_reconstruido_2);

            modificar_registro(registros_cpu, registro_datos, dato_reconstruido);
            free(dato_parte_1);
            free(dato_parte_2);
        }
    }
    incrementar_el_pc();
}

void ejecutar_mov_out(t_pcb* pcb, char* registro_direccion, char* registro_datos) {
    int dir_inicial = devolver_valor_registro(registros_cpu, registro_direccion);
    int tamanio = devolver_tamanio_registro(registro_datos);
    int nro_pagina = (int)floor(dir_inicial / tam_pagina);
    int desplazamiento = dir_inicial - nro_pagina * tam_pagina;
    if(tamanio == 1) {
        uint8_t dato_a_escribir = devolver_valor_registro(registros_cpu, registro_datos);
        int direccion_fisica = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
        log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica, dato_a_escribir);
        pedir_escribir_memoria(fd_memoria, direccion_fisica, tamanio, &dato_a_escribir, pcb->pid);
    }
    else {
        int espacio_restante = tam_pagina - desplazamiento;
        if(espacio_restante >= tamanio) {
            uint32_t dato_a_escribir = devolver_valor_registro(registros_cpu, registro_datos);
            int direccion_fisica = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica, dato_a_escribir);
            pedir_escribir_memoria(fd_memoria, direccion_fisica, tamanio, &dato_a_escribir, pcb->pid);
        }
        else {
            int dato = devolver_valor_registro(registros_cpu, registro_datos);
            void* dato_ptr = &dato;
            
            mem_hexdump((void*)dato_ptr, 4);

            int dato_parte_1 = 0;
            void* dato_parte_1_ptr = &dato_parte_1;
            memcpy(dato_parte_1_ptr, dato_ptr, espacio_restante);
            int direccion_fisica_1 = consultar_mmu(pcb->pid, nro_pagina, desplazamiento);
            // mem_hexdump((void*)dato_parte_1_ptr, espacio_restante);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica_1, dato_parte_1);
            pedir_escribir_memoria(fd_memoria, direccion_fisica_1, espacio_restante, dato_parte_1_ptr, pcb->pid);
            
            int dato_parte_2 = 0;
            void* dato_parte_2_ptr = &dato_parte_2;
            memcpy(dato_parte_2_ptr, (char*)(dato_ptr + espacio_restante), tamanio - espacio_restante);
            int direccion_fisica_2 = consultar_mmu(pcb->pid, nro_pagina + 1, 0);
            // mem_hexdump((void*)dato_parte_2_ptr, tamanio - espacio_restante);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", pcb->pid, direccion_fisica_2, dato_parte_2);
            pedir_escribir_memoria(fd_memoria, direccion_fisica_2, tamanio - espacio_restante, dato_parte_2_ptr, pcb->pid);
        }
    }
    incrementar_el_pc();
}

void ejecutar_copy_string(t_pcb* pcb, char* tamanio_s) {
    int dir_logica_si = devolver_valor_registro(registros_cpu, "SI");
    int tamanio = atoi(tamanio_s);

    t_list* dir_logicas_si = obtener_dir_logicas(dir_logica_si, tamanio);

    char* string_leido = malloc(tamanio + 1);
    memset(string_leido, '\0', tamanio + 1);

    // Lee
    for(int i = 0; i < list_size(dir_logicas_si); i++) {
        t_dir* dir_n = list_get(dir_logicas_si, i);
        int dir_fisica = consultar_mmu(pcb->pid, dir_n->nro_pagina, dir_n->desplazamiento);
        void* dato_parcial = pedir_leer_memoria(fd_memoria, dir_fisica, dir_n->tamanio, pcb->pid);
        char* string_parcial = malloc(dir_n->tamanio + 1);
        memcpy(string_parcial, dato_parcial, dir_n->tamanio);
        string_parcial[dir_n->tamanio] = '\0';
        log_info(logger_cpu_obligatorio, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %s", pcb->pid, dir_fisica, string_parcial);
        string_append(&string_leido, string_parcial);
        free(string_parcial);
        free(dato_parcial);
    }

    // log_error(logger_cpu, "String: %s", string_leido);

    list_destroy_and_destroy_elements(dir_logicas_si, &free);

    int dir_logica_di = devolver_valor_registro(registros_cpu, "DI");

    t_list* dir_logicas_di = obtener_dir_logicas(dir_logica_di, tamanio);

    int pos_inicial = 0;

    // Escribe
    for(int i = 0; i < list_size(dir_logicas_di); i++) {
        t_dir* dir_n = list_get(dir_logicas_di, i);
        int dir_fisica = consultar_mmu(pcb->pid, dir_n->nro_pagina, dir_n->desplazamiento);

        void* dato = malloc(dir_n->tamanio);
        memset(dato, 0, dir_n->tamanio);
        int tam_restante = string_length(string_leido) - pos_inicial;
        if(tam_restante > dir_n->tamanio) {
            char* substring = string_substring(string_leido, pos_inicial, dir_n->tamanio);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", pcb->pid, dir_fisica, substring);
            memcpy(dato, substring, dir_n->tamanio);
            free(substring);
            pos_inicial += dir_n->tamanio;
        }
        else {
            char* substring = string_substring(string_leido, pos_inicial, tam_restante);
            log_info(logger_cpu_obligatorio, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", pcb->pid, dir_fisica, substring);
            memcpy(dato, substring, tam_restante);
            free(substring);
            pos_inicial += dir_n->tamanio;
        }
        
        pedir_escribir_memoria(fd_memoria, dir_fisica, dir_n->tamanio, dato, pcb->pid);
        free(dato);
    }

    list_destroy_and_destroy_elements(dir_logicas_di, &free);

    free(string_leido);

    incrementar_el_pc();
}

void ejecutar_wait(t_pcb* pcb, char* recurso) {
    actualizar_motivo_salida(BLOQUEO_RECURSO);
    pcb->datos_bloqueo->nombre = strdup(recurso);
    pcb->datos_bloqueo->instruccion = WAIT;
    incrementar_el_pc();
}

void ejecutar_signal(t_pcb* pcb, char* recurso) {
    actualizar_motivo_salida(BLOQUEO_RECURSO);
    pcb->datos_bloqueo->nombre = strdup(recurso);
    pcb->datos_bloqueo->instruccion = SIGNAL;
    incrementar_el_pc();
}

void ejecutar_io_stdin_read(t_pcb *pcb, char** parametros) {
    char* reg_dir = parametros[2];
    char* reg_tam = parametros[3];
    int dir_logica = devolver_valor_registro(registros_cpu, reg_dir);
    int tamanio = devolver_valor_registro(registros_cpu, reg_tam);
    t_list* dir_logicas = obtener_dir_logicas(dir_logica, tamanio);
    for(int i = 0; i < list_size(dir_logicas); i++) {
        t_dir* dir_log = list_get(dir_logicas, i);
        t_dir_fisica* dir_fisica = malloc(sizeof(t_dir_fisica));
        dir_fisica->dir = consultar_mmu(pcb->pid, dir_log->nro_pagina, dir_log->desplazamiento);
        dir_fisica->tam = dir_log->tamanio;
        log_info(logger_cpu, "Dir: %d, Tam: %d", dir_fisica->dir, dir_fisica->tam);
        list_add(pcb->datos_bloqueo->dir_fisicas, dir_fisica);
    }
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_STDIN_READ;
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_stdout_write(t_pcb *pcb, char** parametros) {
    char* reg_dir = parametros[2];
    char* reg_tam = parametros[3];
    int dir_logica = devolver_valor_registro(registros_cpu, reg_dir);
    int tamanio = devolver_valor_registro(registros_cpu, reg_tam);
    t_list* dir_logicas = obtener_dir_logicas(dir_logica, tamanio);
    for(int i = 0; i < list_size(dir_logicas); i++) {
        t_dir* dir_log = list_get(dir_logicas, i);
        t_dir_fisica* dir_fisica = malloc(sizeof(t_dir_fisica));
        dir_fisica->dir = consultar_mmu(pcb->pid, dir_log->nro_pagina, dir_log->desplazamiento);
        dir_fisica->tam = dir_log->tamanio;
        log_info(logger_cpu, "Dir: %d, Tam: %d", dir_fisica->dir, dir_fisica->tam);
        list_add(pcb->datos_bloqueo->dir_fisicas, dir_fisica);
    }
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_STDOUT_WRITE;
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_fs_create(t_pcb *pcb, char** parametros) {
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_FS_CREATE;
    char* nombre_archivo = parametros[2];
    list_add(pcb->datos_bloqueo->argumentos, nombre_archivo);
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_fs_delete(t_pcb *pcb, char** parametros) {
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_FS_DELETE;
    char* nombre_archivo = parametros[2];
    list_add(pcb->datos_bloqueo->argumentos, nombre_archivo);
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_fs_truncate(t_pcb *pcb, char** parametros) {
    char* nombre_archivo = parametros[2];
    char* reg_tam = parametros[3];
    int tamanio = devolver_valor_registro(registros_cpu, reg_tam);
    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_FS_TRUNCATE;
    char* tamanio_s = string_itoa(tamanio);
    list_add(pcb->datos_bloqueo->argumentos, nombre_archivo);
    list_add(pcb->datos_bloqueo->argumentos, tamanio_s);
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_fs_write(t_pcb *pcb, char** parametros) {
    char* nombre_archivo = parametros[2];
    char* reg_dir = parametros[3];
    char* reg_tam = parametros[4];
    char* reg_ptr = parametros[5];
    int tamanio = devolver_valor_registro(registros_cpu, reg_tam);
    int dir_logica = devolver_valor_registro(registros_cpu, reg_dir);
    int puntero_archivo = devolver_valor_registro(registros_cpu, reg_ptr);

    // Una direccion para cada pagina a escribir
    t_list* dir_logicas = obtener_dir_logicas(dir_logica, tamanio);

    for(int i = 0; i < list_size(dir_logicas); i++) {
        t_dir* dir_log = list_get(dir_logicas, i);
        t_dir_fisica* dir_fisica = malloc(sizeof(t_dir_fisica));
        dir_fisica->dir = consultar_mmu(pcb->pid, dir_log->nro_pagina, dir_log->desplazamiento);
        dir_fisica->tam = dir_log->tamanio;
        log_info(logger_cpu, "Dir: %d, Tam: %d", dir_fisica->dir, dir_fisica->tam);
        list_add(pcb->datos_bloqueo->dir_fisicas, dir_fisica);
    }

    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_FS_WRITE;
    char* tam_s = string_itoa(tamanio);
    char* ptr_s = string_itoa(puntero_archivo);
    list_add(pcb->datos_bloqueo->argumentos, nombre_archivo);
    list_add(pcb->datos_bloqueo->argumentos, tam_s);
    list_add(pcb->datos_bloqueo->argumentos, ptr_s);
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void ejecutar_io_fs_read(t_pcb *pcb, char** parametros) {
    char* nombre_archivo = parametros[2];
    char* reg_dir = parametros[3];
    char* reg_tam = parametros[4];
    char* reg_ptr = parametros[5];
    int tamanio = devolver_valor_registro(registros_cpu, reg_tam);
    int dir_logica = devolver_valor_registro(registros_cpu, reg_dir);
    int puntero_archivo = devolver_valor_registro(registros_cpu, reg_ptr);

    // Una direccion para cada pagina a escribir
    t_list* dir_logicas = obtener_dir_logicas(dir_logica, tamanio);

    for(int i = 0; i < list_size(dir_logicas); i++) {
        t_dir* dir_log = list_get(dir_logicas, i);
        t_dir_fisica* dir_fisica = malloc(sizeof(t_dir_fisica));
        dir_fisica->dir = consultar_mmu(pcb->pid, dir_log->nro_pagina, dir_log->desplazamiento);
        dir_fisica->tam = dir_log->tamanio;
        log_info(logger_cpu, "Dir: %d, Tam: %d", dir_fisica->dir, dir_fisica->tam);
        list_add(pcb->datos_bloqueo->dir_fisicas, dir_fisica);
    }

    pcb->datos_bloqueo->nombre = strdup(parametros[1]);
    pcb->datos_bloqueo->instruccion = IO_FS_READ;
    char* tam_s = string_itoa(tamanio);
    char* ptr_s = string_itoa(puntero_archivo);
    list_add(pcb->datos_bloqueo->argumentos, nombre_archivo);
    list_add(pcb->datos_bloqueo->argumentos, tam_s);
    list_add(pcb->datos_bloqueo->argumentos, ptr_s);
    actualizar_motivo_salida(BLOQUEO_IO);
    incrementar_el_pc();
}

void execute(char **instruccion, t_pcb *pcb) {
    for (int i = 0; instruccion[i] != NULL; i++) {
        string_trim(&instruccion[i]); // Buscar forma de solucionar esto
    }
    char *id = instruccion[0];
    t_instr_cpu cod = verificar_tipo_instruccion(id);
    switch (cod) {
        case SET:
            ejecutar_set(instruccion[1], instruccion[2]);
            break;
        case SUM:
            ejecutar_sum(instruccion[1], instruccion[2]);
            break;
        case SUB:
            ejecutar_sub(instruccion[1], instruccion[2]);
            break;
        case JNZ:
            ejecutar_jnz(instruccion[1], instruccion[2]);
            break;
        case IO_GEN_SLEEP:
            ejecutar_io_gen_sleep(pcb, instruccion);
            break;
        case RESIZE:
            ejecutar_resize(pcb, instruccion[1]);
            break;
        case MOV_IN:
            ejecutar_mov_in(pcb, instruccion[1], instruccion[2]);
            break;
        case MOV_OUT:
            ejecutar_mov_out(pcb, instruccion[1], instruccion[2]);
            break;
        case COPY_STRING:
            ejecutar_copy_string(pcb, instruccion[1]);
            break;
        case WAIT:
            ejecutar_wait(pcb, instruccion[1]);
            break;
        case SIGNAL:
            ejecutar_signal(pcb, instruccion[1]);
            break;
        case IO_STDIN_READ:
            ejecutar_io_stdin_read(pcb, instruccion);
            break;
        case IO_STDOUT_WRITE:
            ejecutar_io_stdout_write(pcb, instruccion);
            break;
        case IO_FS_CREATE:
            ejecutar_io_fs_create(pcb, instruccion);
            break;
        case IO_FS_DELETE:
            ejecutar_io_fs_delete(pcb, instruccion);
            break;
        case IO_FS_TRUNCATE:
            ejecutar_io_fs_truncate(pcb, instruccion);
            break;
        case IO_FS_WRITE:
            ejecutar_io_fs_write(pcb, instruccion);
            break;
        case IO_FS_READ:
            ejecutar_io_fs_read(pcb, instruccion);
            break;
        case EXIT:
            log_info(logger_cpu, "Ha finalizado el proceso");
            pcb->motivo_salida = FIN_EXITOSO;
            break;
        default:
            break;
        }
    // string_array_destroy(instruccion);
}

t_entrada_tlb *devolver_entrada_tlb(int pid, int num_pagina)
{
    for (int i = 0; i < list_size(tlb); i++) {
        t_entrada_tlb *entrada = list_get(tlb, i);
        if (entrada->pid == pid && entrada->nro_pagina == num_pagina)
            return entrada;
    }

    return NULL;
}

int pedir_marco_a_memoria(int pid, int num_pagina) {
    t_paquete *paquete = crear_paquete(MARCO);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &num_pagina, sizeof(int));
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    op_code cod = recibir_operacion(fd_memoria);
    if (cod == MARCO) {
        t_list *lista = recibir_paquete(fd_memoria);
        int marco = *(int *)list_get(lista, 0);
        list_destroy_and_destroy_elements(lista, &free);
        return marco;
    }
    else
        exit(-1);
}

t_entrada_tlb *entrada_menos_usada_ultimamente() {
    t_entrada_tlb *entrada_lru = (t_entrada_tlb *)list_get(tlb, 0);
    for (int i = 1; i < CONFIG_CPU.CANTIDAD_ENTRADAS_TLB; i++) {
        t_entrada_tlb *sig_entrada = (t_entrada_tlb *)list_get(tlb, i);
        if (sig_entrada->ultimo_acceso < entrada_lru->ultimo_acceso)
            entrada_lru = sig_entrada;
    }
    return entrada_lru;
}

void agregar_lru_tlb(t_entrada_tlb *entrada_nueva) {
    t_entrada_tlb *entrada_vieja = entrada_menos_usada_ultimamente();
    list_remove_element(tlb, entrada_vieja);
    log_info(logger_cpu, "Sale de TLB: PID %d - Pagina %d - Marco %d", entrada_vieja->pid, entrada_vieja->nro_pagina, entrada_vieja->nro_pagina);
    free(entrada_vieja);
    list_add(tlb, entrada_nueva);
}

void agregar_fifo_tlb(t_entrada_tlb *entrada_nueva) {
    t_entrada_tlb* se_retira = list_remove(tlb, 0);
    log_info(logger_cpu, "Sale de TLB: PID %d - Pagina %d - Marco %d", se_retira->pid, se_retira->nro_pagina, se_retira->nro_pagina);
    free(se_retira);
    list_add(tlb, entrada_nueva);
}

int consultar_mmu(int pid, int num_pagina, int desplazamiento) {
    int num_marco;
    t_entrada_tlb *entrada = devolver_entrada_tlb(pid, num_pagina);
    if (entrada != NULL) {
        log_info(logger_cpu_obligatorio, "PID: %d - TLB HIT - Pagina: %d", pid, num_pagina);
        num_marco = entrada->nro_marco;
        // entrada->ultimo_acceso = temporal_create();
        entrada->ultimo_acceso = time(NULL);
    }
    else {
        log_info(logger_cpu_obligatorio, "PID: %d - TLB MISS - Pagina: %d", pid, num_pagina);
        num_marco = pedir_marco_a_memoria(pid, num_pagina);
        // Agregar a la tlb y tmb implementar algoritmos de tlb.
        t_entrada_tlb *entrada = malloc(sizeof(t_entrada_tlb));
        entrada->pid = pid;
        entrada->nro_pagina = num_pagina;
        entrada->nro_marco = num_marco;
        // entrada->ultimo_acceso = temporal_create();
        entrada->ultimo_acceso = time(NULL);
        if (CONFIG_CPU.CANTIDAD_ENTRADAS_TLB > 0) {
            if (list_size(tlb) < CONFIG_CPU.CANTIDAD_ENTRADAS_TLB)
                list_add(tlb, entrada);
            else {
                printf("Algoritmo: %s", CONFIG_CPU.ALGORITMO_TLB);
                if (string_equals_ignore_case(CONFIG_CPU.ALGORITMO_TLB, "FIFO"))
                    agregar_fifo_tlb(entrada);
                else if (string_equals_ignore_case(CONFIG_CPU.ALGORITMO_TLB, "LRU"))
                    agregar_lru_tlb(entrada);
                else {
                    log_error(logger_cpu, "Algoritmo de TLB incorrecto");
                    exit(-1);
                }
            }
        }
            
    }
    log_info(logger_cpu_obligatorio, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid, num_pagina, num_marco);
    return num_marco * tam_pagina + desplazamiento;
}
