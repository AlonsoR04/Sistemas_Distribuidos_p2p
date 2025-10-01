#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#include "comm.h"
#include "funciones.h"
#include "log.h"

#define MAX_SIZE 1024
#define MAX_THREADS 10
#define MAX_PETICIONES 10

int buffer_peticiones[MAX_PETICIONES];   // Buffer donde almacenamos las peticiones entrantes

int n_elementos = 0;			// Elementos del buffer de peticiones
int pos_servicio = 0;

// Mutex para evitar un acceso simultaneo
pthread_mutex_t mutex;
pthread_cond_t no_lleno;
pthread_cond_t no_vacio;

pthread_mutex_t mfin;
int fin=false;

int send_rpc (struct log_msg peticion_log) {
    char *host = getenv("LOG_RPC_IP"); // Recuperar la IP del servidor

    CLIENT *clnt;
	enum clnt_stat retval_1;
	int result_1 = 0;

	clnt = clnt_create (host, LOG, LOG_VER, "udp"); // Crear el cliente RPC
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}

	retval_1 = send_log_1(peticion_log, &result_1, clnt); // Llamada al funcion RPC
	if (retval_1 != RPC_SUCCESS) {
		clnt_perror (clnt, "call failed");
        result_1 = -1;
	}

	clnt_destroy (clnt); // Destruir el cliente RPC
    return result_1;
}

void tratar_pet(char *peticion, int *respuesta, char **respuesta2) {
    char operacion[15];
    char usuario[256];
    char file_name[256];
    char description[1024];
    char port[10];
    char ip[16];
    char target[256];
    char time[32];
    char date[32];

    struct log_msg peticion_log;
    peticion_log.file_name_log = ""; // Evitar que valga NULL

    // Parsea el string recibido como peticion
    char *saveptr;
    char *token = strtok_r(peticion, ",", &saveptr);

    strcpy(operacion, token);
    peticion_log.op_log = strdup(operacion);

    if (strcmp(operacion, "GET_FILE") != 0) { // Si no es GET_FILE, se espera que haya fecha y hora

        if ((token = strtok_r(NULL, ",", &saveptr))) {
            strcpy(date, token); // Guardamos la fecha
            peticion_log.date_log = strdup(date);
        }

        if ((token = strtok_r(NULL, ",", &saveptr))) {
            strcpy(time, token); // Guardamos la hora
            peticion_log.time_log = strdup(time);
        }
    }

    if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener usuario
        strcpy(usuario, token);
        peticion_log.user_log = strdup(usuario);
    }

    printf("OPERACION: %s FROM %s\n", operacion, usuario);

    if (strcmp(operacion, "REGISTER") == 0){
        *respuesta = register_user(usuario);
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    
    if (strcmp(operacion, "UNREGISTER") == 0){
        *respuesta = unregister_user(usuario);
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    
    if (strcmp(operacion, "CONNECT") == 0){
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener IP
            strcpy(ip, token);
        }
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener puerto
            strcpy(port, token);
        }
        *respuesta = connect_user(usuario, ip, atoi(port));
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    
    if (strcmp(operacion, "DISCONNECT") == 0){
        *respuesta = disconnect_user(usuario);
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    
    if (strcmp(operacion, "PUBLISH") == 0){
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener nombre de archivo
            strcpy(file_name, token);
            peticion_log.file_name_log = strdup(file_name);
        }
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener descripcion
            strcpy(description, token);
        }
        *respuesta = publish_file(usuario, file_name, description);
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    if (strcmp(operacion, "DELETE") == 0){
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener nombre de archivo
            strcpy(file_name, token);
            peticion_log.file_name_log = strdup(file_name);
        }
        *respuesta = delete_file(usuario, file_name);
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    if (strcmp(operacion, "LIST_USERS") == 0){
        *respuesta = list_users(usuario);
        if (*respuesta == 0) { // Si la lista de usuarios es correcta, la formateamos para enviar
            *respuesta2 = formatear_list_users();
        }
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    if (strcmp(operacion, "LIST_CONTENT") == 0){
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener nombre de usuario
            strcpy(target, token);
        }
        *respuesta = list_content(usuario, target);
        if (*respuesta == 0) { // Si la lista de contenido es correcta, la formateamos para enviar
            *respuesta2 = formatear_list_content(target);
        }
        if ((send_rpc(peticion_log)) != 0) {
            *respuesta = -1;
        };
        return;
    }
    if (strcmp(operacion, "GET_FILE") == 0){
        if ((token = strtok_r(NULL, ",", &saveptr))) { // Obtener nombre de usuario
            strcpy(target, token);
        }
        *respuesta = get_file(usuario,target);
        if (*respuesta == 0) { // Enviar Ip y puerto del cliente del target
            *respuesta2 = obtener_datos_cliente(target);
        }
        return;
    }
    
}

void servicio(void) {
    /* Funcion que ofrece a lectura, procesado y envio */
    int sc;
    char peticion[MAX_SIZE];
    int respuesta;
    ssize_t ret;

    for (;;) {
        // Extrae un socket del buffer
        pthread_mutex_lock(&mutex);
        while (n_elementos == 0) {
            if (fin) {
                pthread_mutex_unlock(&mutex);
                pthread_exit(0);
            }
            pthread_cond_wait(&no_vacio, &mutex);
        }
        // Actualiza atributos del buffer
        sc = buffer_peticiones[pos_servicio];
        pos_servicio = (pos_servicio + 1) % MAX_PETICIONES;
        n_elementos--;

        pthread_cond_signal(&no_lleno);
        pthread_mutex_unlock(&mutex);

        // Procesa la petición
        ret = readLine(sc, peticion, sizeof(peticion));
        if (ret < 0) {
            fprintf(stderr, "Error en recepción\n");
            close(sc);
            continue;
        }
        char *respuesta2 = NULL;
        tratar_pet(peticion, &respuesta, &respuesta2);   
        ret = sendMessage(sc, (char *)&respuesta, sizeof(respuesta));
        if (ret < 0) {
            fprintf(stderr, "Error en envio respuesta\n");
            close(sc);
            continue;
        }
        
        if (respuesta2 != NULL) {
            ret = writeLine(sc, respuesta2);
            if (ret < 0) {
                fprintf(stderr, "Error en envio respuesta\n");
                close(sc);
                continue;
            }
        }

        free(respuesta2);
        close(sc); // Cierra el socket
    }
    pthread_exit(0);
}

int main(int argc, char* argv[]) {

    max_connected_users = 25;
    connected_users = calloc(max_connected_users, sizeof(struct user));
    if (connected_users == NULL) {
        fprintf(stderr, "Error: No se pudo alocar memoria para usuarios conectados\n");
        return -1;
    }
    num_connected_users = 0;

    int sd, sc;
    pthread_attr_t t_attr;	// atributos de los threads
	pthread_t thid[MAX_THREADS]; // vector de threads
    int pos = 0; // Posicion del buffer de peticiones

    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Uso: %s -p <puerto>\n", argv[0]);
        return -1;
    }

    int port = atoi(argv[2]);

    if (port < 1024 || port > 65535) {
        fprintf(stderr, "El puerto debe estar entre 1024 y 65535\n");
        return -1;
    }

    // Crea socket
    sd = serverSocket(INADDR_ANY, port, SOCK_STREAM) ;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    if (getsockname(sd, (struct sockaddr *)&server_addr, &addr_len) == -1) {
        perror("Error obteniendo la dirección del socket");
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &server_addr.sin_addr, ip, sizeof(ip)) == NULL) {
        perror("Error convirtiendo la dirección IP");
        return -1;
    }    

    if (sd < 0) {
        fprintf (stderr, "SERVER: Error creando serverSocket\n");
        return -1;
    }

    printf("init server %s: %d\n", ip, port );
    pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&no_lleno,NULL);
	pthread_cond_init(&no_vacio,NULL);
	pthread_mutex_init(&mfin,NULL);

    pthread_attr_init(&t_attr);
    for (int i = 0; i < MAX_THREADS; i++){ // Crea el pool de threads donde cada uno ejecuta servicio
		if (pthread_create(&thid[i], NULL, (void *)servicio, NULL) !=0){
			perror("Error creando el pool de threads\n");
			return -1;
		}
    }

    while (1)
    {
        // Acepta cliente
        sc = serverAccept(sd);
        if (sc < 0) {
            printf("Error en serverAccept\n");
            continue;
        }

        pthread_mutex_lock(&mutex);
		while (n_elementos == MAX_PETICIONES)
				pthread_cond_wait(&no_lleno, &mutex);
        // Actualiza atributos del buffer
		buffer_peticiones[pos] = sc;
		pos = (pos+1) % MAX_PETICIONES;
		n_elementos++;
		pthread_cond_signal(&no_vacio);
		pthread_mutex_unlock(&mutex);
    }

    // Evita acceso multiple a la variable fin
    pthread_mutex_lock(&mfin);
	fin=true;
	pthread_mutex_unlock(&mfin);

	pthread_mutex_lock(&mutex);
	pthread_cond_broadcast(&no_vacio);
	pthread_mutex_unlock(&mutex);

	// Recupera los hilos
	for (int i=0;i<MAX_THREADS;i++)
		pthread_join(thid[i],NULL);

	// Destruye los mutex y las var.cond
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&no_lleno);
	pthread_cond_destroy(&no_vacio);
	pthread_mutex_destroy(&mfin);

    close(sd); // Cierra el socket servidor
    return 0;

}

