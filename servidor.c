#include <sys/socket.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8778
#define MAX_CLIENTS 32
#define ACK 1
#define END_MARK "\r\n"
#define VERSION 1

#define OP_REQUEST_USER 0
#define OP_REQUEST_PASSWD 1
#define OP_DIR 5
#define OP_CD 6
#define OP_GET 7
#define OP_RGET 8
#define OP_EXIT 666

typedef enum {
    LLIURE = 0,
    OCUPAT = 1
} e_estat;

typedef struct {
    pthread_t th;       
    int sockid;
    char ip[20];
    char usr_path[256];
    e_estat estat;
} t_data;

typedef struct {
    int op;
    int version;
    char data[1024];
} t_header;

t_data clients[MAX_CLIENTS];

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER; 

int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int get(int clifd, char *path) {
    int res;
    char buff[1024];
    char filename[1024];
    char filepath[2048];
    FILE *f;

    //nombre del fichero
    if ((res = read(clifd, (void *)&filename, sizeof(filename))) < 0) {
        perror("read\n");
        return -1;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);

    //por si es un directorio
    if (is_directory(filepath)) {
        char error_msg[] = "Error: Es un directorio. Use RGET para descargar directorios\n";
        if (write(clifd, error_msg, strlen(error_msg)) < 0) {
            perror("write\n");
        }
        return -1;
    }

    f = fopen(filepath, "rb");
    if (f == NULL) {
        perror("fopen\n");
        char result[256];
        snprintf(result, sizeof(result), "Error: %s\n", strerror(errno));
        if ((res = write(clifd, (void *)&result, sizeof(result))) < 0) {
            perror("write\n");
            return -1;
        }
        return -1;
    }

    //tamaño del fichero
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);
//se lo paso al ciente
    if ((res = write(clifd, &file_size, sizeof(file_size))) < 0) {
        perror("write file size\n");
        fclose(f);
        return -1;
    }

//aqui le paso el fichero
    while ((res = fread(buff, 1, sizeof(buff), f)) > 0) {
        if (write(clifd, buff, res) < 0) {
            perror("write\n");
            fclose(f);
            return -1;
        }
        memset(buff, 0, sizeof(buff));
    }

    fclose(f);
    return 0;
}


int cd(int clifd, char *path){
    int res;
    char temp_path[1024];
    char resolved_path[512];
    char new_dir[512];

    if((res = read(clifd, (void *)&new_dir, sizeof(new_dir))) < 0){
        perror("read\n");
        return -1;
    }

    snprintf(temp_path, sizeof(temp_path), "%s/%s", path, new_dir);

    if(realpath(temp_path, resolved_path) == NULL){
        perror("realpath\n");
        char response[256];
        snprintf(response, sizeof(response), "Error: %s\n", strerror(errno));
        if((res = write(clifd, response, strlen(response))) < 0){
            perror("write\n");
            return -1;
        }
        return -1;
    }

    strcpy(path, resolved_path);
    char response[256];
    snprintf(response, sizeof(response), "Directorio cambiado a: %s\n", path);
    if((res = write(clifd, response, strlen(response))) < 0){
        perror("write\n");
        return -1;
    }
    return 0;
}

int rget(int clifd, char *path) {
    char dir_name[512], tar_path[1024], cmd[2048];
    int res;

    if ((res = read(clifd, dir_name, sizeof(dir_name))) < 0) {
        perror("read dir_name");
        return -1;
    }
    
    char full_dir_path[1024];
    snprintf(full_dir_path, sizeof(full_dir_path), "%s/%s", path, dir_name);

    struct stat st;
    if (stat(full_dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        char error_msg[] = "Error: El directorio no existe\n";
        write(clifd, error_msg, strlen(error_msg));
        return -1;
    }

//Comprimir directorio (idea de marc)
    snprintf(tar_path, sizeof(tar_path), "%s/%s.tar.gz", path, dir_name);
    snprintf(cmd, sizeof(cmd), "tar -czf %s -C %s %s", tar_path, path, dir_name);
    if (system(cmd) != 0) {
        perror("Error al comprimir el directorio");
        return -1;
    }

    FILE *f = fopen(tar_path, "rb");
    if (!f) {
        perror("fopen tar.gz");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (write(clifd, &file_size, sizeof(file_size)) < 0) {
        perror("write file size");
        fclose(f);
        return -1;
    }

    char buff[1024];
    while ((res = fread(buff, 1, sizeof(buff), f)) > 0) {
        if (write(clifd, buff, res) < 0) {
            perror("write tar.gz");
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    snprintf(cmd, sizeof(cmd), "rm %s", tar_path);
    system(cmd);

    return 0;
}


int dir(int clifd, char *path) {
    DIR *dir;
    struct dirent *entry;
    char buffer[1024];

    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
        if (write(clifd, buffer, strlen(buffer)) == -1) {
            perror("Error al escribir datos");
            closedir(dir);
            return -1;
        }
    }

    snprintf(buffer, sizeof(buffer), END_MARK);
    if (write(clifd, buffer, strlen(buffer)) == -1) {
        perror("Error al enviar el indicador de fin");
        closedir(dir);
        return -1;
    }

    closedir(dir);
    return 0;
}


int send_answer(int clifd, int ack){
    if(write(clifd, (void *)&ack, sizeof(int)) < 0){
        return -1;
    }
    return 0;
}

void *atendre_client(void *d){
    t_data *data = (t_data *)d;
    int clifd = data->sockid;
    t_header header;
    int res;

    char usr[1024];
    char psw[1024];

    header.op = OP_REQUEST_USER;
    header.version = VERSION;
    strcpy(header.data, "Ingrese su usuario\n");
    if((res = write(clifd, (void *)&header, sizeof(t_header))) < 0){
        perror("write\n");
    }

    if((res = read(clifd, (void *)&header, sizeof(t_header))) < 0){
        perror("read usuario\n");
    }    
    printf("Usuario recibido: %s\n", header.data);
    strcpy(usr, header.data);

    header.op = OP_REQUEST_PASSWD;
    header.version = VERSION;
    strcpy(header.data, "Ingrese su contraseña\n");

    if((res = write(clifd, (void *)&header, sizeof(t_header))) < 0){
        perror("write\n");
    }

    if((res = read(clifd, (void *)&header, sizeof(t_header))) < 0){
        perror("read contraseña\n");
    }   

    printf("Contraseña recibida: %s\n", header.data);
    strcpy(psw, header.data);

    if(strcmp(usr, "Usuario") == 0 && strcmp(psw, "Usuario") == 0){
        send_answer(clifd, ACK);
        if((res = read(clifd, (void *)&header, sizeof(t_header))) < 0){
            perror("read\n");
        }

        while(header.op != OP_EXIT){
            if(header.op == OP_DIR && header.version == VERSION){
                send_answer(clifd, ACK);
                printf("Un cliente quiere hacer un dir\n");
                dir(clifd, data->usr_path);
            }else if(header.op == OP_CD && header.version == VERSION){
                send_answer(clifd, ACK);
                printf("Un cliente quiere hacer un cd\n");
                cd(clifd, data->usr_path);
            }else if(header.op == OP_GET && header.version == VERSION){
                send_answer(clifd, ACK);
                printf("Un cliente quiere hacer un get\n");
                get(clifd, data->usr_path);
            }else if(header.op == OP_RGET && header.version == VERSION){
                send_answer(clifd, ACK);
                printf("Un cliente quiere hacer un rget\n");
                rget(clifd, data->usr_path);
            }else{
                send_answer(clifd, 0);
            }

            if((res = read(clifd, (void *)&header, sizeof(t_header))) < 0){
                if(errno == EINTR) continue;
                perror("read\n");
                break;
            }
        }
    }else{
        send_answer(clifd, 0);
    }

    printf("Un cliente se ha desconectado\n");

    if((res = close(clifd)) < 0){
        perror("close\n");
    }
    pthread_mutex_lock(&mut);
    data->estat = LLIURE;
    pthread_mutex_unlock(&mut);
}

void endapp(int signum) {
    sleep(2);
    exit(0);  
}

int okupa_taula() {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].estat == LLIURE) {
            clients[i].estat = OCUPAT;  
            return i;
        }
    }
    return -1;
}

int main(){
    int sockfd, res, clifd;
    struct sockaddr_in server, client;
    socklen_t len;
    int pos;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket\n");
        return -1;
    }

    memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if((res = bind(sockfd, (void *)&server, sizeof(server))) < 0){
        perror("bind\n");
        return -1;
    }

    if((res = listen(sockfd, 10)) < 0){
        perror("listen\n");
        return -1;
    }

    signal(SIGINT, endapp);
    len = sizeof(client);

    printf("Servidor iniciado correctamente\n");

    while(1){
        clifd = accept(sockfd, (struct sockaddr *)&client , &len);
    
        while(clifd < 0 && errno == EINTR){
            clifd = accept(sockfd, (struct sockaddr *)&client , &len);
        }
        if(clifd < 0){
            perror("accept\n");
            continue;
        }

        pthread_mutex_lock(&mut);
        pos = okupa_taula();
        if(pos < 0){
            close(clifd);
        }else{
            clients[pos].sockid = clifd;
            sprintf(clients[pos].ip, "%s", inet_ntoa(client.sin_addr)); //IP del cliente
            strcpy(clients[pos].usr_path, "/");

            pthread_create(&clients[pos].th, NULL, atendre_client, (void *)&clients[pos]);
            pthread_detach(clients[pos].th);
        }

        pthread_mutex_unlock(&mut);
    }

    if((res = close(sockfd)) < 0){
        perror("close\n");
        return -1;
    }

    return 0;
}