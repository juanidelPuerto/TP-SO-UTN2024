#ifndef INICIALIZAR_H_
#define INICIALIZAR_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include "../../utils/src/utils/socket.h"
#include "config_kernel.h"
#include "logs_kernel.h"
#include "global.h"


void cargar_configuracion(t_config*, t_config*);
void generar_conexion();
void inicializar_colas();
void destruir_semaforos();
void inicializar_semaforos();
void inicializar_recursos();

#endif