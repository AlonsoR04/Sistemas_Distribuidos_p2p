#ifndef FUNCIONES_H
#define FUNCIONES_H

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

#define MAX_USER_LENGTH 256
#define DIR_NAME "storage"

struct user {
    char username[MAX_USER_LENGTH];
    char ip[16];
    int port;
};

extern struct user *connected_users;
extern int num_connected_users;
extern int max_connected_users;

int exist(char* username);
int register_user(char* username);  
int unregister_user(char* username);
int connect_user(char* username, char *ip, int port);
int disconnect_user(char* username);
int publish_file(char* username, char* file_name, char* description);
int delete_file(char* username, char* file_name);
int list_users(char* username);
int list_content(char*user, char* target);
int get_file(char* user, char* target);

char *formatear_list_users(void);
char *formatear_list_content(char* target);
char *obtener_datos_cliente(char* user);

DIR *open_folder(void);
int exists_user(const char* username);

#endif