#include "../include/consola.h"

void iniciar_consola() {
    char* leido;
	leido = readline("> ");
    while (1) {
        string_trim(&leido);
        if(esSalida(leido)) return;
        realizar_instruccion(leido);
		leido = readline("> ");
    }
	free(leido);
}

void realizar_instruccion(void* instruccion) {
    char** split_instr = string_n_split((char*)instruccion, 2, " ");
    t_instr cod = verificar_tipo_instruccion(split_instr[0]);
    switch (cod) {
        case EJECUTAR_SCRIPT:
            ejecutar_script(split_instr[1]);
            break;
        case INICIAR_PROCESO:
            iniciar_proceso(strdup(split_instr[1]));
            break;
        case FINALIZAR_PROCESO:
            finalizar_proceso(split_instr[1]);
            break;
        case INICIAR_PLANIFICACION:
            iniciar_planificacion(split_instr[1]);
            break;
        case DETENER_PLANIFICACION:
            detener_planificacion(split_instr[1]);
            break;
        case PROCESO_ESTADO:
            proceso_estado();
            break; 
        case  MULTIPROGRAMACION:
            multiprogramacion(split_instr[1]);
            break;
        default:
            printf("No existe ese comando\n");
            break;
    }
    for(int i = 0; split_instr[i] != NULL; i++) {
        free(split_instr[i]);
    }
    free(split_instr);
    free(instruccion);
    // string_array_destroy(split_instr);
}

t_instr verificar_tipo_instruccion(char* inst) {
    if (string_equals_ignore_case(inst, "INICIAR_PROCESO")) return INICIAR_PROCESO;
    if (string_equals_ignore_case(inst, "FINALIZAR_PROCESO")) return FINALIZAR_PROCESO;
    if (string_equals_ignore_case(inst, "EJECUTAR_SCRIPT")) return EJECUTAR_SCRIPT;
    if (string_equals_ignore_case(inst, "INICIAR_PLANIFICACION")) return INICIAR_PLANIFICACION;
    if (string_equals_ignore_case(inst, "DETENER_PLANIFICACION")) return DETENER_PLANIFICACION;
    if (string_equals_ignore_case(inst, "MULTIPROGRAMACION")) return MULTIPROGRAMACION;
    if (string_equals_ignore_case(inst, "PROCESO_ESTADO")) return PROCESO_ESTADO;
    else return OTRO;
}

bool esSalida(char* leido) {
    return string_equals_ignore_case(leido, "EXIT");
}

void ejecutar_script(char* params) {
    if(!es_un_solo_param(params)) {
        log_error(logger_kernel, "Instrucción no válida. Formato correcto: INICIAR_PROCESO [PATH]");
        return;
    }

    t_list* instrucciones = obtener_instrucciones(params);

    list_destroy(list_map(instrucciones, (void*)&realizar_instruccion));

    list_destroy(instrucciones);
}