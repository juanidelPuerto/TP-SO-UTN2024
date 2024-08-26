#include "../include/utils_kernel.h"

// Manejo de instrucciones de consola

void iniciar_proceso(char* params) {
    if(!es_un_solo_param(params)) {
        log_error(logger_kernel, "Instrucción no válida. Formato correcto: INICIAR_PROCESO [PATH]");
        return;
    }

    string_trim(&params);
    t_pcb* pcb = crear_pcb();

    pcb->path = params;
    
    log_creacion_proceso(pcb);
    //No faltaria un mutex para la lista
    pthread_mutex_lock(&mutex_lista_pcb);
    list_add(lista_pcb, pcb);
    pthread_mutex_unlock(&mutex_lista_pcb);

    push_cola_new(pcb);

    // send_proceso_memoria(fd_memoria, pcb->pid, params);
}

void finalizar_proceso(char* params) {
    if(!es_un_solo_param(params)) {
        log_error(logger_kernel, "Instrucción no válida. Formato correcto: FINALIZAR_PROCESO [PID]");
        return;
    }

    int pid = atoi(params);

    detener_planificacion(NULL);

    buscar_proceso_y_eliminar(pid);

    iniciar_planificacion(NULL);
}


bool existe_planificador = false;
bool planificador_bloqueado = false;

void iniciar_planificacion(char* param) {
    if (param != NULL) {
        log_error(logger_kernel, "Instrucción no válida. Formato correcto: INICIAR_PLANIFICACION");
        return;
    }
    if(!existe_planificador) {
        iniciar_planificador();
    }
    else if(planificador_bloqueado) {
        log_info(logger_kernel, "Se renaudo la planificacion");
        planificador_bloqueado = false;
        sem_post(&sem_planificacion_new);
        sem_post(&sem_planificacion_ready);
        sem_post(&sem_planificacion_exec);
        sem_post(&sem_planificacion_exit);
        // Agregar io
    }
}

void stop_new() {
    sem_wait(&sem_planificacion_new);
}

void stop_ready() {
    sem_wait(&sem_planificacion_ready);
}

void stop_exec() {
    sem_wait(&sem_planificacion_exec);
}

void stop_exit() {
    sem_wait(&sem_planificacion_exit);
}

void detener_planificacion(char* param) {
    if (param != NULL) {
        log_error(logger_kernel, "Instrucción no válida. Formato correcto: DETENER_PLANIFICACION");
        return;
    }
    if(!planificador_bloqueado) {
        log_info(logger_kernel, "Se detuvo la planificacion");
        planificador_bloqueado = true;
        pthread_t hilo_stop_new;
        pthread_t hilo_stop_ready;
        pthread_t hilo_stop_exec;
        pthread_t hilo_stop_exit;

        pthread_create(&hilo_stop_new, NULL, (void*)stop_new, NULL);
        pthread_create(&hilo_stop_ready, NULL, (void*)stop_ready, NULL);
        pthread_create(&hilo_stop_exec, NULL, (void*)stop_exec, NULL);
        pthread_create(&hilo_stop_exit, NULL, (void*)stop_exit, NULL);

        pthread_detach(hilo_stop_new);
        pthread_detach(hilo_stop_ready);
        pthread_detach(hilo_stop_exec);
        pthread_detach(hilo_stop_exit); // Los tuve que separar porque a veces uno se colgaba
    }
    // Agregar io
}

// Extra

int es_fifo() {
    return KERNEL_CONFIG->ALGORITMO_PLANIFICACION == FIFO;
}

int es_rr() {
    return KERNEL_CONFIG->ALGORITMO_PLANIFICACION == RR;
}

int es_vrr() {
    return KERNEL_CONFIG->ALGORITMO_PLANIFICACION == VRR;
}

void iniciar_planificador() {
    planificador_largo_plazo();

    pthread_t hilo_planificador_corto_plazo;
    if(es_fifo()) {
        log_info(logger_kernel, "Algoritmo: FIFO");
        pthread_create(&hilo_planificador_corto_plazo, NULL, (void*) planificador_corto_plazo_fifo, NULL);
    }
    else if(es_rr()) {
        log_info(logger_kernel, "Algoritmo: RR");
        pthread_create(&hilo_planificador_corto_plazo, NULL, (void*) planificador_corto_plazo_rr, NULL);
    }
     else if(es_vrr()) {
        log_info(logger_kernel, "Algoritmo: VRR");
        pthread_create(&hilo_planificador_corto_plazo, NULL, (void*) planificador_corto_plazo_vrr, NULL);
    }
    pthread_detach(hilo_planificador_corto_plazo);

    existe_planificador = true;
}

void planificador_largo_plazo() {
    pthread_t hilo_manejo_new;
    pthread_create(&hilo_manejo_new, NULL, (void*)manejar_new, NULL);
    pthread_detach(hilo_manejo_new);
    pthread_t hilo_manejar_exit;
    pthread_create(&hilo_manejar_exit, NULL, (void*)manejar_exit, NULL);
    pthread_detach(hilo_manejar_exit);
}

int procesos_en_multiprogramacion = 0;

void manejar_new() {
    while(esta_activo) {
        sem_wait(&sem_multiprogramacion); // Inicia el valor igual al grado de mp
        sem_wait(&procesos_en_new);
        sem_wait(&sem_planificacion_new);

        pthread_mutex_lock(&cant_proc_multip);
        procesos_en_multiprogramacion++;
        pthread_mutex_unlock(&cant_proc_multip);

        t_pcb* pcb = pop_cola_new();

        // Agregar respuesta de memoria
        send_proceso_memoria(fd_memoria, pcb->pid, pcb->path);

        push_cola_ready(pcb);

        sem_post(&sem_planificacion_new);
    }
}

void liberar_recurso(t_recurso* recurso) {
    pthread_mutex_lock(&mutex_recursos);
    recurso->instancias_disponibles++;
    if (recurso->instancias_disponibles <= 0 && list_size(recurso->cola_bloqueados_recursos) > 0) {
        t_pcb* pcb_desbloqueado = pop_cola_bloq(recurso->cola_bloqueados_recursos);
        push_cola_ready(pcb_desbloqueado);
    }
    pthread_mutex_unlock(&mutex_recursos);
}

void manejar_exit() {
    while(esta_activo) {
        sem_wait(&procesos_en_exit);
        sem_wait(&sem_planificacion_exit);

        t_pcb* pcb = pop_cola_exit();

        // Liberar recursos
        pthread_mutex_lock(&mutex_recursosXProcesos);
        for (int i = 0; i < list_size(lista_recursos_procesos); i++) {
            t_recurso_proceso* rp = list_get(lista_recursos_procesos, i);
            if (rp->pid == pcb->pid) {
                t_recurso* recurso = dictionary_get(recursos, rp->nombre_recurso);
                liberar_recurso(recurso);
                list_remove(lista_recursos_procesos, i);
                free(rp->nombre_recurso);
                free(rp);
                i--;
            }
        }
        pthread_mutex_unlock(&mutex_recursosXProcesos);

        send_fin_proceso_memoria(fd_memoria, pcb->pid);

        remover_proceso_en_cola(lista_pcb, pcb->pid);

        mostrar_registros(pcb);

        liberar_pcb(pcb);

        pthread_mutex_lock(&cant_proc_multip);
        procesos_en_multiprogramacion--;
        pthread_mutex_unlock(&cant_proc_multip);

        sem_post(&sem_planificacion_exit);
    }
}

// Unificar planificadores
bool recibiendo_pcb = false;

void planificador_corto_plazo_fifo() {
    while(esta_activo) {
        sem_wait(&procesos_en_ready);
        sem_wait(&proceso_ejecutando);
        sem_wait(&sem_planificacion_ready);

        t_pcb* sgteProceso = pop_cola_ready();
      
        push_cola_exec(sgteProceso);

        sem_post(&proceso_ejecutando);
        sem_post(&sem_planificacion_ready);
    }
}

void planificador_corto_plazo_rr() {
    while(esta_activo) {
        sem_wait(&procesos_en_ready);
        sem_wait(&proceso_ejecutando);
        sem_wait(&sem_planificacion_ready);
       
        t_pcb* sgteProceso = pop_cola_ready();

        push_cola_exec(sgteProceso);

        sem_post(&proceso_ejecutando);
        sem_post(&sem_planificacion_ready);
    } 
}

void planificador_corto_plazo_vrr() {
    while (esta_activo)
    {
        sem_wait(&procesos_en_ready);
        sem_wait(&proceso_ejecutando);
        sem_wait(&sem_planificacion_ready);

        t_pcb *sgteProceso;

        if(list_is_empty(COLA_READY_AUX))
            sgteProceso = pop_cola_ready();
        else
            sgteProceso = pop_cola_ready_aux();

        push_cola_exec(sgteProceso);
        
        sem_post(&proceso_ejecutando);
        sem_post(&sem_planificacion_ready);
    }
}

void ejecutar_fifo(t_pcb* pcb) {
    send_pcb(pcb, fd_cpu_dispatch);

    op_code cod = recibir_operacion(fd_cpu_dispatch);

    switch(cod) {
    case PCB:
        t_pcb* contexto_ejecucion = deserializar_pcb(fd_cpu_dispatch);
        pthread_mutex_lock(&mutex_recibir_pcb);
        recibiendo_pcb = true;
        pthread_mutex_unlock(&mutex_recibir_pcb);
        manejar_proceso(contexto_ejecucion);
        break;
    case DESCONEXION:
        log_error(logger_kernel, "El CPU-DISPATCH se desconecto.");
        break;
    default:
        log_error(logger_kernel, "Operacion desconocida");
        break;
    }
}

bool ya_termino_el_proceso = false;

void iniciar_quantum(void* _args) {
    t_args_hilo_quantum* args = (t_args_hilo_quantum*)_args;
    
    dormir(args->quantum_restante);

    pthread_mutex_lock(&(args->mutex_quantum));
    bool termino_proceso = args->termino_el_proceso;
    pthread_mutex_unlock(&(args->mutex_quantum));

    if(termino_proceso) return;

    t_paquete* paquete = crear_paquete(INTERRUPCION_QUANTUM);
    agregar_a_paquete(paquete, &(args->pid), sizeof(int));
    enviar_paquete(paquete, fd_cpu_interrupt);
    eliminar_paquete(paquete);
}

void ejecutar_rr(t_pcb* pcb) {

    t_args_hilo_quantum* args = malloc(sizeof(t_args_hilo_quantum));
    args->quantum_restante = KERNEL_CONFIG->QUANTUM - pcb->quantum;
    args->pid = pcb->pid;
    args->termino_el_proceso = false;
    pthread_mutex_init(&(args->mutex_quantum), NULL);

    send_pcb(pcb, fd_cpu_dispatch);

    pthread_t hilo_iniciar_quantum;
    pthread_create(&hilo_iniciar_quantum, NULL, (void*) iniciar_quantum, (void*)args);
    pthread_detach(hilo_iniciar_quantum);

    op_code cod = recibir_operacion(fd_cpu_dispatch);

    pthread_mutex_lock(&(args->mutex_quantum));
    args->termino_el_proceso = true;
    pthread_mutex_unlock(&(args->mutex_quantum));

    pthread_cancel(hilo_iniciar_quantum);

    pthread_mutex_destroy(&args->mutex_quantum);
    free(args);

    switch(cod) {
        case PCB:
            t_pcb* contexto_ejecucion = deserializar_pcb(fd_cpu_dispatch);
            pthread_mutex_lock(&mutex_recibir_pcb);
            recibiendo_pcb = true;
            pthread_mutex_unlock(&mutex_recibir_pcb);
            manejar_proceso(contexto_ejecucion);
            break;
        case DESCONEXION:
            log_error(logger_kernel, "El CPU-DISPATCH se desconecto.");
            break;
        default:
            log_error(logger_kernel, "Operacion desconocida");
            break;
    }
}

void ejecutar_vrr(t_pcb* pcb) {
    t_args_hilo_quantum* args = malloc(sizeof(t_args_hilo_quantum));
    args->pid = pcb->pid;
    args->termino_el_proceso = false;
    args->quantum_restante = KERNEL_CONFIG->QUANTUM - pcb->quantum; 
    pthread_mutex_init(&(args->mutex_quantum), NULL);

    send_pcb(pcb, fd_cpu_dispatch);
    
    pthread_t hilo_iniciar_quantum;
    pthread_create(&hilo_iniciar_quantum, NULL, (void*) iniciar_quantum, (void*)args);
    pthread_detach(hilo_iniciar_quantum);

    op_code cod = recibir_operacion(fd_cpu_dispatch);

    pthread_mutex_lock(&(args->mutex_quantum));
    args->termino_el_proceso = true;
    pthread_mutex_unlock(&(args->mutex_quantum));

    pthread_cancel(hilo_iniciar_quantum);

    pthread_mutex_destroy(&args->mutex_quantum);
    free(args);

    switch (cod) {
        case PCB:
            t_pcb* contexto_ejecucion = deserializar_pcb(fd_cpu_dispatch);
            pthread_mutex_lock(&mutex_recibir_pcb);
            recibiendo_pcb = true;
            pthread_mutex_unlock(&mutex_recibir_pcb);
            manejar_proceso(contexto_ejecucion);
            break;
        case DESCONEXION:
            log_error(logger_kernel, "El CPU-DISPATCH se desconectó.");
            ya_termino_el_proceso = true;
            break;
        default:
            log_error(logger_kernel, "Operación desconocida");
            ya_termino_el_proceso = true;
            break;
    }
}

//
void liberar_contexto(t_pcb* contexto) {
    dictionary_destroy_and_destroy_elements(contexto->registros, &free);
    list_destroy(contexto->datos_bloqueo->argumentos);
    list_destroy(contexto->datos_bloqueo->dir_fisicas);
    free(contexto->datos_bloqueo->nombre);
    free(contexto->datos_bloqueo);
    free(contexto);
}

void actualizar_pcb(t_pcb* pcb, t_pcb* contexto) {
    actualizar_registros(pcb->registros, contexto->registros);
    pcb->datos_bloqueo->nombre = strdup(contexto->datos_bloqueo->nombre);
    pcb->datos_bloqueo->instruccion = contexto->datos_bloqueo->instruccion;
    list_add_all(pcb->datos_bloqueo->argumentos, contexto->datos_bloqueo->argumentos);
    list_add_all(pcb->datos_bloqueo->dir_fisicas, contexto->datos_bloqueo->dir_fisicas);
    pcb->quantum = contexto->quantum;
    pcb->motivo_salida = contexto->motivo_salida;
    pcb->estado = contexto->estado;
}

bool hay_interrupcion_por_usuario = false;


int queda_quantum(t_pcb* pcb) {
    return pcb->quantum < KERNEL_CONFIG->QUANTUM;
}

void manejar_proceso(t_pcb* contexto_ejecucion) {
    sem_wait(&sem_planificacion_exec);

    t_pcb* pcb = pop_cola_exec();

    actualizar_pcb(pcb, contexto_ejecucion);

    liberar_contexto(contexto_ejecucion);

    if(pcb->motivo_salida!=FINALIZACION_USUARIO && hay_interrupcion_por_usuario) {
        hay_interrupcion_por_usuario = false;
        push_cola_exit(pcb);
    }
    else {
        switch(pcb->motivo_salida) {
            case FIN_EXITOSO:
                log_fin_proceso(pcb->pid, "SUCCESS");
                push_cola_exit(pcb);
                break;
            case BLOQUEO_IO:
                if(dictionary_get(interfaces_io, pcb->datos_bloqueo->nombre) == NULL) {
                    log_fin_proceso(pcb->pid, "INVALID_INTERFACE");
                    push_cola_exit(pcb);
                    break;
                }
                log_motivo_bloqueo(pcb, pcb->datos_bloqueo->nombre);
                pcb->motivo_salida = -1;
                push_cola_bloq_io(pcb->datos_bloqueo->nombre, pcb);
                break;
            case BLOQUEO_RECURSO:
                t_recurso* recurso = dictionary_get(recursos, pcb->datos_bloqueo->nombre);
                if(recurso == NULL) {
                    log_fin_proceso(pcb->pid, "INVALID_RESOURCE");
                    push_cola_exit(pcb);
                    break;
                }
                log_motivo_bloqueo(pcb, pcb->datos_bloqueo->nombre);
                pcb->motivo_salida = -1;
                manejar_recurso(pcb, recurso);
                return;
                break;
            case OUT_OF_MEMORY:
                log_fin_proceso(pcb->pid, "OUT_OF_MEMORY");
                push_cola_exit(pcb);
                break;
            case FINALIZACION_USUARIO:
                log_fin_proceso(pcb->pid, "INTERRUPTED_BY_USER");
                push_cola_exit(pcb);
                break;
            case FIN_QUANTUM:
                log_fin_quantum(pcb);
                pcb->motivo_salida = -1;
                push_cola_ready(pcb);
                break;
            default:
                log_error(logger_kernel, "Motivo desconocido");
                break;
        }
    }
    pthread_mutex_lock(&mutex_recibir_pcb);
    recibiendo_pcb = false;
    pthread_mutex_unlock(&mutex_recibir_pcb);
    sem_post(&sem_planificacion_exec);
}

void ejecutar_segun_algoritmo(t_pcb* pcb) {
    if(es_fifo())
        ejecutar_fifo(pcb);
    else if(es_rr())
        ejecutar_rr(pcb);
    else if(es_vrr())
        ejecutar_vrr(pcb);
}

void manejar_recurso(t_pcb* pcb, t_recurso* recurso) {
    t_instr_cpu instruccion = pcb->datos_bloqueo->instruccion;

    pthread_mutex_lock(&mutex_recibir_pcb);
    recibiendo_pcb = false;
    pthread_mutex_unlock(&mutex_recibir_pcb);
    sem_post(&sem_planificacion_exec);

    switch (instruccion) {
        case WAIT:
            pthread_mutex_lock(&mutex_recursos);
            recurso->instancias_disponibles--;

            t_recurso_proceso* rp = malloc(sizeof(t_recurso_proceso));
            rp->pid = pcb->pid;
            rp->nombre_recurso = strdup(recurso->nombre);
            pthread_mutex_lock(&mutex_recursosXProcesos);
            list_add(lista_recursos_procesos, rp);
            pthread_mutex_unlock(&mutex_recursosXProcesos);

            if(recurso->instancias_disponibles < 0) 
                push_cola_bloq_rec(recurso->cola_bloqueados_recursos, pcb);
            else {
                if(es_fifo())
                    push_cola_exec(pcb);
                else if(queda_quantum(pcb)) // En el caso que sea rr o vrr
                    push_cola_exec(pcb);
                else
                    push_cola_ready(pcb);
            }
            pthread_mutex_unlock(&mutex_recursos);
            break;
        case SIGNAL:
            if(manejar_signal(pcb, recurso)) {
                if(es_fifo())
                    push_cola_exec(pcb);
                else if(queda_quantum(pcb)) // En el caso que sea rr o vrr
                    push_cola_exec(pcb);
                else
                    push_cola_ready(pcb);
            }
            break;

        default:
            break;
    }
}

void esperar_io() {
    while (1) {
        int fd_io = accept(fd_kernel, NULL, NULL);

        op_code codOp = recibir_operacion(fd_io);

        switch(codOp) {
            case NOMBRE_INTERFAZ:
                char* nombre_interfaz = recibir_nombre_interfaz(fd_io);

                if(ya_existe_esa_interfaz(nombre_interfaz)) {
                    log_warning(logger_kernel, "Ya existe una interfaz conectada con ese nombre: %s", nombre_interfaz);
                    liberar_conexion(&fd_io);
                    break;
                }

                log_info(logger_kernel, "Se conecto la interfaz: %s", nombre_interfaz);

                t_args_manejar_io args = {fd_io, nombre_interfaz};

                pthread_t hilo_manejar_io;
                pthread_create(&hilo_manejar_io, NULL, (void*) manejar_io, (void*)&args);
                pthread_detach(hilo_manejar_io);
                break;
            default:
                liberar_conexion(&fd_io);
                break;
        }
    } 
}

bool ya_existe_esa_interfaz(char* nombre) {
    return dictionary_get(interfaces_io, nombre) != NULL;
}

void _push_cola_exit(void* _pcb) {
    push_cola_exit((t_pcb*)_pcb);
}

void liberar_io(void* _datos) {
    t_cola_bloq* datos = (t_cola_bloq*)_datos;
    list_map(datos->cola_bloq, (void*)_push_cola_exit);
    list_destroy(datos->cola_bloq);
    sem_destroy(datos->sem_cola_bloq);
    free(datos);
}

void manejar_io(void* arg) {
    t_args_manejar_io* params = (t_args_manejar_io*) arg;

    int fd_interfaz = params->socket;
    char* nombre_interfaz = params->nombre;

    // Además de una cola, voy a necesitar un semaforo que me indique cuando se le ingresan procesos.
    t_cola_bloq* datos_cola_bloq = malloc(sizeof(t_cola_bloq));

    t_list* cola_bloq = list_create();

    sem_t sem_cola_bloq;

    sem_init(&sem_cola_bloq, 0, 0);

    sem_t* ptr_sem_cola_bloq = &sem_cola_bloq;

    datos_cola_bloq->cola_bloq = cola_bloq;
    datos_cola_bloq->sem_cola_bloq = ptr_sem_cola_bloq;
    
    pthread_mutex_lock(&mutex_dicc_io);
    dictionary_put(interfaces_io, nombre_interfaz, datos_cola_bloq);
    pthread_mutex_unlock(&mutex_dicc_io);
    
    while(esta_activo) {
        // Agregar para arreglarse cuando se desconecta la io
        // Detener planificaciom, agregar otro semaforo
        sem_wait(ptr_sem_cola_bloq);

        if(fd_interfaz == -1) {
            log_error(logger_kernel, "Se desconecto la interfaz: %s", nombre_interfaz);
            break;
        }

        pthread_mutex_lock(&mutex_kernel_io);
        
        t_pcb* pcb = (t_pcb*)list_get(cola_bloq, 0);

        send_instruccion_io(fd_interfaz, pcb->pid, pcb->datos_bloqueo->instruccion, pcb->datos_bloqueo->argumentos, pcb->datos_bloqueo->dir_fisicas);

        op_code code = recibir_operacion(fd_interfaz);

        // Lo dejo así hasta ver como solucionarlo (no se porque me tira cero después de la primera) 
        // if(code == 0) code = recibir_operacion(fd_interfaz);
        
        switch (code) {
            case BLOQUEO_TERMINADO:
                int size;
                free(recibir_buffer(&size, fd_interfaz));
                pcb->datos_bloqueo->nombre = string_new(); // Arreglar esto
                pcb->datos_bloqueo->instruccion = -1;
                list_clean_and_destroy_elements(pcb->datos_bloqueo->argumentos, &free);
                list_clean_and_destroy_elements(pcb->datos_bloqueo->dir_fisicas, &free);
                if(!list_remove_element(cola_bloq, (void*)pcb)) {
                    log_error(logger_kernel, "Error al sacar pcb de la cola de %s", nombre_interfaz);
                    break;
                }
                if(es_vrr() && queda_quantum(pcb))
                    push_cola_ready_aux(pcb);
                else
                    push_cola_ready(pcb);
                break;
            case ERROR:
                log_error(logger_kernel, "La interfaz no pudo realizar la instruccion");
                break;
            case DESCONEXION:
                log_error(logger_kernel, "Se desconecto la interfaz: %s", nombre_interfaz);
                liberar_io(datos_cola_bloq);
                pthread_mutex_unlock(&mutex_dicc_io);
                dictionary_remove_and_destroy(interfaces_io, nombre_interfaz, &liberar_io);
                pthread_mutex_unlock(&mutex_dicc_io);
                break;
            default:
                // liberar_pcb
                printf("Code: %d\n", code);
                log_error(logger_kernel, "Error desconocido");
                break;
        }

        pthread_mutex_unlock(&mutex_kernel_io);
    }
    pthread_mutex_lock(&mutex_dicc_io);
    dictionary_remove_and_destroy(interfaces_io, nombre_interfaz, &liberar_io);
    pthread_mutex_unlock(&mutex_dicc_io);
}

int ignorar_signal_mp = 0;

// Funciones de cola
t_pcb* eliminar_primer_elemento(t_list* cola) {
    if(list_is_empty(cola)) return NULL;
    return (t_pcb*)list_remove(cola, 0);
}

t_pcb* pop_cola_new() {
    pthread_mutex_lock(&mutex_cola_new);
    t_pcb* pcb = eliminar_primer_elemento(COLA_NEW);
    pthread_mutex_unlock(&mutex_cola_new);
    return pcb;
}

t_pcb* pop_cola_bloq(t_list* cola) {
    return eliminar_primer_elemento(cola);
}

t_pcb* pop_cola_ready() {
    pthread_mutex_lock(&mutex_cola_ready);
    t_pcb* pcb = eliminar_primer_elemento(COLA_READY);
    pthread_mutex_unlock(&mutex_cola_ready);
    return pcb;
}

t_pcb* pop_cola_ready_aux() {
    pthread_mutex_lock(&mutex_cola_ready_aux);
    t_pcb* pcb = eliminar_primer_elemento(COLA_READY_AUX);
    pthread_mutex_unlock(&mutex_cola_ready_aux);
    return pcb;
}

t_pcb* pop_cola_exec() {
    pthread_mutex_lock(&mutex_cola_exec);
    t_pcb* pcb = eliminar_primer_elemento(COLA_EXEC);
    pthread_mutex_unlock(&mutex_cola_exec);
    return pcb;
}

t_pcb* pop_cola_exit() {
    pthread_mutex_lock(&mutex_cola_exit);
    t_pcb* pcb = eliminar_primer_elemento(COLA_EXIT);
    pthread_mutex_unlock(&mutex_cola_exit);
    return pcb;
}

void push_cola_ready(t_pcb* pcb) {
    pcb->quantum = 0;
    pthread_mutex_lock(&mutex_cola_ready);
    list_add(COLA_READY, pcb);
    pthread_mutex_unlock(&mutex_cola_ready);
    log_ingreso_ready("Ready", COLA_READY);
    log_cambio_estado(pcb->pid, pcb->estado, LISTO);
    pcb->estado = LISTO;
    sem_post(&procesos_en_ready);
}

void push_cola_ready_aux(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_cola_ready_aux);
    list_add(COLA_READY_AUX, pcb);
    pthread_mutex_unlock(&mutex_cola_ready_aux);
    log_ingreso_ready("Ready Aux", COLA_READY_AUX);
    log_cambio_estado(pcb->pid, pcb->estado, LISTO);
    pcb->estado = LISTO;
    sem_post(&procesos_en_ready);
}

void push_cola_exec(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_cola_exec);
    list_add(COLA_EXEC, pcb);
    pthread_mutex_unlock(&mutex_cola_exec);
    log_cambio_estado(pcb->pid, pcb->estado, EJECUTANDO);
    pcb->estado = EJECUTANDO;
    // pthread_t hilo_exec;
    // pthread_create(&hilo_exec, NULL, (void*)ejecutar_segun_algoritmo, (void*)pcb);
    // pthread_detach(hilo_exec);
    ejecutar_segun_algoritmo(pcb);
}

void push_cola_bloq_io(char* nombre, t_pcb* pcb) {
    pthread_mutex_lock(&mutex_dicc_io);
    t_cola_bloq* datos = (t_cola_bloq*) dictionary_get(interfaces_io, nombre);
    pthread_mutex_unlock(&mutex_dicc_io);
    list_add(datos->cola_bloq, pcb);
    log_cambio_estado(pcb->pid, pcb->estado, BLOQUEADO);
    pcb->estado = BLOQUEADO;
    sem_post(datos->sem_cola_bloq);
}

void push_cola_bloq_rec(t_list* cola, t_pcb* pcb) {
    list_add(cola, pcb);
    log_cambio_estado(pcb->pid, pcb->estado, BLOQUEADO);
    pcb->estado = BLOQUEADO;
}

void push_cola_new(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_cola_new);
    list_add(COLA_NEW, pcb);
    pthread_mutex_unlock(&mutex_cola_new);
    pcb->estado = NUEVO;
    sem_post(&procesos_en_new);
}

void push_cola_exit(t_pcb* pcb) {
    pthread_mutex_lock(&mutex_cola_exit);
    list_add(COLA_EXIT, pcb);
    pthread_mutex_unlock(&mutex_cola_exit);
    log_cambio_estado(pcb->pid, pcb->estado, TERMINADO);
    pcb->estado = TERMINADO;
    pthread_mutex_lock(&mutex_ignorar_signal);
    if(!ignorar_signal_mp)
        sem_post(&sem_multiprogramacion);
    else ignorar_signal_mp--;
    pthread_mutex_unlock(&mutex_ignorar_signal);
    sem_post(&procesos_en_exit);
}

t_pcb* buscar_proceso(int pid) {
    for(int i = 0; i < list_size(lista_pcb); i++) {
        t_pcb* pcb_actual = (t_pcb*)list_get(lista_pcb, i);
        if(pcb_actual->pid == pid) return pcb_actual;
    }
    return NULL;
}

bool eliminar_de_lista_por_pid(t_list* lista, int pid) {
    for(int i = 0; i < list_size(lista); i++) {
        t_pcb* pcb_actual = (t_pcb*)list_get(lista, i);
        if(pcb_actual->pid == pid) {
            list_remove(lista, i);
            return true;
        }
    }
    return false;
}

void remover_proceso_en_cola(t_list* cola, int pid) {
    if(!eliminar_de_lista_por_pid(cola, pid)) {
        log_error(logger_kernel, "Error al remover al pcb de la lista");
        // exit(-1);
    }
}

void buscar_proceso_y_eliminar(int pid) {
    t_pcb* pcb = buscar_proceso(pid);
    t_estado estado = pcb->estado;
    if(pcb == NULL) {
        log_error(logger_kernel, "Error al finalizar proceso, no se encontro el pcb");
        exit(-1);
    }
    if(estado == NUEVO) {
        remover_proceso_en_cola(COLA_NEW, pid);
        push_cola_exit(pcb);
    }
    else if(estado == LISTO) { // Arreglar para vrr
        remover_proceso_en_cola(COLA_READY, pid);
        push_cola_exit(pcb);
    }
    else if(estado == EJECUTANDO) {
        pthread_mutex_lock(&mutex_recibir_pcb);
        hay_interrupcion_por_usuario = true;
        if (!recibiendo_pcb) {
            t_paquete* paquete = crear_paquete(INTERRUPCION_USUARIO);
            agregar_a_paquete(paquete, &pid, sizeof(int));
            enviar_paquete(paquete, fd_cpu_interrupt);
            eliminar_paquete(paquete);
        }
        pthread_mutex_unlock(&mutex_recibir_pcb);
    }
    else if(estado == BLOQUEADO) {
        if(pcb->datos_bloqueo->instruccion == WAIT) {
            t_recurso* recurso = dictionary_get(recursos, pcb->datos_bloqueo->nombre);
            remover_proceso_en_cola(recurso->cola_bloqueados_recursos, pid);
            push_cola_exit(pcb);
        }
        else { // Arreglar para los casos en que ya este en io
            t_cola_bloq* interfaz = dictionary_get(interfaces_io, pcb->datos_bloqueo->nombre);
            remover_proceso_en_cola(interfaz->cola_bloq, pid);
            push_cola_exit(pcb);
        }
    }
    else log_error(logger_kernel, "Error al verificar el estado");
}

void imprimir_proceso_estado(void* _pcb) {
    t_pcb* pcb = (t_pcb*)_pcb;
    printf("PID: %d\n", pcb->pid);
}

void imprimir_bloq_io(char* nombre_interfaz) {
    printf("\nProcesos Bloqueados por %s:\n", nombre_interfaz);
    t_cola_bloq* interfaz = dictionary_get(interfaces_io, nombre_interfaz);
    list_map(interfaz->cola_bloq, (void*)imprimir_proceso_estado);
}

void imprimir_bloq_rec(char* nombre_recurso) {
    printf("\nProcesos Bloqueados por %s:\n", nombre_recurso);
    t_recurso* recurso = dictionary_get(recursos, nombre_recurso);
    list_map(recurso->cola_bloqueados_recursos, (void*)imprimir_proceso_estado);
}

// iniciar_proceso TEST

//Funciones de consola
//Listar procesos por estado
void proceso_estado() {
    detener_planificacion(NULL);

    printf("\nProcesos en New:\n");
    list_map(COLA_NEW, (void*)imprimir_proceso_estado);

    printf("\nProcesos en Ready:\n");
    list_map(COLA_READY, (void*)imprimir_proceso_estado);

    if(KERNEL_CONFIG->ALGORITMO_PLANIFICACION == VRR) {
        printf("\nProcesos en Ready Aux:\n");
        list_map(COLA_READY_AUX, (void*)imprimir_proceso_estado);
    }

    pthread_mutex_lock(&mutex_dicc_io);
    t_list* lista_interfaces = dictionary_keys(interfaces_io);
    list_map(lista_interfaces, (void*)imprimir_bloq_io);
    pthread_mutex_unlock(&mutex_dicc_io);

    t_list* lista_recursos = dictionary_keys(recursos);
    list_map(lista_recursos, (void*)imprimir_bloq_rec);

    printf("\nProcesos en Exec:\n");
    list_map(COLA_EXEC, (void*)imprimir_proceso_estado);

    printf("\nProcesos en Exit:\n");
    list_map(COLA_EXIT, (void*)imprimir_proceso_estado);

    iniciar_planificacion(NULL);
}


//Cambiar grado multiprogramacion
void multiprogramacion(char* inst) {
    detener_planificacion(NULL);

    int nueva_mp = atoi(inst);

    int diferencia = nueva_mp - KERNEL_CONFIG->GRADO_MULTIPROGRAMACION;

    if (diferencia > 0) {
        for (int i = 0; i < diferencia; i++) {
            sem_post(&sem_multiprogramacion);
        }
    }
    else {
        // Caso en el que los procesos en mp sean menores al nuevo grado
        pthread_mutex_lock(&cant_proc_multip);

        if(procesos_en_multiprogramacion < nueva_mp) {
            for(int i = 0; i < abs(diferencia); i++) {
                sem_wait(&sem_multiprogramacion);
            }
        }

        pthread_mutex_lock(&mutex_ignorar_signal);

        if (procesos_en_multiprogramacion > nueva_mp) {
            for(int i = 0; i < (KERNEL_CONFIG->GRADO_MULTIPROGRAMACION - procesos_en_multiprogramacion); i++) {
                sem_wait(&sem_multiprogramacion);
            }
            ignorar_signal_mp = procesos_en_multiprogramacion - nueva_mp;
        }

        pthread_mutex_unlock(&mutex_ignorar_signal);
        
        pthread_mutex_unlock(&cant_proc_multip);
    }

    KERNEL_CONFIG->GRADO_MULTIPROGRAMACION = nueva_mp;

    iniciar_planificacion(NULL);

    return;
}

int manejar_signal(t_pcb* pcb, t_recurso* recurso) {
    pthread_mutex_lock(&mutex_recursosXProcesos);
    bool found = false;
    for(int i = 0; i < list_size(lista_recursos_procesos) && !found; i++) {
        t_recurso_proceso* rp = list_get(lista_recursos_procesos, i);
        if (strcmp(rp->nombre_recurso, recurso->nombre) == 0 && rp->pid == pcb->pid) {
            list_remove(lista_recursos_procesos, i);
            found = true;
            free(rp->nombre_recurso);
            free(rp);
        }
    }
    pthread_mutex_unlock(&mutex_recursosXProcesos);

    if (!found) {
        log_fin_proceso(pcb->pid, "INVALID_SIGNAL");
        push_cola_exit(pcb);
        return 0;
    }

    liberar_recurso(recurso);
    
    return 1;
}