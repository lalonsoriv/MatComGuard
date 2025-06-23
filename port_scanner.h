/*
 * Port Scanner - Escáner de puertos TCP con detección de servicios y anomalías
 */

#ifndef PORT_SCANNER_H
#define PORT_SCANNER_H

#include "alert_manager.h"

typedef struct {
    char target_host[256];
    int timeout;
    AlertManager *alert_manager;
    int *previous_open_ports;
    int previous_count;
    int first_scan;
} PortScanner;

typedef struct {
    int port;
    const char *service;
} ServiceMapping;

typedef struct {
    int port;
    const char *description;
} SuspiciousPort;

// Funciones públicas
PortScanner* port_scanner_create(const char *target_host, int timeout, AlertManager *alert_manager);
void port_scanner_destroy(PortScanner *scanner);
int port_scanner_scan(PortScanner *scanner, const char *port_range_string);

// Funciones auxiliares
int* parse_port_range(const char *port_string, int *count);
int scan_single_port(const char *host, int port, int timeout);
const char* get_service_name(int port);
const char* get_suspicious_description(int port);
int is_suspicious_port(int port);

#endif
