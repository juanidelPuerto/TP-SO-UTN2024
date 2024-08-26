#include "../include/inicializar.h"

t_algoritmo convertir_algoritmo(char* algoritmo) {
    if(string_equals_ignore_case(algoritmo, "FIFO")) return FIFO;
    else if(string_equals_ignore_case(algoritmo, "RR")) return RR;
    else if(string_equals_ignore_case(algoritmo, "VRR")) return VRR;
    else {
        log_error(logger_kernel, "No hay un algoritmo de planificacion especificado");
        exit(-1);
    }
}

void cargar_configuracion(t_config* config_kernel, t_config* config_ips) {

    if (config_kernel == NULL) {
        log_error(logger_kernel, "No se encontro kernel.config");
        exit(-1);
    }

    if (config_kernel == NULL) {
        log_error(logger_kernel, "No se encontro ips.config");
        exit(-1);
    }

    KERNEL_CONFIG->IP_CPU = config_get_string_value(config_ips, "IP_CPU");
    KERNEL_CONFIG->IP_MEMORIA = config_get_string_value(config_ips, "IP_MEMORIA");
    
    KERNEL_CONFIG->PUERTO_ESCUCHA = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");
    KERNEL_CONFIG->PUERTO_CPU_DISPATCH = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");
    KERNEL_CONFIG->PUERTO_CPU_INTERRUPT = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");
    KERNEL_CONFIG->PUERTO_MEMORIA = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    
    char* algoritmo = config_get_string_value(config_kernel, "ALGORITMO_PLANIFICACION");
    KERNEL_CONFIG->ALGORITMO_PLANIFICACION = convertir_algoritmo(algoritmo);
    free(algoritmo);
    
    KERNEL_CONFIG->QUANTUM = config_get_int_value(config_kernel, "QUANTUM");
    KERNEL_CONFIG->RECURSOS = config_get_array_value(config_kernel, "RECURSOS");
    KERNEL_CONFIG->INSTANCIAS_RECURSOS = config_get_array_value(config_kernel, "INSTANCIAS_RECURSOS");
    KERNEL_CONFIG->GRADO_MULTIPROGRAMACION = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");

    log_info(logger_kernel, "Archivo de configuracion cargado correctamente");
}

void generar_conexion() {
    
    // Inicio servidor
    fd_kernel = iniciar_servidor(logger_kernel, "KERNEL", KERNEL_CONFIG->PUERTO_ESCUCHA);

    // Me conecto a memoria
    fd_memoria = crear_conexion(logger_kernel, KERNEL_CONFIG->IP_MEMORIA, KERNEL_CONFIG->PUERTO_MEMORIA);
    log_info(logger_kernel, "Se conecto a MEMORIA con exito!!");

    // Me conecto a CPU
    fd_cpu_dispatch = crear_conexion(logger_kernel, KERNEL_CONFIG->IP_CPU, KERNEL_CONFIG->PUERTO_CPU_DISPATCH);
    log_info(logger_kernel, "Se conecto a CPU-DISPATCH con exito!!");
    fd_cpu_interrupt = crear_conexion(logger_kernel, KERNEL_CONFIG->IP_CPU, KERNEL_CONFIG->PUERTO_CPU_INTERRUPT);
    log_info(logger_kernel, "Se conecto a CPU-INTERRUPT con exito!!");
    
}

void inicializar_colas() {
    lista_pcb = list_create();
    lista_recursos_procesos = list_create();

    COLA_NEW = list_create();
    COLA_READY = list_create();
    COLA_READY_AUX = list_create();
    COLA_EXIT = list_create();
    COLA_EXEC = list_create();
}

void inicializar_semaforos() {
    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_ready_aux, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_kernel_io, NULL);
    pthread_mutex_init(&mutex_lista_pcb, NULL);
    pthread_mutex_init(&mutex_recursos, NULL);
    pthread_mutex_init(&mutex_recursosXProcesos, NULL);
    pthread_mutex_init(&mutex_recibir_pcb, NULL);
    pthread_mutex_init(&mutex_ignorar_signal, NULL);
    pthread_mutex_init(&mutex_dicc_io, NULL);
    pthread_mutex_init(&cant_proc_multip, NULL);

    sem_init(&procesos_en_new, 0, 0);
    sem_init(&procesos_en_ready, 0, 0);
    sem_init(&procesos_en_exit, 0, 0);
    sem_init(&planificador_activo, 0, 0);
    sem_init(&proceso_ejecutando, 0, 1);
    sem_init(&sem_planificacion_new, 0, 1);
    sem_init(&sem_planificacion_ready, 0, 1);
    sem_init(&sem_planificacion_exec, 0, 1);
    sem_init(&sem_planificacion_exit, 0, 1);
    sem_init(&sem_multiprogramacion, 0, KERNEL_CONFIG->GRADO_MULTIPROGRAMACION);
}

void destruir_semaforos() {
    pthread_mutex_destroy(&mutex_cola_new);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_ready_aux);
    pthread_mutex_destroy(&mutex_cola_exec);
    pthread_mutex_destroy(&mutex_cola_exit);
    pthread_mutex_destroy(&mutex_kernel_io);
    pthread_mutex_destroy(&mutex_lista_pcb);
    pthread_mutex_destroy(&mutex_recursos);
    pthread_mutex_destroy(&mutex_recursosXProcesos);
    pthread_mutex_destroy(&mutex_recibir_pcb);
    pthread_mutex_destroy(&mutex_ignorar_signal);
    pthread_mutex_destroy(&mutex_dicc_io);
    pthread_mutex_destroy(&cant_proc_multip);


    sem_destroy(&procesos_en_ready);
    sem_destroy(&procesos_en_new);
    sem_destroy(&procesos_en_exit);
    sem_destroy(&planificador_activo);
    sem_destroy(&proceso_ejecutando);
    sem_destroy(&sem_planificacion_new);
    sem_destroy(&sem_planificacion_ready);
    sem_destroy(&sem_planificacion_exec);
    sem_destroy(&sem_planificacion_exit);
    sem_destroy(&sem_multiprogramacion);
}

void inicializar_recursos(){
    recursos = dictionary_create();
    for (int i = 0; KERNEL_CONFIG->RECURSOS[i] != NULL; i++) {
        t_recurso* recurso = malloc(sizeof(t_recurso));
        recurso->nombre =  strdup(KERNEL_CONFIG->RECURSOS[i]);  
        recurso->instancias_disponibles = atoi(KERNEL_CONFIG->INSTANCIAS_RECURSOS[i]);
        recurso->cola_bloqueados_recursos = list_create();
        dictionary_put(recursos, recurso->nombre, recurso);
    }
}
