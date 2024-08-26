#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <readline/readline.h>
#include "../../utils/src/utils/socket.h"
#include "logs_kernel.h"
#include "utils_kernel.h"
#include "global.h"


typedef enum {
    INICIAR_PROCESO,
    FINALIZAR_PROCESO,
    EJECUTAR_SCRIPT,
    INICIAR_PLANIFICACION,
    DETENER_PLANIFICACION,
    MULTIPROGRAMACION,
    PROCESO_ESTADO,
    OTRO
} t_instr;

void iniciar_consola();
void realizar_instruccion(void*);
t_instr verificar_tipo_instruccion(char*);
bool esSalida(char*);
void ejecutar_script(char* params);

#endif