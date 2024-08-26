#include "../include/entradasalida.h"

int main(int argc, char *argv[]) {

    logger_io = log_create("entradasalida.log", "I/O", 1, LOG_LEVEL_DEBUG);
    logger_io_obligatorio = log_create("entradasalida_obligatorio.log", "I/O", 1, LOG_LEVEL_DEBUG);
    

    if(argc != 3) {
        log_error(logger_io, "Uso correcto: %s [Nombre] [Path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    nombre_interfaz = argv[1];
    path_config = argv[2];

    config_io = iniciar_config();
    t_config* config_ips = config_create("./configs/ips.config");

    cargar_configs(config_ips);

    if(TIPO_INTERFAZ == DIALFS)
        iniciar_estructuras_fs();

    // t_list* lista = list_create();
    // list_add(lista, "prueba.txt");
    // manejar_fs_create(0, lista);
    // list_destroy(lista);

    if(TIPO_INTERFAZ != GENERICA) {
        // Me conecto a memoria
        fd_memoria = crear_conexion(logger_io, IP_MEMORIA, PUERTO_MEMORIA);
        log_info(logger_io, "Se conecto a MEMORIA con exito!!");
    }
    
    // Me conecto a kernel
    fd_kernel = crear_conexion(logger_io, IP_KERNEL, PUERTO_KERNEL);
    log_info(logger_io, "Se conecto a KERNEL con exito!!");

    send_nombre_interfaz(nombre_interfaz, fd_kernel);

    // Atender mensajes del kernel
    pthread_t hilo_kernel;
    pthread_create(&hilo_kernel, NULL, (void*)atender_kernel, NULL);
    pthread_join(hilo_kernel, NULL);

    terminar_programa();

    config_destroy(config_ips);

    return EXIT_SUCCESS;
}

void liberar_fcb(void* _fcb) {
    t_fcb* fcb = (t_fcb*)_fcb;
    free(fcb->nombre);
    config_destroy(fcb->archivo);
    free(fcb);
}

void buscar_archivos_en_directorio(){
	DIR *directorio = opendir(PATH_BASE_DIALFS);
	struct dirent *dir;

	if(directorio == NULL){
		log_error(logger_io, "No se pudo abrir el directorio");
		exit(-1);
	}

	while((dir = readdir(directorio)) != NULL){
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;
		if (strcmp(dir->d_name, "bloques.dat") != 0 && strcmp(dir->d_name, "bitmap.dat") != 0){
            log_info(logger_io, "Archivo encontrado en directorio: %s", dir->d_name);

            t_fcb* fcb = malloc(sizeof(t_fcb));
            fcb->nombre = strdup(dir->d_name);

            char* path_archivo = get_path(fcb->nombre);
            fcb->archivo = config_create(path_archivo);

            fcb->bloque_inicial = config_get_int_value(fcb->archivo, "BLOQUE_INICIAL");
            fcb->tamanio = config_get_int_value(fcb->archivo, "TAMANIO_ARCHIVO");

            list_add(lista_archivos, fcb);

            free(path_archivo);
        }
	}

	closedir(directorio);
}

void terminar_programa() {
    bitarray_destroy(bitmap_bloques);
    if(TIPO_INTERFAZ != GENERICA) close(fd_memoria);
	close(fd_kernel);
    if(TIPO_INTERFAZ == DIALFS)
        list_destroy_and_destroy_elements(lista_archivos, &liberar_fcb);
    log_destroy(logger_io);
    config_destroy(config_io);
}

t_config* iniciar_config() {
    t_config* config = config_create(path_config);
    if (config == NULL) {
        printf("No se pudo acceder\n");
        exit(-1);
    }
    return config;
}

t_tipo_interfaz tipo_interfaz_a_enum(char* tipo_interfaz) {
    if(string_equals_ignore_case(tipo_interfaz, "GENERICA"))
        return GENERICA;
    if(string_equals_ignore_case(tipo_interfaz, "STDIN"))
        return STDIN;
    if(string_equals_ignore_case(tipo_interfaz, "STDOUT"))
        return STDOUT;
    if(string_equals_ignore_case(tipo_interfaz, "DIALFS"))
        return DIALFS;
    log_error(logger_io, "Tipo de interfaz incorrecto");
    exit(-1);
}

void cargar_configs(t_config* config_ips) {
    char* tipo = config_get_string_value(config_io, "TIPO_INTERFAZ");
    TIPO_INTERFAZ = tipo_interfaz_a_enum(tipo);

    IP_KERNEL = config_get_string_value(config_ips, "IP_KERNEL");
    IP_MEMORIA = config_get_string_value(config_ips, "IP_MEMORIA");
    
    TIEMPO_UNIDAD_TRABAJO = config_get_int_value(config_io, "TIEMPO_UNIDAD_TRABAJO");
    PUERTO_KERNEL = config_get_string_value(config_io, "PUERTO_KERNEL");
    PUERTO_MEMORIA = config_get_string_value(config_io, "PUERTO_MEMORIA");
    if(TIPO_INTERFAZ == DIALFS) {
        PATH_BASE_DIALFS = config_get_string_value(config_io, "PATH_BASE_DIALFS");
        BLOCK_SIZE = config_get_int_value(config_io, "BLOCK_SIZE");
        BLOCK_COUNT = config_get_int_value(config_io, "BLOCK_COUNT");
        RETRASO_COMPACTACION = config_get_int_value(config_io, "RETRASO_COMPACTACION");
    }
}

void inicializar_bloques(){
    char* path_archivo = string_new();
    string_append(&path_archivo, PATH_BASE_DIALFS);
    string_append(&path_archivo, "/bloques.dat");
	int fd_archivo = open(path_archivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	int tamanio = BLOCK_SIZE * BLOCK_COUNT;
    ftruncate(fd_archivo, tamanio);
	buffer_bloques = mmap(NULL, tamanio, PROT_READ | PROT_WRITE, MAP_SHARED, fd_archivo, 0);

	if(fd_archivo == -1){
		log_error(logger_io, "Hubo un problema al crear o abrir el archivo de bloques");
		exit(1);
	}

	close(fd_archivo);
    free(path_archivo);
}

void inicializar_bitmap(){
    char* path_archivo = string_new();
    string_append(&path_archivo, PATH_BASE_DIALFS);
    string_append(&path_archivo, "/bitmap.dat");

	int tamanio = ceil((double)BLOCK_COUNT/(double)8);

    bool existe_archivo = true;

    if(access(path_archivo, F_OK) == -1) {
        existe_archivo = false;
    }

	int fd_archivo = open(path_archivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    
    ftruncate(fd_archivo, tamanio);

    // printf("tamanio: %d\n", tamanio);

	buffer_bitmap = mmap(NULL, tamanio, PROT_READ | PROT_WRITE, MAP_SHARED, fd_archivo, 0);

    if(!existe_archivo) memset(buffer_bitmap, 0, tamanio);

    // mem_hexdump(buffer_bitmap, tamanio);

    bitmap_bloques = bitarray_create_with_mode(buffer_bitmap, tamanio, LSB_FIRST);

	if(fd_archivo == -1){
		log_error(logger_io, "Hubo un problema al crear o abrir el archivo de bitmap");
		exit(1);
	}

	close(fd_archivo);
    free(path_archivo);
}

void iniciar_estructuras_fs() {
    inicializar_bloques();
    inicializar_bitmap();
    lista_archivos = list_create();
    buscar_archivos_en_directorio();
}

void atender_kernel() {
    int esta_activo = 1;
    while (esta_activo) {
		int cod_op = recibir_operacion(fd_kernel);
		switch (cod_op) {
        case INSTRUCCION: // Agregar que reciba el pid
            t_list* lista = recibir_paquete(fd_kernel);
            int pid = *(int*)list_get(lista, 0);
            t_instr_cpu instr = *(t_instr_cpu*)list_get(lista, 1);
            t_list* args = list_create();
            t_list* dirs = list_create();
            int pos = 2;
            deserializar_lista_args_io(lista, &pos, args);
            deserializar_lista_dir(lista, pos, dirs); // Direcciones fisicas

            if(TIPO_INTERFAZ == GENERICA)
                manejar_operacion_generica(pid, instr, args);
            else if(TIPO_INTERFAZ == STDIN)
                manejar_operacion_stdin(pid, instr, dirs);
            else if(TIPO_INTERFAZ == STDOUT)
                manejar_operacion_stdout(pid, instr, dirs);
            else if(TIPO_INTERFAZ == DIALFS)
                manejar_operacion_fs(pid, instr, args, dirs);
            else exit(-1);

            send_cod(fd_kernel, BLOQUEO_TERMINADO);

            list_destroy_and_destroy_elements(lista, &free);
            list_destroy_and_destroy_elements(args, &free);
            list_destroy_and_destroy_elements(dirs, &free);
            // Borrar listas y lo que sea necesario
            break;
		case DESCONEXION:
			log_error(logger_io, "El KERNEL se desconecto.");
            esta_activo = 0;
            break;
		default:
			log_warning(logger_io,"Operacion desconocida. No quieras meter la pata");
			break;
		}
	}
}

void manejar_gen_sleep(int pid, t_list* parametros) {
    int unidades_de_trabajo = atoi(list_get(parametros, 0));

    log_operacion(pid, "IO_GEN_SLEEP");

    dormir(unidades_de_trabajo*TIEMPO_UNIDAD_TRABAJO);
}

void manejar_stdin_read(int pid, t_list* dirs) {

    log_operacion(pid, "IO_STDIN_READ");

    char* dato_ingresado = readline("> ");

    if(list_size(dirs) == 1) {
        t_dir_fisica* dir = list_get(dirs, 0);
        log_info(logger_io, "Dir: %d, Tam: %d", dir->dir, dir->tam);
        void* dato = malloc(dir->tam);
        memcpy(dato, dato_ingresado, dir->tam);
        pedir_escribir_memoria(fd_memoria, dir->dir, dir->tam, dato, pid);
        free(dato);
    }
    else {
        int pos_inicial = 0;
        for(int i = 0; i < list_size(dirs); i++) {
            t_dir_fisica* dir = list_get(dirs, i);
            log_info(logger_io, "Dir: %d, Tam: %d", dir->dir, dir->tam);
            void* dato = malloc(dir->tam);
            memset(dato, 0, dir->tam);
            if((string_length(dato_ingresado) - pos_inicial) > dir->tam)
                memcpy(dato, dato_ingresado + pos_inicial, dir->tam);
            else
                memcpy(dato, dato_ingresado + pos_inicial, (string_length(dato_ingresado) - pos_inicial));

            pedir_escribir_memoria(fd_memoria, dir->dir, dir->tam, dato, pid);
            pos_inicial += dir->tam;
            free(dato);
        }
    }

    free(dato_ingresado);
}

int tamanio_total(t_list* dirs) {
    int tam = 0;
    for(int i = 0; i < list_size(dirs); i++) {
        t_dir_fisica* dir = list_get(dirs, i);
        tam += dir->tam;
    }
    return tam;
}

void manejar_stdout_write(int pid, t_list* dirs) {

    log_operacion(pid, "IO_STDOUT_WRITE");

    if(list_size(dirs) == 1) {
        t_dir_fisica* dir = list_get(dirs, 0);
        log_info(logger_io, "Dir: %d, Tam: %d", dir->dir, dir->tam);
        void* dato = pedir_leer_memoria(fd_memoria, dir->dir, dir->tam, pid);
        char* dato_string = malloc(dir->tam + 1);
        memcpy(dato_string, dato, dir->tam);
        dato_string[dir->tam] = '\0';
        log_info(logger_io, "Elemento recibido: %s\n", dato_string);
        free(dato_string);
        free(dato);
    }
    else {
        int tam_total = tamanio_total(dirs);
        char* dato_leido = malloc(tam_total + 1);
        memset(dato_leido, '\0', tam_total + 1);

        for(int i = 0; i < list_size(dirs); i++) {
            t_dir_fisica* dir = list_get(dirs, i);
            log_info(logger_io, "Dir: %d, Tam: %d", dir->dir, dir->tam);
            void* dato = pedir_leer_memoria(fd_memoria, dir->dir, dir->tam, pid);
            char* dato_string = malloc(dir->tam + 1);
            memcpy(dato_string, dato, dir->tam);
            dato_string[dir->tam] = '\0';
            string_append(&dato_leido, dato_string);
            free(dato_string);
            free(dato);
        }

        log_info(logger_io, "Elemento recibido: %s\n", dato_leido);

        free(dato_leido);
    }
}

void manejar_operacion_generica(int pid, t_instr_cpu instruccion, t_list* parametros) {
    switch (instruccion) {
        case IO_GEN_SLEEP:
            manejar_gen_sleep(pid, parametros);
            break;
        default:
            log_error(logger_io, "La interfaz Generica no entiende esa instruccion");
            send_cod(fd_kernel, ERROR);
            break;
    }
}

void manejar_operacion_stdin(int pid, t_instr_cpu instruccion, t_list* dirs) {
    switch (instruccion) {
        case IO_STDIN_READ:
            manejar_stdin_read(pid, dirs);
            break;
        default:
            log_error(logger_io, "La interfaz STDIN no entiende esa instruccion");
            send_cod(fd_kernel, ERROR);
            break;
    }
}

void manejar_operacion_stdout(int pid, t_instr_cpu instruccion, t_list* dirs) {
    switch (instruccion) {
        case IO_STDOUT_WRITE:
            manejar_stdout_write(pid, dirs);
            break;
        default:
            log_error(logger_io, "La interfaz STDOUT no entiende esa instruccion");
            send_cod(fd_kernel, ERROR);
            break;
    }
}

void manejar_operacion_fs(int pid, t_instr_cpu instruccion, t_list* parametros, t_list* dirs) {
    switch (instruccion) {
        case IO_FS_CREATE:
            manejar_fs_create(pid, parametros);
            break;
        case IO_FS_DELETE:
            manejar_fs_delete(pid, parametros);
            break;
        case IO_FS_TRUNCATE:
            manejar_fs_truncate(pid, parametros);
            break;
        case IO_FS_WRITE: 
            manejar_fs_write(pid, parametros, dirs);
            break;
        case IO_FS_READ:
            manejar_fs_read(pid, parametros, dirs);
            break;
        default:
            log_error(logger_io, "La interfaz DialFS no entiende esa instruccion");
            send_cod(fd_kernel, ERROR);
            break;
    }
    mem_hexdump(buffer_bitmap, (BLOCK_COUNT/8.0));
    dormir(TIEMPO_UNIDAD_TRABAJO);
}


void sincronizar_cambios_bloques() {
    int tamanio = BLOCK_SIZE * BLOCK_COUNT;
    if (msync(buffer_bloques, tamanio, MS_SYNC) == -1) {
        log_error(logger_io, "Error al sincronizar los cambios");
        munmap(buffer_bloques, tamanio);
        exit(-1);
    }
}

void sincronizar_cambios_bitmap() {
    int tamanio = ceil((double)BLOCK_COUNT/(double)8);
    if (msync(buffer_bitmap, tamanio, MS_SYNC) == -1) {
        log_error(logger_io, "Error al sincronizar los cambios");
        munmap(buffer_bitmap, tamanio);
        exit(-1);
    }
}

void manejar_fs_create(int pid, t_list* parametros) {
    log_operacion(pid, "IO_FS_CREATE");

    char* nombre_archivo = list_get(parametros, 0);

    log_fs_create(pid, nombre_archivo);

    int bloque_libre = -1;

    for (int i = 0; i < BLOCK_COUNT; i++) {
        if(!bitarray_test_bit(bitmap_bloques, i)) {
            bloque_libre = i;
            bitarray_set_bit(bitmap_bloques, i);
            sincronizar_cambios_bitmap();
            break;
        }
    }

    if (bloque_libre == -1) {
        log_error(logger_io, "No hay bloques libres disponibles.");
        exit(-1);
    }

    t_fcb* fcb = malloc(sizeof(t_fcb));
    fcb->nombre = strdup(nombre_archivo);

    char* path_archivo = get_path(nombre_archivo);

    FILE* file_fcb = fopen(path_archivo, "a+");
    fclose(file_fcb);

    fcb->archivo = config_create(path_archivo);


    actualizar_bloque_inicial_archivo(fcb, bloque_libre);
    actualizar_tamanio_archivo(fcb, 0);

    list_add(lista_archivos, fcb);

    free(path_archivo);
}

t_fcb* buscar_fcb(char* nombre_archivo) {
    for(int i = 0; i < list_size(lista_archivos); i++) {
        t_fcb* fcb = (t_fcb*)list_get(lista_archivos, i);
        if(strcmp(fcb->nombre, nombre_archivo) == 0) return fcb;
    }
    return NULL;
}

void manejar_fs_read(int pid, t_list* parametros, t_list* dirs) {
    log_operacion(pid, "IO_FS_READ");

    char* nombre_archivo = (char*)list_get(parametros, 0);
    int tamanio = atoi(list_get(parametros, 1));
    int puntero_archivo =  atoi(list_get(parametros, 2));

    log_fs_read(pid, nombre_archivo, tamanio, puntero_archivo);

    t_fcb* fcb = buscar_fcb(nombre_archivo);

    int pos_a_leer = BLOCK_SIZE*fcb->bloque_inicial + puntero_archivo;

    if(puntero_archivo + tamanio > fcb->tamanio) {
        log_error(logger_io, "Los bytes a leer sobrepasan el limite del archivo");
        exit(-1);
    }


    if(list_size(dirs) == 1) {
        void* dato_leido = malloc(tamanio);
        t_dir_fisica* dir_fisica = list_get(dirs, 0);
        memcpy(dato_leido, buffer_bloques + pos_a_leer, tamanio);
        pedir_escribir_memoria(fd_memoria, dir_fisica->dir, tamanio, dato_leido, pid);
        free(dato_leido);
    }
    else {
        for(int i = 0; i < list_size(dirs); i++) {
            t_dir_fisica* dir = list_get(dirs, i);
            void* dato_leido = malloc(dir->tam);
            memcpy(dato_leido, buffer_bloques + pos_a_leer, dir->tam);
            pedir_escribir_memoria(fd_memoria, dir->dir, dir->tam, dato_leido, pid);
            pos_a_leer += dir->tam;
            free(dato_leido);
        }
    }
}

void manejar_fs_write(int pid, t_list* parametros, t_list* dirs) {
    log_operacion(pid, "IO_FS_WRITE");

    char* nombre_archivo = (char*)list_get(parametros, 0);
    int tamanio = atoi(list_get(parametros, 1));
    int puntero_archivo =  atoi(list_get(parametros, 2));

    log_fs_write(pid, nombre_archivo, tamanio, puntero_archivo);

    t_fcb* fcb = buscar_fcb(nombre_archivo);

    int pos_a_escribir = BLOCK_SIZE*fcb->bloque_inicial + puntero_archivo;

    if(puntero_archivo + tamanio > fcb->tamanio) {
        log_error(logger_io, "Los bytes a escribir sobrepasan el limite del archivo");
        exit(-1);
    }

    if(list_size(dirs) == 1) {
        t_dir_fisica* dir = list_get(dirs, 0);

        void* dato_a_escribir = pedir_leer_memoria(fd_memoria, dir->dir, dir->tam, pid);

        memcpy(buffer_bloques + pos_a_escribir, dato_a_escribir, dir->tam);

        mem_hexdump(dato_a_escribir, dir->tam); // Mostrar contenido por pantalla
 
        free(dato_a_escribir);
    }
    else {
        for(int i = 0; i < list_size(dirs); i++) {
            t_dir_fisica* dir = list_get(dirs, i);
            void* dato = pedir_leer_memoria(fd_memoria, dir->dir, dir->tam, pid);
            memcpy(buffer_bloques + pos_a_escribir, dato, dir->tam);
            mem_hexdump(dato, dir->tam);
            pos_a_escribir += dir->tam;
            free(dato);
        }
    }

    sincronizar_cambios_bloques();
}

void manejar_fs_delete(int pid, t_list* parametros) {
    log_operacion(pid, "IO_FS_DELETE");

    char* nombre_archivo = (char*)list_get(parametros, 0);
    
    log_fs_delete(pid, nombre_archivo);

    t_fcb* fcb = buscar_fcb(nombre_archivo);

    if(fcb == NULL) log_error(logger_io, "Error al buscar el fcb");

    int bloque_inicial = config_get_int_value(fcb->archivo, "BLOQUE_INICIAL");
    int tamanio_archivo = config_get_int_value(fcb->archivo, "TAMANIO_ARCHIVO");

    int cant_bloques = ceil((double)tamanio_archivo/(double)BLOCK_SIZE);

    for(int i = 0; i < cant_bloques; i++) {
        bitarray_clean_bit(bitmap_bloques, bloque_inicial + i);
    }

    sincronizar_cambios_bitmap();
    char* path_archivo = get_path(nombre_archivo);
    remove(path_archivo);

    if(!list_remove_element(lista_archivos, fcb))
        log_error(logger_io, "Error al eliminar el archivo de la lista");

    liberar_fcb(fcb);

    free(path_archivo);
}

bool hay_suficientes_bloques_libres_delante(int pos_fin_archivo, int bloques_necesarios) {
    for(int i = 1; i <= bloques_necesarios; i++) {
        if(bitarray_test_bit(bitmap_bloques, pos_fin_archivo + i))
            return false;
    }
    return true;
}

int cant_bloques_libres_detras(int pos_inicial) {
    int i = 0;
    while((pos_inicial - 1 - i) >= 0) {
        if(bitarray_test_bit(bitmap_bloques, pos_inicial - 1 - i))
            break;
        i++;
    }
    return i;
}

int cant_bloques_archivo(int tamanio) {
    return tamanio == 0 ? 1 : (int)ceil((double)tamanio/(double)BLOCK_SIZE);;
}

bool mas_cercano(void* _fcb_1, void* _fcb_2) {
    t_fcb* fcb_1 = (t_fcb*)_fcb_1;
    t_fcb* fcb_2 = (t_fcb*)_fcb_2;
    return fcb_2->bloque_inicial > fcb_1->bloque_inicial;
}

t_list* buscar_archivos_detras(int pos_base) {
    t_list* lista = list_create();
    for(int i = 0; i < list_size(lista_archivos); i++) {
        t_fcb* fcb = list_get(lista_archivos, i);
        if(fcb->bloque_inicial < pos_base) {
            log_error(logger_io, "Archivo: %s, Bloque: %d", fcb->nombre, fcb->bloque_inicial);
            list_add_sorted(lista, fcb, &mas_cercano);
        }
    }
    return lista;
}

void actualizar_tamanio_archivo(t_fcb* fcb, int nuevo_tamanio) {
    fcb->tamanio = nuevo_tamanio;
    char* tamanio_archivo_s =  string_itoa(fcb->tamanio);
    config_set_value(fcb->archivo, "TAMANIO_ARCHIVO", tamanio_archivo_s);
    config_save(fcb->archivo);
    free(tamanio_archivo_s);
    log_info(logger_io, "Nuevo tamanio: %d (%s)", fcb->tamanio, fcb->nombre);
}

void actualizar_bloque_inicial_archivo(t_fcb* fcb, int bloque_inicial) {
    fcb->bloque_inicial = bloque_inicial;
    char* bloque_inicial_s =  string_itoa(bloque_inicial);
    config_set_value(fcb->archivo, "BLOQUE_INICIAL", bloque_inicial_s);
    config_save(fcb->archivo);
    free(bloque_inicial_s);
    log_info(logger_io, "Nuevo bloque inicial: %d (%s)", fcb->bloque_inicial, fcb->nombre);
}

void agregar_bloques(t_fcb* fcb, int bloques_a_agregar) {
    int pos_fin_archivo = fcb->bloque_inicial + cant_bloques_archivo(fcb->tamanio) - 1;
    int bloques_a_agregar_restantes = bloques_a_agregar;
    for(int i = 1; bloques_a_agregar_restantes > 0; i++) {
        bitarray_set_bit(bitmap_bloques, pos_fin_archivo + i);
        bloques_a_agregar_restantes--;
    }
    log_info(logger_io, "Se agregaron %d bloques al archivo %s", bloques_a_agregar, fcb->nombre);
}

void mover_archivo_detras(t_fcb* fcb, int espacios) {
    int pos_final = fcb->bloque_inicial + cant_bloques_archivo(fcb->tamanio);
    memcpy(buffer_bloques+(fcb->bloque_inicial-espacios)*BLOCK_SIZE, buffer_bloques+fcb->bloque_inicial*BLOCK_SIZE, fcb->tamanio);
    int bloque_inicial_aux = fcb->bloque_inicial;
    bloque_inicial_aux-=espacios;
    for(int i = 1; espacios > 0; i++) {
        bitarray_set_bit(bitmap_bloques, fcb->bloque_inicial - i);
        bitarray_clean_bit(bitmap_bloques, pos_final - i);
        espacios--;
    }
    actualizar_bloque_inicial_archivo(fcb, bloque_inicial_aux);
}

void mover_archivo_delante(t_fcb* fcb, int espacios) {
    int pos_final = fcb->bloque_inicial + cant_bloques_archivo(fcb->tamanio);
    memcpy(buffer_bloques+(fcb->bloque_inicial+espacios)*BLOCK_SIZE, buffer_bloques+fcb->bloque_inicial*BLOCK_SIZE, fcb->tamanio);
    int bloque_inicial_aux = fcb->bloque_inicial;
    bloque_inicial_aux+=espacios;
    // Asumo que hay suficiente espacio para poder moverlos
    for(int i = 0; espacios > 0; i++) {
        bitarray_clean_bit(bitmap_bloques, fcb->bloque_inicial + i);
        bitarray_set_bit(bitmap_bloques, pos_final + i);
        espacios--;
    }
    actualizar_bloque_inicial_archivo(fcb, bloque_inicial_aux);
}

bool mas_lejano(void* _fcb_1, void* _fcb_2) {
    t_fcb* fcb_1 = (t_fcb*)_fcb_1;
    t_fcb* fcb_2 = (t_fcb*)_fcb_2;
    return fcb_1->bloque_inicial > fcb_2->bloque_inicial;
}

t_list* archivos_delante_ordenado_lejano_a_cercano(int pos_base) {
    t_list* lista = list_create();
    for(int i = 0; i < list_size(lista_archivos); i++) {
        t_fcb* fcb = list_get(lista_archivos, i);
        if(fcb->bloque_inicial > pos_base)
            list_add_sorted(lista, fcb, &mas_lejano);
    }
    return lista;
}

void compactar(int pid, t_fcb* fcb, int bloques_a_agregar, int nuevo_tamanio) {
    log_fs_inicio_compactacion(pid);

    // Primero fijarse si se pueden acomodar los archivos de atras
    t_list* archivos_detras = buscar_archivos_detras(fcb->bloque_inicial);

    int cant_archivos_detras = list_size(archivos_detras);

    if(cant_archivos_detras > 0) {
        for(int i = 0; i < cant_archivos_detras; i++) {
            t_fcb* fcb_detras = list_get(archivos_detras, i);
            int espacios_libres_detras = cant_bloques_libres_detras(fcb_detras->bloque_inicial);
            if(espacios_libres_detras > 0)
                mover_archivo_detras(fcb_detras, espacios_libres_detras);
        }
    }

    int espacios_libres_detras = cant_bloques_libres_detras(fcb->bloque_inicial);
    if(espacios_libres_detras > 0)
        mover_archivo_detras(fcb, espacios_libres_detras);

    int cant_bloques_libres_delante = 0;

    int pos_fin_archivo = fcb->bloque_inicial + cant_bloques_archivo(fcb->tamanio) - 1;

    for(int i = 1; bloques_a_agregar > cant_bloques_libres_delante; i++) {
        if(bitarray_test_bit(bitmap_bloques, pos_fin_archivo + i)) {
            break;
        }
        cant_bloques_libres_delante++;
    }

    int cant_bloques_faltantes = bloques_a_agregar - cant_bloques_libres_delante;

    if(cant_bloques_faltantes > 0) {
        // Ahora voy a mover todos los archivos que estan adelante la cantidad
        // de bloques que le faltan al archivo para poder agrandarse
        t_list* archivos_delante = archivos_delante_ordenado_lejano_a_cercano(fcb->bloque_inicial);

        for(int i = 0; i < list_size(archivos_delante); i++) {
            t_fcb* fcb_actual = list_get(archivos_delante, i);
            mover_archivo_delante(fcb_actual, cant_bloques_faltantes);
        }
    
        list_destroy(archivos_delante);
    }

    log_fs_fin_compactacion(pid);

    dormir(RETRASO_COMPACTACION);

    agregar_bloques(fcb, bloques_a_agregar);
    // finalizar_proceso 3
    actualizar_tamanio_archivo(fcb, nuevo_tamanio);

    list_destroy(archivos_detras);

    sincronizar_cambios_bloques();
    sincronizar_cambios_bitmap();
}

void manejar_fs_truncate(int pid, t_list* parametros) {
    log_operacion(pid, "IO_FS_TRUNCATE");

    char* nombre_archivo = list_get(parametros, 0);
    int nuevo_tamanio = atoi(list_get(parametros, 1));

    log_fs_truncate(pid, nombre_archivo, nuevo_tamanio);

    t_fcb* fcb = buscar_fcb(nombre_archivo);

    if(fcb == NULL) log_error(logger_io, "Error al buscar el fcb");

    // Si el tamanio es cero la cant es 1
    int cant_bloques = cant_bloques_archivo(fcb->tamanio);
    int cant_bloques_nuevo = cant_bloques_archivo(nuevo_tamanio);
    
    int pos_fin_archivo = fcb->bloque_inicial + cant_bloques - 1;
    
    // Casos en los que no haga falta cambiar los bloques
    if(fcb->tamanio == nuevo_tamanio) {
        log_info(logger_io, "El archivo queda igual");
        return;
    }
    if((cant_bloques - cant_bloques_nuevo) == 0) {
        log_info(logger_io, "Cambia el tamanio del archivo, pero no los bloques");
        actualizar_tamanio_archivo(fcb, nuevo_tamanio);
        return;
    }

    // Desasignar bloques
    if(nuevo_tamanio < fcb->tamanio) {
        int bloques_a_eliminar = cant_bloques - cant_bloques_nuevo;
        for(int i = 0; i < bloques_a_eliminar; i++) {
            bitarray_clean_bit(bitmap_bloques, pos_fin_archivo - i);
        }
        sincronizar_cambios_bitmap();
        actualizar_tamanio_archivo(fcb, nuevo_tamanio);
    }
    // Asignar bloques
    else {
        int bloques_a_agregar = cant_bloques_nuevo - cant_bloques;
        int cant_bloques_libres_delante = 0;
        int cant_bloques_libres_total;

        // Aca se fija si tiene suficientes bloques libres adelante y atras
        for(int i = 1; bloques_a_agregar > cant_bloques_libres_delante; i++) {
            if(bitarray_test_bit(bitmap_bloques, pos_fin_archivo + i)) {
                break;
            }
            cant_bloques_libres_delante++;
        }
        cant_bloques_libres_total=cant_bloques_libres_delante;
        for(int i = 1; bloques_a_agregar > cant_bloques_libres_total; i++) {
            if(fcb->bloque_inicial - i < 0 || bitarray_test_bit(bitmap_bloques, fcb->bloque_inicial - i))
                break;
            cant_bloques_libres_total++;
        }

        // Hay suficientes bloques libres contiguos
        if(cant_bloques_libres_total == bloques_a_agregar) {
            // Caso en que no alcancen solo los de adelante primero muevo el archivo
            // las posiciones necesarias para atras
            int bloques_detras = cant_bloques_libres_total - cant_bloques_libres_delante;
            if(bloques_detras > 0) {
                log_info(logger_io, "Hay suficientes bloques libres, pero hay que usar de atras tmb");
                mover_archivo_detras(fcb, bloques_detras);
                agregar_bloques(fcb, bloques_a_agregar);
            }
            // Hay suficientes bloques libres delante
            else {
                log_info(logger_io, "Hay suficientes bloques libres delante");
                agregar_bloques(fcb, bloques_a_agregar);
            }
            actualizar_tamanio_archivo(fcb, nuevo_tamanio);
        }
        // Caso en el que no hay bloques libres de forma contigua
        else compactar(pid, fcb, bloques_a_agregar, nuevo_tamanio);
    }
}

void log_operacion(int pid, char* operacion) {
    log_info(logger_io_obligatorio, "PID: %d - Operacion: %s", pid, operacion);
}


void log_fs_create(int pid, char *nombre_archivo) {
    log_info(logger_io_obligatorio, "PID: %d - Crear Archivo: %s", pid, nombre_archivo);
}

void log_fs_delete(int pid, char *nombre_archivo) {
    log_info(logger_io_obligatorio, "PID: %d - Eliminar Archivo: %s", pid, nombre_archivo);
}

void log_fs_truncate(int pid, char *nombre_archivo, int tamanio) {
    log_info(logger_io_obligatorio, "PID: %d - Truncar Archivo: %s - Tamaño: %d", pid, nombre_archivo, tamanio);
}

void log_fs_write(int pid, char *nombre_archivo, int tamanio, int ptr) {
    log_info(logger_io_obligatorio, "PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombre_archivo, tamanio, ptr);
}

void log_fs_read(int pid, char *nombre_archivo, int tamanio, int puntero_archivo) {
    log_info(logger_io_obligatorio, "PID: %d - Leer Archivo: %s - Tamaño a Leer: %d - Puntero Archivo: %d", pid, nombre_archivo, tamanio, puntero_archivo);
}

void log_fs_inicio_compactacion(int pid) {
    log_info(logger_io_obligatorio, "PID: %d - Inicio Compactación.", pid);
}

void log_fs_fin_compactacion(int pid) {
    log_info(logger_io_obligatorio, "PID: %d - Fin Compactación.", pid);
}

char* get_path(char* nombre_archivo) {
    char* path = string_new();
    string_append(&path, PATH_BASE_DIALFS);
    string_append_with_format(&path, "/%s", nombre_archivo);
    return path;
}