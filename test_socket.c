/*
 * Test Socket - Programa auxiliar para pruebas de detección en tiempo real
 * Crea un socket TCP temporal para verificar la detección de MatcomGuard
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

volatile int keep_running = 1;

void signal_handler(int signum) {
    keep_running = 0;
    printf("\n[INFO] Recibida señal %d, cerrando socket...\n", signum);
}

int main(int argc, char *argv[]) {
    int port = 9999;
    int duration = 30;
    
    // Parsear argumentos
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Puerto inválido %d\n", port);
            return 1;
        }
    }
    
    if (argc >= 3) {
        duration = atoi(argv[2]);
        if (duration <= 0) {
            fprintf(stderr, "Error: Duración inválida %d\n", duration);
            return 1;
        }
    }
    
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Crear socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creando socket");
        return 1;
    }
    
    // Configurar reutilización de dirección
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error configurando SO_REUSEADDR");
        close(sock_fd);
        return 1;
    }
    
    // Configurar dirección
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);
    
    // Hacer bind
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error haciendo bind al puerto %d: %s\n", port, strerror(errno));
        close(sock_fd);
        return 1;
    }
    
    // Hacer listen
    if (listen(sock_fd, 5) < 0) {
        perror("Error haciendo listen");
        close(sock_fd);
        return 1;
    }
    
    printf("[INFO] Socket abierto en puerto %d por %d segundos\n", port, duration);
    printf("[INFO] PID: %d\n", getpid());
    fflush(stdout);
    
    // Esperar por tiempo especificado o hasta recibir señal
    for (int i = 0; i < duration && keep_running; i++) {
        sleep(1);
    }
    
    // Cerrar socket
    close(sock_fd);
    printf("[INFO] Socket cerrado\n");
    
    return 0;
}
