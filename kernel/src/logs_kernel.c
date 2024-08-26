#include "../include/logs_kernel.h"


void log_creacion_proceso(t_pcb* pcb) {
    log_info(logger_kernel_obligatorio, "Se crea el proceso %d en NEW", pcb->pid);
}

void log_fin_proceso(int pid, char* motivo) {
    log_info(logger_kernel_obligatorio, "Finaliza el proceso %d - Motivo: %s", pid, motivo);
}

void log_cambio_estado(int pid, t_estado estado_anterior, t_estado estado_actual) {
    char* estado_anterior_str = estado_to_string(estado_anterior);
    char* estado_nuevo_str = estado_to_string(estado_actual);
    log_info(logger_kernel_obligatorio, "PID: %d - Estado Anterior: %s - Estado Actual: %s", pid, estado_anterior_str, estado_nuevo_str);
}

char* estado_to_string(t_estado estado) {
    switch (estado) {
        case NUEVO: return "NUEVO";
        case LISTO: return "LISTO";
        case EJECUTANDO: return "EJECUTANDO";
        case BLOQUEADO: return "BLOQUEADO";
        case TERMINADO: return "TERMINADO";
        default: return "DESCONOCIDO";
    }
}

void log_motivo_bloqueo(t_pcb* pcb, char* motivo) {
    log_info(logger_kernel_obligatorio, "PID: %d - Bloqueado por: %s", pcb->pid, motivo);
}

void log_fin_quantum(t_pcb* pcb) {
    log_info(logger_kernel_obligatorio, "PID: %d - Desalojado por fin de Quantum", pcb->pid);
}

void log_ingreso_ready(char* tipo_cola, t_list* cola) {
    char* lista_pids = string_new();
    string_append(&lista_pids, "[ ");
    for(int i = 0; i < list_size(cola); i++) {
        t_pcb* pcb = list_get(cola, i);
        if(i == (list_size(cola) - 1))
            string_append_with_format(&lista_pids, "%d ]", pcb->pid);
        else
            string_append_with_format(&lista_pids, "%d, ", pcb->pid);
    }
    log_info(logger_kernel_obligatorio, "Cola %s: %s", tipo_cola, lista_pids);
    free(lista_pids);
}

// char* obtener_lista_pids_ready() {
//     pthread_mutex_lock(&mutex_cola_ready);

//     t_list* lista = list_create();
//     int size = queue_size(COLA_READY);
//     for (int i = 0; i < size; i++) {
//         t_pcb* pcb = queue_pop(COLA_READY);
//         list_add(lista, pcb);
//         queue_push(COLA_READY, pcb);
//     }

//     char* lista_pids = string_new();
//     for (int i = 0; i < list_size(lista); i++) {
//         t_pcb* pcb = list_get(lista, i);
//         if (i > 0) {
//             string_append(&lista_pids, ", ");
//         }
//         char pid_str[10];
//         sprintf(pid_str, "%d", pcb->pid);
//         string_append(&lista_pids, pid_str);
//     }

//     list_destroy(lista);
//     pthread_mutex_unlock(&mutex_cola_ready);
//     return lista_pids;
// }