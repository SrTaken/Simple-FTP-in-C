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
#include <sys/stat.h> 

#define PORT 8778
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

typedef struct {
    int op;
    int version;
    char data[1024];
} t_header;

int dir(int sockfd) {
    char buffer[1024];
    int bytes;

    while ((bytes = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0'; 
        if (strstr(buffer, END_MARK) != NULL) { 
            break; 
        }
        printf("%s", buffer); 
    }

    if (bytes == -1) {
        perror("Error al leer datos");
        return -1;
    }

    return 0;
}


int cd(int sockfd){
    int res;
    char new_dir[512];
    char response[256];
    int ack;

    if((res = read(sockfd, (void *)&ack, sizeof(ack)))<0){
        perror("read\n");
        return -1;
    }

    if(ack == ACK){
        printf("Donde te quieres mover?\n");
        scanf("%s", new_dir);

        if((res = write(sockfd, (void *)&new_dir, sizeof(new_dir))) < 0){
            perror("write\n");
            return -1;
        }

        if((res = read(sockfd, (void *)&response, sizeof(response))) < 0){
            perror("write\n");
            return -1;
        }

        printf("%s\n", response);

    }else{
        printf("Error, servidor no ack\n");
    }

    return 0;
}


int get(int sockfd) {
    int res;
    char filename[1024];
    char buff[1024];
    FILE *f;
    long file_size;
    int ack;

    if ((res = read(sockfd, (void *)&ack, sizeof(ack))) < 0) {
        perror("read ack\n");
        return -1;
    }

    if (ack != ACK) {
        printf("Error: servidor no autoriza la operación\n");
        return -1;
    }

    printf("Dame el nombre del archivo que quieres descargar: \n");
    scanf(" %1023[^\n]", filename);
    getchar();

    if (write(sockfd, filename, sizeof(filename)) < 0) {
        perror("write filename\n");
        return -1;
    }

    if ((res = read(sockfd, &file_size, sizeof(file_size))) < 0) {
        perror("read file size\n");
        return -1;
    }

    f = fopen(filename, "wb");

    if (f == NULL) {
        perror("fopen\n");
        return -1;
    }

    long bytes_received = 0;
    while (bytes_received < file_size) {
        res = read(sockfd, buff, sizeof(buff));
        if (res < 0) {
            perror("read\n");
            fclose(f);
            return -1;
        }
        fwrite(buff, 1, res, f);
        bytes_received += res;
    }

    printf("Archivo %s descargado correctamente\n", filename);
    fclose(f);
    return 0;
}


int rget(int sockfd) {
    char dir_name[512], tar_filename[512], buff[1024];
    int res;
    FILE *f;
    long file_size;
    int ack;

    if ((res = read(sockfd, (void *)&ack, sizeof(ack))) < 0) {
        perror("read ack\n");
        return -1;
    }

    if (ack != ACK) {
        printf("Error: servidor no autoriza la operación\n");
        return -1;
    }

    printf("Ingrese el nombre del directorio a descargar: ");
    scanf("%s", dir_name);

    if (write(sockfd, dir_name, sizeof(dir_name)) < 0) {
        perror("write dir_name");
        return -1;
    }
    printf("Nombre del directorio enviado: %s\n", dir_name);

    snprintf(tar_filename, sizeof(tar_filename), "%s.tar.gz", dir_name);
    printf("Esperando recibir el archivo: %s\n", tar_filename);

    if ((res = read(sockfd, &file_size, sizeof(file_size))) < 0) {
        perror("read file size\n");
        return -1;
    }

    f = fopen(tar_filename, "wb");
    if (!f) {
        perror("fopen");
        return -1;
    }

    long bytes_received = 0;
    while (bytes_received < file_size) {
        res = read(sockfd, buff, sizeof(buff));
        if (res < 0) {
            perror("read\n");
            fclose(f);
            return -1;
        }
        fwrite(buff, 1, res, f);
        bytes_received += res;
    }

    printf("Archivo %s recibido correctamente\n", tar_filename);
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tar -xzf %s", tar_filename);
    if (system(cmd) != 0) {
        perror("system(cmd)");
        return -1;
    }
    
    snprintf(cmd, sizeof(cmd), "rm %s", tar_filename); //eliminar tar
    return 0;
}



int main(){
    int res, sockfd;
    struct sockaddr_in server;
    int ack;
    char result[512];
    t_header header;

    char usr[512];
    char psw[512];

    //crear socket para cliente
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    //conectarse al servidor
    if ((res = connect(sockfd, (struct sockaddr *)&server, sizeof(server))) < 0) {
        perror("connect");
        return -1;
    }

    //recibe mensaje "ingrese su usuario"
    if((res = read(sockfd, (void *)&header, sizeof(t_header))) < 0){
        perror("read\n");
        return -1;
    }

    printf("%s", header.data);
    scanf("%s", usr);

    //mandar usr

    header.op = OP_REQUEST_USER;
    header.version = VERSION;
    strcpy(header.data, usr);

    if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
        perror("write\n");
        return -1;
    }

    if((res = read(sockfd, (void *)&header, sizeof(t_header))) < 0){
        perror("read\n");
        return -1;
    }

    printf("%s", header.data);
    scanf("%s", psw);

    //mandar psw
    header.op = OP_REQUEST_PASSWD;
    header.version = VERSION;
    strcpy(header.data, psw);

    if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
        perror("write\n");
        return -1;
    }

    //ack del usuario 
    if((res = read(sockfd, (void *)&ack, sizeof(ack))) < 0){
        perror("read\n");
        return -1;
    }

    if(ack == ACK){
        int option;
        while(1){
            header.version = VERSION;
            strcpy(header.data, "");

            printf("1) dir\n");
            printf("2) cd\n");
            printf("3) get\n");
            printf("4) rget\n");
            printf("5) exit\n");
            scanf("%d", &option);

            if(option == 5) break;
            if(option == 1){
                header.op = OP_DIR;

                if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
                    perror("write\n");
                    return -1;
                }
                dir(sockfd);
            } 
            if(option == 2){
                header.op = OP_CD;
                if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
                    perror("write\n");
                    return -1;
                }
                cd(sockfd);
            } 
            if(option == 3){
                header.op = OP_GET;
                if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
                    perror("write\n");
                    return -1;
                }
                get(sockfd);
            }
            if(option == 4){
                header.op = OP_RGET;
                if((res = write(sockfd, (void *)&header, sizeof(t_header))) < 0){
                    perror("write\n");
                    return -1;
                }
                rget(sockfd);
            }
        }
    }

    header.op = OP_EXIT;
    header.version = VERSION;
    strcpy(header.data, "");

    if((res = write(sockfd ,(void *)&header, sizeof(t_header)))< 0){
        perror("write\n");
        return -1;
    }

    if ((res = close(sockfd)) < 0) {
        perror("close");
        return -1;
    }

    return 0;
}