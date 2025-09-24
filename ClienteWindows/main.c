#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8778
#define ACK 1
#define END_MARK "\r\n"
#define VERSION 1
#define IPSERVIDOR "10.2.192.11"

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

int dir(SOCKET sockfd) {
    char buffer[1024];
    int bytes;

    char full_buffer[4096] = {0};
    while ((bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        strcat(full_buffer, buffer);

        if (strstr(full_buffer, END_MARK) != NULL) {
            break;
        }

        printf("%s", buffer);
    }

    printf("Directorio recibido:\n%s", full_buffer);


    if (bytes == SOCKET_ERROR) {
        perror("Error al leer datos");
        return -1;
    }

    return 0;
}

int cd(SOCKET sockfd) {
    int res;
    char new_dir[512];
    char response[256];
    int ack;

    if ((res = recv(sockfd, (void *)&ack, sizeof(ack), 0)) < 0) {
        perror("recv\n");
        return -1;
    }

    if (ack == ACK) {
        printf("Donde te quieres mover?\n");
        scanf("%s", new_dir);

        if ((res = send(sockfd, (void *)&new_dir, sizeof(new_dir), 0)) < 0) {
            perror("send\n");
            return -1;
        }

        if ((res = recv(sockfd, (void *)&response, sizeof(response), 0)) < 0) {
            perror("recv\n");
            return -1;
        }

        printf("%s\n", response);

    } else {
        printf("Error, servidor no ack\n");
    }

    return 0;
}

int get(SOCKET sockfd) {
    int res;
    char filename[1024];
    char buff[1024];
    FILE *f;
    long file_size;
    int ack;

    if ((res = recv(sockfd, (void *)&ack, sizeof(ack), 0)) < 0) {
        perror("recv ack\n");
        return -1;
    }

    if (ack != ACK) {
        printf("Error: servidor no autoriza la operaci�n\n");
        return -1;
    }

    printf("Dame el nombre del archivo que quieres descargar: \n");
    scanf(" %1023[^\n]", filename);
    getchar();

    if (send(sockfd, filename, sizeof(filename), 0) < 0) {
        perror("send filename\n");
        return -1;
    }

    if ((res = recv(sockfd, &file_size, sizeof(file_size), 0)) < 0) {
        perror("recv file size\n");
        return -1;
    }

    f = fopen(filename, "wb");

    if (f == NULL) {
        perror("fopen\n");
        return -1;
    }

    long bytes_received = 0;
    while (bytes_received < file_size) {
        res = recv(sockfd, buff, sizeof(buff), 0);
        if (res < 0) {
            perror("recv\n");
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

int rget(SOCKET sockfd) {
    char dir_name[512], tar_filename[512], buff[1024];
    int res;
    FILE *f;
    long file_size;
    int ack;

    if ((res = recv(sockfd, (void *)&ack, sizeof(ack), 0)) < 0) {
        perror("recv ack\n");
        return -1;
    }

    if (ack != ACK) {
        printf("Error: servidor no autoriza la operación\n");
        return -1;
    }

    printf("Ingrese el nombre del directorio a descargar: ");
    scanf("%s", dir_name);

    if (send(sockfd, dir_name, sizeof(dir_name), 0) < 0) {
        perror("send dir_name");
        return -1;
    }
    printf("Nombre del directorio enviado: %s\n", dir_name);

    snprintf(tar_filename, sizeof(tar_filename), "%s.tar.gz", dir_name);
    printf("Esperando recibir el archivo: %s\n", tar_filename);

    // Recibir el tamaño del archivo
    if ((res = recv(sockfd, (char *)&file_size, sizeof(file_size), 0)) < 0) {
        perror("recv file size\n");
        return -1;
    }

    printf("Tamaño del archivo esperado: %ld bytes\n", file_size);

    f = fopen(tar_filename, "wb");
    if (!f) {
        perror("fopen");
        return -1;
    }

    long bytes_received = 0;
    while (bytes_received < file_size) {
        res = recv(sockfd, buff, sizeof(buff), 0);
        if (res < 0) {
            perror("recv\n");
            fclose(f);
            return -1;
        }
        fwrite(buff, 1, res, f);
        bytes_received += res;
    }

    fclose(f);
    printf("Archivo %s recibido correctamente (%ld bytes)\n", tar_filename, bytes_received);

    // Descomprimir usando winrar (tar.gz)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\\WinRAR\\winrar.exe\" x %s ", getenv("ProgramFiles"), tar_filename);
    if (system(cmd) != 0) {
        perror("Error al descomprimir con winrar");
        return -1;
    }

    // Eliminar el archivo .tar.gz
    snprintf(cmd, sizeof(cmd), "del %s", tar_filename);
    if (system(cmd) != 0) {
        perror("Error al eliminar archivo .tar.gz");
        return -1;
    }

    //tar
    snprintf(tar_filename, sizeof(tar_filename), "%s.tar", dir_name);
    printf("Descomprimiendo archivo .tar: %s\n", tar_filename);

    snprintf(cmd, sizeof(cmd), "\"%s\\WinRAR\\winrar.exe\" x %s ", getenv("ProgramFiles"), tar_filename);
    if (system(cmd) != 0) {
        perror("Error al descomprimir archivo .tar con WinRAR");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "del %s", tar_filename);
    if (system(cmd) != 0) {
        perror("Error al eliminar archivo .tar");
        return -1;
    }

    return 0;
}

int main() {
    int res;
    SOCKET sockfd;
    WSADATA wsaData;
    struct sockaddr_in server;
    int ack;
    t_header header;

    char usr[512];
    char psw[512];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Error al inicializar Winsock\n");
        return -1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Error al crear socket\n");
        WSACleanup();
        return -1;
    }

    memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IPSERVIDOR);

    if ((res = connect(sockfd, (struct sockaddr *)&server, sizeof(server))) == SOCKET_ERROR) {
        printf("Error al conectar con el servidor\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    if ((res = recv(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
        perror("recv\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    printf("%s", header.data);
    scanf("%s", usr);

    header.op = OP_REQUEST_USER;
    header.version = VERSION;
    strcpy(header.data, usr);

    if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
        perror("send\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    if ((res = recv(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
        perror("recv\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    printf("%s", header.data);
    scanf("%s", psw);

    header.op = OP_REQUEST_PASSWD;
    header.version = VERSION;
    strcpy(header.data, psw);

    if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
        perror("send\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    if ((res = recv(sockfd, (void *)&ack, sizeof(ack), 0)) < 0) {
        perror("recv\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    if (ack == ACK) {
        int option;
        while (1) {
            header.version = VERSION;
            strcpy(header.data, "");

            printf("1) dir\n");
            printf("2) cd\n");
            printf("3) get\n");
            printf("4) rget\n");
            printf("5) exit\n");
            scanf("%d", &option);

            if (option == 5) break;
            if (option == 1) {
                header.op = OP_DIR;

                if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0){
                    perror("send\n");
                    closesocket(sockfd);
                    WSACleanup();
                    return -1;
                }
                dir(sockfd);
            }
            if (option == 2) {
                header.op = OP_CD;
                if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
                    perror("send\n");
                    closesocket(sockfd);
                    WSACleanup();
                    return -1;
                }
                cd(sockfd);
            }
            if (option == 3) {
                header.op = OP_GET;
                if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
                    perror("send\n");
                    closesocket(sockfd);
                    WSACleanup();
                    return -1;
                }
                get(sockfd);
            }
            if (option == 4) {
                header.op = OP_RGET;
                if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
                    perror("send\n");
                    closesocket(sockfd);
                    WSACleanup();
                    return -1;
                }
                rget(sockfd);
            }
        }
    }

    header.op = OP_EXIT;
    header.version = VERSION;
    strcpy(header.data, "");

    if ((res = send(sockfd, (void *)&header, sizeof(t_header), 0)) < 0) {
        perror("send\n");
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    if (closesocket(sockfd) == SOCKET_ERROR) {
        printf("Error al cerrar el socket\n");
        WSACleanup();
        return -1;
    }

    WSACleanup();
    return 0;
}

