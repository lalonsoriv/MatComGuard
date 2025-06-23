/*
 * Port Scanner - Implementación del escáner de puertos TCP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>
#include "port_scanner.h"

// Mapeo de servicios comunes
static ServiceMapping common_services[] = {
    {21, "FTP"}, {22, "SSH"}, {23, "Telnet"}, {25, "SMTP"}, {53, "DNS"},
    {80, "HTTP"}, {110, "POP3"}, {143, "IMAP"}, {443, "HTTPS"}, {993, "IMAPS"},
    {995, "POP3S"}, {465, "SMTPS"}, {587, "SMTP"}, {139, "NetBIOS"}, {445, "SMB"},
    {3389, "RDP"}, {5432, "PostgreSQL"}, {3306, "MySQL"}, {1433, "MSSQL"},
    {6379, "Redis"}, {27017, "MongoDB"}, {5672, "RabbitMQ"}, {9200, "Elasticsearch"},
    {0, NULL} // Terminador
};

// Puertos sospechosos conocidos
static SuspiciousPort suspicious_ports[] = {
    {31337, "Backdoor común"}, {12345, "NetBus"}, {54321, "Back Orifice"},
    {6667, "IRC"}, {6666, "IRC/Backdoor"}, {4444, "Metasploit"}, {5555, "Android Debug"},
    {8080, "Proxy/Web alternativo"}, {8888, "Proxy alternativo"}, {9999, "Backdoor común"},
    {1234, "Ultors Trojan"}, {6969, "GateCrasher"}, {7777, "Tini backdoor"},
    {0, NULL} // Terminador
};

typedef struct {
    PortScanner *scanner;
    int port;
    int *result;
} ScanThreadData;

const char* get_service_name(int port) {
    for (int i = 0; common_services[i].service != NULL; i++) {
        if (common_services[i].port == port) {
            return common_services[i].service;
        }
    }
    return "Desconocido";
}

const char* get_suspicious_description(int port) {
    for (int i = 0; suspicious_ports[i].description != NULL; i++) {
        if (suspicious_ports[i].port == port) {
            return suspicious_ports[i].description;
        }
    }
    return NULL;
}

int is_suspicious_port(int port) {
    return get_suspicious_description(port) != NULL;
}

int scan_single_port(const char *host, int port, int timeout) {
    int sock;
    struct sockaddr_in target;
    int flags;
    fd_set fdset;
    struct timeval tv;
    int result;
    socklen_t len;
    int error;
    
    // Crear socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }
    
    // Configurar socket no bloqueante
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Configurar dirección objetivo
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &target.sin_addr) <= 0) {
        close(sock);
        return 0;
    }
    
    // Intentar conexión
    result = connect(sock, (struct sockaddr*)&target, sizeof(target));
    
    if (result < 0) {
        if (errno == EINPROGRESS) {
            // Conexión en progreso, usar select para timeout
            FD_ZERO(&fdset);
            FD_SET(sock, &fdset);
            tv.tv_sec = timeout;
            tv.tv_usec = 0;
            
            result = select(sock + 1, NULL, &fdset, NULL, &tv);
            
            if (result > 0) {
                // Verificar si la conexión fue exitosa
                len = sizeof(error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                    close(sock);
                    return 0;
                }
            } else {
                // Timeout o error
                close(sock);
                return 0;
            }
        } else {
            // Error inmediato
            close(sock);
            return 0;
        }
    }
    
    close(sock);
    return 1; // Puerto abierto
}

int* parse_port_range(const char *port_string, int *count) {
    int *ports = NULL;
    int capacity = 0;
    *count = 0;
    
    char *str_copy = strdup(port_string);
    char *token = strtok(str_copy, ",");
    
    while (token != NULL) {
        // Remover espacios
        while (*token == ' ') token++;
        
        if (strchr(token, '-') != NULL) {
            // Rango de puertos
            int start, end;
            if (sscanf(token, "%d-%d", &start, &end) == 2) {
                for (int port = start; port <= end; port++) {
                    if (*count >= capacity) {
                        capacity = capacity == 0 ? 1000 : capacity * 2;
                        ports = realloc(ports, capacity * sizeof(int));
                    }
                    ports[*count] = port;
                    (*count)++;
                }
            }
        } else {
            // Puerto individual
            int port = atoi(token);
            if (port > 0 && port <= 65535) {
                if (*count >= capacity) {
                    capacity = capacity == 0 ? 1000 : capacity * 2;
                    ports = realloc(ports, capacity * sizeof(int));
                }
                ports[*count] = port;
                (*count)++;
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    free(str_copy);
    
    // Ordenar y eliminar duplicados
    if (*count > 1) {
        // Ordenamiento burbuja simple
        for (int i = 0; i < *count - 1; i++) {
            for (int j = 0; j < *count - i - 1; j++) {
                if (ports[j] > ports[j + 1]) {
                    int temp = ports[j];
                    ports[j] = ports[j + 1];
                    ports[j + 1] = temp;
                }
            }
        }
        
        // Eliminar duplicados
        int unique_count = 1;
        for (int i = 1; i < *count; i++) {
            if (ports[i] != ports[unique_count - 1]) {
                ports[unique_count] = ports[i];
                unique_count++;
            }
        }
        *count = unique_count;
    }
    
    return ports;
}

void* scan_port_thread(void* arg) {
    ScanThreadData *data = (ScanThreadData*)arg;
    *(data->result) = scan_single_port(data->scanner->target_host, data->port, data->scanner->timeout);
    return NULL;
}

PortScanner* port_scanner_create(const char *target_host, int timeout, AlertManager *alert_manager) {
    PortScanner *scanner = malloc(sizeof(PortScanner));
    if (!scanner) return NULL;
    
    strncpy(scanner->target_host, target_host, sizeof(scanner->target_host) - 1);
    scanner->target_host[sizeof(scanner->target_host) - 1] = '\0';
    scanner->timeout = timeout;
    scanner->alert_manager = alert_manager;
    scanner->previous_open_ports = NULL;
    scanner->previous_count = 0;
    scanner->first_scan = 1;
    
    return scanner;
}

void port_scanner_destroy(PortScanner *scanner) {
    if (scanner) {
        if (scanner->previous_open_ports) {
            free(scanner->previous_open_ports);
        }
        free(scanner);
    }
}

int port_scanner_scan(PortScanner *scanner, const char *port_range_string) {
    int port_count;
    int *ports = parse_port_range(port_range_string, &port_count);
    
    if (!ports || port_count == 0) {
        printf("[ERROR] Rango de puertos inválido\n");
        return -1;
    }
    
    printf("[INFO] Escaneando %d puertos en %s...\n", port_count, scanner->target_host);
    
    // Arrays para resultados
    int *results = calloc(port_count, sizeof(int));
    int *open_ports = malloc(port_count * sizeof(int));
    int open_count = 0;
    
    // Escanear puertos (limitando threads para evitar sobrecargar el sistema)
    const int MAX_THREADS = 50;
    int active_threads = 0;
    
    for (int i = 0; i < port_count; i++) {
        results[i] = scan_single_port(scanner->target_host, ports[i], scanner->timeout);
        
        if (results[i]) {
            open_ports[open_count] = ports[i];
            open_count++;
        }
        
        // Mostrar progreso cada 100 puertos
        if ((i + 1) % 100 == 0 || i == port_count - 1) {
            printf("[INFO] Progreso: %d/%d puertos escaneados\n", i + 1, port_count);
        }
    }
    
    // Detectar cambios desde el último escaneo
    if (!scanner->first_scan && scanner->previous_open_ports) {
        int new_ports[1024], closed_ports[1024];
        int new_count = 0, closed_count = 0;
        
        // Encontrar nuevos puertos
        for (int i = 0; i < open_count; i++) {
            int found = 0;
            for (int j = 0; j < scanner->previous_count; j++) {
                if (open_ports[i] == scanner->previous_open_ports[j]) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                new_ports[new_count++] = open_ports[i];
            }
        }
        
        // Encontrar puertos cerrados
        for (int i = 0; i < scanner->previous_count; i++) {
            int found = 0;
            for (int j = 0; j < open_count; j++) {
                if (scanner->previous_open_ports[i] == open_ports[j]) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                closed_ports[closed_count++] = scanner->previous_open_ports[i];
            }
        }
        
        // Mostrar cambios
        if (new_count > 0) {
            printf("\n[CAMBIO] Nuevos puertos abiertos: ");
            for (int i = 0; i < new_count; i++) {
                printf("%d%s", new_ports[i], i < new_count - 1 ? "," : "");
            }
            printf("\n");
        }
        
        if (closed_count > 0) {
            printf("[CAMBIO] Puertos cerrados: ");
            for (int i = 0; i < closed_count; i++) {
                printf("%d%s", closed_ports[i], i < closed_count - 1 ? "," : "");
            }
            printf("\n");
        }
        
        if (new_count == 0 && closed_count == 0) {
            printf("[INFO] Sin cambios detectados\n");
        }
    }
    
    // Analizar puertos abiertos
    if (open_count > 0) {
        printf("\n[RESULTADO] %d puertos abiertos encontrados:\n", open_count);
        
        for (int i = 0; i < open_count; i++) {
            int port = open_ports[i];
            const char *service = get_service_name(port);
            const char *suspicious_desc = get_suspicious_description(port);
            
            const char *status;
            const char *emoji;
            AlertLevel alert_level;
            
            if (suspicious_desc) {
                status = "ALERTA";
                emoji = "🔴";
                alert_level = ALERT_HIGH;
            } else if (port > 1024 && strcmp(service, "Desconocido") == 0) {
                status = "ADVERTENCIA";
                emoji = "🟡";
                alert_level = ALERT_MEDIUM;
            } else if (strcmp(service, "Desconocido") == 0) {
                status = "ADVERTENCIA";
                emoji = "🟡";
                alert_level = ALERT_MEDIUM;
            } else {
                status = "OK";
                emoji = "🟢";
                alert_level = ALERT_LOW;
            }
            
            char message[512];
            if (suspicious_desc) {
                snprintf(message, sizeof(message), "[%s] Puerto %d/tcp abierto (%s)", 
                        status, port, suspicious_desc);
            } else {
                snprintf(message, sizeof(message), "[%s] Puerto %d/tcp (%s) abierto", 
                        status, port, service);
            }
            
            printf("%s %s\n", emoji, message);
            
            // Registrar alerta si es necesario
            if (scanner->alert_manager && (alert_level == ALERT_HIGH || alert_level == ALERT_MEDIUM)) {
                Alert alert;
                alert.level = alert_level;
                strncpy(alert.message, message, sizeof(alert.message) - 1);
                alert.message[sizeof(alert.message) - 1] = '\0';
                alert.port = port;
                strncpy(alert.service, suspicious_desc ? suspicious_desc : service, sizeof(alert.service) - 1);
                alert.service[sizeof(alert.service) - 1] = '\0';
                alert.timestamp = time(NULL);
                
                alert_manager_add_alert(scanner->alert_manager, &alert);
            }
        }
    } else {
        printf("\n[INFO] No se encontraron puertos abiertos\n");
    }
    
    // Actualizar estado anterior
    if (scanner->previous_open_ports) {
        free(scanner->previous_open_ports);
    }
    scanner->previous_open_ports = malloc(open_count * sizeof(int));
    if (scanner->previous_open_ports) {
        memcpy(scanner->previous_open_ports, open_ports, open_count * sizeof(int));
        scanner->previous_count = open_count;
    }
    scanner->first_scan = 0;
    
    // Limpieza
    free(ports);
    free(results);
    free(open_ports);
    
    return 0;
}
