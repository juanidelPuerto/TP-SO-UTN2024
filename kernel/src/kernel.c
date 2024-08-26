#include "../include/kernel.h"

int main(int argc, char *argv[]) {

    logger_kernel = log_create("kernel.log", "KERNEL", 1, LOG_LEVEL_DEBUG);
    logger_kernel_obligatorio = log_create("kernel_obligatorio.log", "KERNEL", 1, LOG_LEVEL_DEBUG);

    if(argc != 2) {
        log_error(logger_kernel, "Uso correcto: %s [Path_Config]\n", argv[0]);
        return EXIT_FAILURE;
    }


    t_config* config_ips = config_create("./configs/ips.config");
    config_kernel = config_create(argv[1]);
    KERNEL_CONFIG = malloc(sizeof(t_config_kernel));

    esta_activo = 1;
    
    cargar_configuracion(config_kernel, config_ips);

    inicializar_recursos();
    
    inicializar_colas();

    inicializar_semaforos();

    generar_conexion();

    interfaces_io = dictionary_create();

    pthread_t hilo_esperar_io;
    pthread_create(&hilo_esperar_io, NULL, (void*) esperar_io, NULL);
    pthread_detach(hilo_esperar_io);

    iniciar_consola();

    esta_activo = 0;
    sleep(1);

    config_destroy(config_ips);

    terminar_programa();

    return EXIT_SUCCESS;
}

void terminar_programa() {
    destruir_semaforos();

    log_destroy(logger_kernel_obligatorio);
    log_destroy(logger_kernel);

    list_destroy_and_destroy_elements(COLA_EXEC, &liberar_pcb);
    list_destroy_and_destroy_elements(COLA_READY, &liberar_pcb);
    list_destroy_and_destroy_elements(COLA_NEW, &liberar_pcb);
    list_destroy_and_destroy_elements(COLA_EXIT, &liberar_pcb);
    list_destroy_and_destroy_elements(COLA_READY_AUX, &liberar_pcb);
    list_destroy(lista_pcb);

    dictionary_destroy(interfaces_io);

    string_array_destroy(KERNEL_CONFIG->RECURSOS);
    string_array_destroy(KERNEL_CONFIG->INSTANCIAS_RECURSOS);
    
    free(KERNEL_CONFIG);

    close(fd_kernel);

    close(fd_memoria);
    close(fd_cpu_dispatch);
    close(fd_cpu_interrupt);
}