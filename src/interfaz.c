#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void create_fifo(const char *path) {
    if (mkfifo(path, 0666) == -1 && access(path, F_OK) == -1) {
        perror("mkfifo");
        exit(1);
    }
}

void send_message(const char *fifo_path, const char *message) {
    int fd = open(fifo_path, O_WRONLY);
    if (fd == -1) { perror("open send"); exit(1); }
    write(fd, message, strlen(message) + 1);
    close(fd);
}

void receive_message(const char *fifo_path, char *buffer, size_t size) {
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) { perror("open recv"); exit(1); }
    read(fd, buffer, size);
    close(fd);
}

int main() {
    const char *fifo_req = "fifo_req";
    const char *fifo_resp = "fifo_resp";
    create_fifo(fifo_req);
    create_fifo(fifo_resp);

    printf("=== P1 Data Program ===\n");

    while (1) {
        printf("\n1) Buscar libro\n2) Salir\n> ");
        int opcion;
        scanf("%d", &opcion);
        getchar(); // limpiar salto de línea

        if (opcion == 2) break;

        char titulo[256];
        printf("Ingrese el título del libro: ");
        fgets(titulo, sizeof(titulo), stdin);
        titulo[strcspn(titulo, "\n")] = '\0';

        // enviar solicitud
        send_message(fifo_req, titulo);

        // recibir respuesta
        char respuesta[1024];
        receive_message(fifo_resp, respuesta, sizeof(respuesta));

        printf("\n📚 Resultado del worker:\n%s\n", respuesta);
    }

    printf("Saliendo...\n");
    return 0;
}
