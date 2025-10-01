#include "funciones.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

struct user *connected_users;
int num_connected_users;
int max_connected_users;

DIR *open_folder(void) { // Abre la carpeta de almacenamiento
    DIR *folder = opendir(DIR_NAME);

    if (folder == NULL) {
        if (mkdir(DIR_NAME, 0777) != 0) { // Si no existe, la crea
            fprintf(stderr, "Error creando la carpeta %s\n", DIR_NAME);
            return NULL;
        }

        folder = opendir(DIR_NAME); // Abre la carpeta
    }

    return folder;
}

char *get_file_name(char *username, int folder){
    const char *ext = ".txt";

    // Construir "username.txt"
    size_t total_len = strlen(username) + strlen(ext) + 1;
    char *filename = (char *)malloc(total_len);

    strcpy(filename, username);
    strcat(filename, ext);
    if (!folder) return filename;

    // Construir "DIR_NAME/username.txt"
    char *filepath = (char *)malloc(strlen(DIR_NAME) + strlen(filename) + 2);
    strcpy(filepath, DIR_NAME);
    strcat(filepath, "/");
    strcat(filepath, filename);
    free(filename);
    return filepath;
}

int exist(char *username){
    DIR *carpeta = open_folder(); // Abre la carpeta keys
    if (carpeta == NULL) {
        fprintf(stderr,"Error: al abrir la carpeta");
        return -1;
    }

    char *filename = get_file_name(username, 0); // Genera el nombre del archivo a buscar

    struct dirent *archivo;
    while ((archivo = readdir(carpeta)) != NULL) { // Recorre la carpeta y compara las claves con la especificada en el argumento
        if(strcmp(archivo->d_name, filename) == 0) {
            free(filename);
            closedir(carpeta);
            return 1;
        }
    }
    free(filename); // Libera la memoria
    closedir(carpeta); // Cierra la carpeta
    return 0;
}

int register_user(char* username) {

    if (exist(username) == 1) {
        return 1; // El usuario ya existe
    }

    char *filepath = get_file_name(username, 1);
    FILE *archivo = fopen(filepath, "w"); // Abrimos el archivo

    if (archivo == NULL) {
        fprintf(stderr, "Error al crear el archivo");
        return 2;
    }
    fclose(archivo); // Cerramos el archivo
    free(filepath);
    return 0;
}
    
int unregister_user(char* username) {
    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }
    char *filepath = get_file_name(username, 1);
    if (remove(filepath) == 0) { 
        free(filepath);
        return 0; // En caso de borrar, exito -> 0
    } 
    free(filepath);
    fprintf(stderr, "Error borrando el user %s \n", username);
    return 2;
}

int is_connected(char* username) {
    for (int i = 0; i < num_connected_users; i++) {
        if (strcmp(connected_users[i].username, username) == 0) {
            return 1; // El usuario está conectado
        }
    }
    return 0; // El usuario no está conectado
}

int connect_user(char* username, char* ip, int port) {
    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }
    if (is_connected(username)) {
        return 2; // Usuario ya conectado
    }
    if (num_connected_users == max_connected_users) { // Aumentar el tamaño del array de usuarios conectados
        struct user* new_users = realloc(connected_users, sizeof(struct user) * (max_connected_users + 25));
        if (new_users == NULL) {
            fprintf(stderr, "Error reallocando memoria para usuarios conectados\n");
            return 3;
        }
        connected_users = new_users;
        max_connected_users += 25;
    }
    
    // Agregar el nuevo usuario a la lista de usuarios conectados junto con su IP y puerto
    strncpy(connected_users[num_connected_users].username, username, sizeof(connected_users[num_connected_users].username) - 1);
    connected_users[num_connected_users].username[sizeof(connected_users[num_connected_users].username) - 1] = '\0';
    strncpy(connected_users[num_connected_users].ip, ip, sizeof(connected_users[num_connected_users].ip) - 1);
    connected_users[num_connected_users].ip[sizeof(connected_users[num_connected_users].ip) - 1] = '\0';
    connected_users[num_connected_users].port = port;
    num_connected_users++;
    return 0;
}

int disconnect_user(char* username) {
    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }

    if (!is_connected(username)) {
        return 2; // El usuario no está conectado
    }

    int found = -1;
    for (int i = 0; i < num_connected_users; i++) { // Buscar el usuario en la lista de conectados
        if (strcmp(connected_users[i].username, username) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return 2; // El usuario no está conectado
    }
    
    if (found < num_connected_users - 1) {
        connected_users[found] = connected_users[num_connected_users - 1];
    }
    
    num_connected_users--;
    return 0;
}

int publish_file(char* username, char* file_name, char* description) {
       if (!is_connected(username)) {
        return 2; // El usuario no está conectado
    }
    
    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }

    // Abrir el archivo de usuario
    char *filepath = get_file_name(username, 1);
    FILE *archivo = fopen(filepath, "a");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir el archivo %s\n", filepath);
        free(filepath);
        return 4;
    }

    // Verificar si el archivo ya existe
    FILE *check = fopen(filepath, "r");
    char line[2048];
    while (fgets(line, sizeof(line), check)) {
        // Comparar hasta la coma en cada línea
        if (strncmp(line, file_name, strlen(file_name)) == 0 && line[strlen(file_name)] == ',') {
            fclose(archivo);
            fclose(check);
            free(filepath);
            return 3; // El archivo ya existe
        }
    }
    fprintf(archivo, "%s,%s\n", file_name, description); // Escribir el nombre y la descripción
    fclose(archivo); // Cerrar el archivo
    free(filepath); // Liberar la memoria
    return 0;
}

int delete_file(char* username, char* file_name) {

    if (!is_connected(username)) {
        return 2; // El usuario no está conectado
    }

    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }

    // Abrir el archivo de usuario y temp
    char *filepath = get_file_name(username, 1);
    const char *temp_file = "storage/temp.txt";
    FILE *in = fopen(filepath, "r");
    FILE *out = fopen(temp_file, "w");

    if (!in || !out) {
        fprintf(stderr, "Error abriendo archivos");
        return 4;
    }

    char line[2048];
    int found = 0;
    // Leer línea por línea y escribir en el archivo temporal
    while (fgets(line, sizeof(line), in)) {
        // Comparar hasta la coma en cada línea
        if (strncmp(line, file_name, strlen(file_name)) == 0 && line[strlen(file_name)] == ',') {
            found = 1; // Se encontró y se va a eliminar
            continue;
        }
        fputs(line, out);
    }

    fclose(in); // Cerrar el archivo original
    fclose(out); // Cerrar el archivo temporal

    if (!found) {
        remove(temp_file);
        return 3; // No se encontró el archivo a eliminar
    }

    // Reemplazar archivo original con el temporal
    remove(filepath);
    rename(temp_file, filepath);

    return 0;
}

char *formatear_list_users(void) {
    // Reservamos suficiente memoria para almacenar la lista de usuarios
    size_t total_size = sizeof(struct user) * num_connected_users + 1 + 5 + 2 * num_connected_users;
    char *info_users = (char *) malloc(total_size);
    if (info_users == NULL) {
        fprintf(stderr, "Error al asignar memoria para la lista de usuarios\n");
        return NULL;
    }

    // Inicializamos la cadena con el número de usuarios conectados
    int offset = snprintf(info_users, total_size, "%d", num_connected_users); // La longitud del número de usuarios

    // Concatenamos la información de cada usuario
    for (int i = 0; i < num_connected_users; i++) {
        // Aseguramos que no sobrepasamos los  bytes
        if (offset < total_size) {
            offset += snprintf(info_users + offset, total_size - offset, ",%s,%s,%d", 
                               connected_users[i].username, 
                               connected_users[i].ip, 
                               connected_users[i].port);
        }
    }
    return info_users;
}

int list_users(char* username) {
    if (exist(username) == 0) {
        return 1; // El usuario no existe
    }

    if (!is_connected(username)) {
        return 2; // El usuario no está conectado
    }

    return 0;
    
}

char *formatear_list_content(char* username) {
    char *filepath = get_file_name(username, 1);
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        return NULL;  // Error al abrir el archivo
    }
    int n_lineas = 0;
    char c;
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') {
            n_lineas++;
        }
    }
    rewind(file);
    size_t total_size = 256 * n_lineas + 4 + 1;
    char *result = malloc(total_size);

    snprintf(result, total_size, "%d", n_lineas); // Guardamos el número de líneas

    char linea[256];
    for (int i = 0; i < n_lineas; i++) {
        if (fgets(linea, 256, file) != NULL) {
            // Buscar la primera coma
            char *coma_pos = strchr(linea, ',');
            int len = coma_pos - linea;
            strcat(result, ",");
            strncat(result, linea, len);
        } else {
            break;  // Si no hay más líneas en el archivo
        }
    }

    return result;
}

int list_content(char* user, char* target) {
    if (!is_connected(user)) {
        return 2; // El usuario no está conectado
    }

    if (exist(user) == 0) {
        return 1; // El usuario no existe
    }

    if (exist(target) == 0) {
        return 3; // El usuario no existe
    }
    return 0;
}

int get_file(char* user, char* target) {
    if (!is_connected(user)) {
        return 2; // El usuario no está conectado
    }

    if (exist(user) == 0) {
        return 1; // El usuario no existe
    }

    if (exist(target) == 0) {
        return 3; // El usuario target no existe
    }

    
    return 0;
}

char *obtener_datos_cliente(char* user) {
    for (int i = 0; i < num_connected_users; i++) {
        if (strcmp(connected_users[i].username, user) == 0) {
            char *datos = malloc(256);
            snprintf(datos, 256, "%s,%d", connected_users[i].ip, connected_users[i].port);
            return datos;
        }
    }
    return NULL; // Usuario no encontrado
}