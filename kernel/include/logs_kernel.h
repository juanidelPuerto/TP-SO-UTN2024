#ifndef LOGS_KERNEL_H_
#define LOGS_KERNEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <commons/log.h>
#include "../../utils/src/utils/utils.h"
#include "global.h"

extern t_log* logger_kernel;
extern t_log* logger_kernel_obligatorio;

void log_creacion_proceso(t_pcb* pcb);
void log_fin_proceso(int pid, char* motivo);
void log_cambio_estado(int pid, t_estado estado_anterior, t_estado estado_actual);
char* estado_to_string(t_estado estado);
void log_motivo_bloqueo(t_pcb* pcb, char* motivo); 
void log_fin_quantum(t_pcb* pcb);
void log_ingreso_ready();

char* obtener_lista_pids_ready();

#endif