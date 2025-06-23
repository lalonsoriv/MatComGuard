/*
 * MatcomGuard - Sistema de Monitoreo de Seguridad
 * Escáner de puertos en tiempo real para sistemas Unix-like
 * 
 * Compilar: gcc -o matcomguard matcomguard.c port_scanner.c alert_manager.c report_generator.c -lpthread
 * Uso: ./matcomguard --scan-ports 1-1024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include "port_scanner.h"
#include "alert_manager.h"
#include "report_generator.h"

#define VERSION "1.0.0"
#define MAX_TARGET_LEN 256

// Variables globales para manejo de señales
volatile int keep_running = 1;
AlertManager *global_alert_manager = NULL;

void print_banner() {
    printf("============================================================\n");
    printf("        MATCOMGUARD - ESCÁNER DE PUERTOS v%s\n", VERSION);
    printf("============================================================\n");
}

void print_usage(const char *program_name) {
    printf("Uso: %s [OPCIONES]\n\n", program_name);
    printf("Opciones:\n");
    printf("  --scan-ports RANGO    Rango de puertos a escanear (ej: 1-1024, 80,443,22)\n");
    printf("  --target IP           IP objetivo (por defecto: 127.0.0.1)\n");
    printf("  --continuous          Monitoreo continuo en tiempo real\n");
    printf("  --interval SEGUNDOS   Intervalo entre escaneos (por defecto: 30)\n");
    printf("  --timeout SEGUNDOS    Timeout para conexiones TCP (por defecto: 3)\n");
    printf("  --export-pdf          Exportar alertas a PDF al finalizar\n");
    printf("  --help               Mostrar esta ayuda\n");
    printf("  --version            Mostrar versión\n\n");
    printf("Ejemplos:\n");
    printf("  %s --scan-ports 1-1024\n", program_name);
    printf("  %s --scan-ports 1-65535 --target 192.168.1.1\n", program_name);
    printf("  %s --scan-ports 80,443,22,21 --continuous\n", program_name);
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\n[INFO] Señal de interrupción recibida. Finalizando...\n");
        keep_running = 0;
    }
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

int main(int argc, char *argv[]) {
    char target[MAX_TARGET_LEN] = "127.0.0.1";
    char *port_range = NULL;
    int continuous = 0;
    int interval = 30;
    int timeout = 3;
    int export_pdf = 0;
    
    // Opciones de línea de comandos
    static struct option long_options[] = {
        {"scan-ports", required_argument, 0, 'p'},
        {"target", required_argument, 0, 't'},
        {"continuous", no_argument, 0, 'c'},
        {"interval", required_argument, 0, 'i'},
        {"timeout", required_argument, 0, 'T'},
        {"export-pdf", no_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "p:t:ci:T:ehv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p':
                port_range = strdup(optarg);
                break;
            case 't':
                strncpy(target, optarg, MAX_TARGET_LEN - 1);
                target[MAX_TARGET_LEN - 1] = '\0';
                break;
            case 'c':
                continuous = 1;
                break;
            case 'i':
                interval = atoi(optarg);
                if (interval < 1) {
                    fprintf(stderr, "Error: El intervalo debe ser mayor a 0\n");
                    return 1;
                }
                break;
            case 'T':
                timeout = atoi(optarg);
                if (timeout < 1) {
                    fprintf(stderr, "Error: El timeout debe ser mayor a 0\n");
                    return 1;
                }
                break;
            case 'e':
                export_pdf = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                printf("MatcomGuard versión %s\n", VERSION);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Verificar argumentos requeridos
    if (!port_range) {
        fprintf(stderr, "Error: Debe especificar --scan-ports\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Configurar manejadores de señales
    setup_signal_handlers();
    
    // Mostrar banner e información
    print_banner();
    printf("Objetivo: %s\n", target);
    printf("Puertos: %s\n", port_range);
    printf("Modo: %s\n", continuous ? "Continuo" : "Único");
    if (continuous) {
        printf("Intervalo: %ds\n", interval);
    }
    printf("Timeout: %ds\n", timeout);
    printf("============================================================\n");
    
    // Inicializar componentes
    AlertManager *alert_manager = alert_manager_create();
    if (!alert_manager) {
        fprintf(stderr, "Error: No se pudo inicializar el gestor de alertas\n");
        free(port_range);
        return 1;
    }
    global_alert_manager = alert_manager;
    
    PortScanner *scanner = port_scanner_create(target, timeout, alert_manager);
    if (!scanner) {
        fprintf(stderr, "Error: No se pudo inicializar el escáner de puertos\n");
        alert_manager_destroy(alert_manager);
        free(port_range);
        return 1;
    }
    
    ReportGenerator *report_gen = report_generator_create(alert_manager);
    if (!report_gen) {
        fprintf(stderr, "Error: No se pudo inicializar el generador de reportes\n");
        port_scanner_destroy(scanner);
        alert_manager_destroy(alert_manager);
        free(port_range);
        return 1;
    }
    
    // Ejecutar escaneos
    int scan_count = 0;
    time_t start_time = time(NULL);
    
    do {
        scan_count++;
        
        if (continuous) {
            struct tm *tm_info = localtime(&start_time);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
            printf("\n--- Escaneo #%d - %s ---\n", scan_count, time_str);
        } else {
            printf("\n[INFO] Iniciando escaneo único...\n");
        }
        
        // Realizar escaneo
        int result = port_scanner_scan(scanner, port_range);
        if (result != 0) {
            fprintf(stderr, "Error durante el escaneo\n");
            break;
        }
        
        if (continuous && scan_count == 1) {
            printf("\n[INFO] Primer escaneo completado. Las siguientes alertas mostrarán solo cambios.\n");
        }
        
        // Esperar intervalo si es modo continuo
        if (continuous && keep_running) {
            for (int i = 0; i < interval && keep_running; i++) {
                sleep(1);
            }
        }
        
    } while (continuous && keep_running);
    
    // Mostrar resumen final
    printf("\n============================================================\n");
    printf("            RESUMEN FINAL\n");
    printf("============================================================\n");
    alert_manager_show_summary(alert_manager);
    
    // Exportar PDF si se solicita
    if (export_pdf) {
        printf("\n[INFO] Generando reporte PDF...\n");
        char pdf_path[512];
        if (report_generator_create_pdf(report_gen, target, port_range, pdf_path, sizeof(pdf_path)) == 0) {
            printf("[INFO] Reporte guardado en: %s\n", pdf_path);
        } else {
            printf("[ERROR] No se pudo generar el reporte PDF\n");
        }
    }
    
    // Limpieza
    printf("\n[INFO] Liberando alertas del sistema...\n");
    alert_manager_clear_alerts(alert_manager);
    
    report_generator_destroy(report_gen);
    port_scanner_destroy(scanner);
    alert_manager_destroy(alert_manager);
    free(port_range);
    
    printf("[INFO] MatcomGuard finalizado correctamente\n");
    return 0;
}
