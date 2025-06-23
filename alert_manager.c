/*
 * Alert Manager - Implementación del gestor de alertas
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "alert_manager.h"

const char* alert_level_to_string(AlertLevel level) {
    switch (level) {
        case ALERT_LOW: return "BAJA";
        case ALERT_MEDIUM: return "MEDIA";
        case ALERT_HIGH: return "ALTA";
        default: return "DESCONOCIDA";
    }
}

AlertManager* alert_manager_create() {
    AlertManager *manager = malloc(sizeof(AlertManager));
    if (!manager) return NULL;
    
    manager->head = NULL;
    manager->total_alerts = 0;
    manager->high_alerts = 0;
    manager->medium_alerts = 0;
    manager->low_alerts = 0;
    
    return manager;
}

void alert_manager_destroy(AlertManager *manager) {
    if (!manager) return;
    
    alert_manager_clear_alerts(manager);
    free(manager);
}

int alert_manager_add_alert(AlertManager *manager, Alert *alert) {
    if (!manager || !alert) return -1;
    
    AlertNode *new_node = malloc(sizeof(AlertNode));
    if (!new_node) return -1;
    
    // Copiar la alerta
    new_node->alert = *alert;
    new_node->next = manager->head;
    manager->head = new_node;
    
    // Actualizar contadores
    manager->total_alerts++;
    switch (alert->level) {
        case ALERT_HIGH:
            manager->high_alerts++;
            break;
        case ALERT_MEDIUM:
            manager->medium_alerts++;
            break;
        case ALERT_LOW:
            manager->low_alerts++;
            break;
    }
    
    return 0;
}

void print_alert(Alert *alert) {
    char time_str[64];
    struct tm *tm_info = localtime(&alert->timestamp);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    const char *level_str = alert_level_to_string(alert->level);
    printf("  [%s] %s - Puerto: %d, Servicio: %s - %s\n", 
           level_str, alert->message, alert->port, alert->service, time_str);
}

void alert_manager_show_summary(AlertManager *manager) {
    if (!manager) return;
    
    printf("Total de alertas: %d\n", manager->total_alerts);
    printf("  - Alertas ALTAS: %d\n", manager->high_alerts);
    printf("  - Alertas MEDIAS: %d\n", manager->medium_alerts);
    printf("  - Alertas BAJAS: %d\n", manager->low_alerts);
    
    if (manager->total_alerts > 0) {
        printf("\nDetalle de alertas:\n");
        
        // Mostrar alertas de alta prioridad primero
        AlertNode *current = manager->head;
        while (current) {
            if (current->alert.level == ALERT_HIGH) {
                print_alert(&current->alert);
            }
            current = current->next;
        }
        
        // Luego alertas de prioridad media
        current = manager->head;
        while (current) {
            if (current->alert.level == ALERT_MEDIUM) {
                print_alert(&current->alert);
            }
            current = current->next;
        }
        
        // Finalmente alertas de prioridad baja
        current = manager->head;
        while (current) {
            if (current->alert.level == ALERT_LOW) {
                print_alert(&current->alert);
            }
            current = current->next;
        }
    }
}

void alert_manager_clear_alerts(AlertManager *manager) {
    if (!manager) return;
    
    AlertNode *current = manager->head;
    while (current) {
        AlertNode *next = current->next;
        free(current);
        current = next;
    }
    
    manager->head = NULL;
    manager->total_alerts = 0;
    manager->high_alerts = 0;
    manager->medium_alerts = 0;
    manager->low_alerts = 0;
}

int alert_manager_export_to_file(AlertManager *manager, const char *filename) {
    if (!manager || !filename) return -1;
    
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    
    fprintf(file, "MATCOMGUARD - REPORTE DE ALERTAS\n");
    fprintf(file, "================================\n\n");
    
    time_t now = time(NULL);
    char time_str[64];
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(file, "Fecha de generación: %s\n\n", time_str);
    
    fprintf(file, "RESUMEN:\n");
    fprintf(file, "Total de alertas: %d\n", manager->total_alerts);
    fprintf(file, "  - Alertas ALTAS: %d\n", manager->high_alerts);
    fprintf(file, "  - Alertas MEDIAS: %d\n", manager->medium_alerts);
    fprintf(file, "  - Alertas BAJAS: %d\n\n", manager->low_alerts);
    
    if (manager->total_alerts > 0) {
        fprintf(file, "DETALLE DE ALERTAS:\n");
        fprintf(file, "==================\n\n");
        
        // Exportar alertas ordenadas por prioridad
        const AlertLevel priorities[] = {ALERT_HIGH, ALERT_MEDIUM, ALERT_LOW};
        const char* priority_names[] = {"ALTA PRIORIDAD", "MEDIA PRIORIDAD", "BAJA PRIORIDAD"};
        
        for (int p = 0; p < 3; p++) {
            AlertLevel priority = priorities[p];
            int found_any = 0;
            
            AlertNode *current = manager->head;
            while (current) {
                if (current->alert.level == priority) {
                    if (!found_any) {
                        fprintf(file, "%s:\n", priority_names[p]);
                        fprintf(file, "%s\n", (p == 0) ? "===============" : 
                                            (p == 1) ? "=================" : "================");
                        found_any = 1;
                    }
                    
                    char alert_time_str[64];
                    struct tm *alert_tm_info = localtime(&current->alert.timestamp);
                    strftime(alert_time_str, sizeof(alert_time_str), "%Y-%m-%d %H:%M:%S", alert_tm_info);
                    
                    fprintf(file, "• %s\n", current->alert.message);
                    fprintf(file, "  Puerto: %d | Servicio: %s | Hora: %s\n\n", 
                           current->alert.port, current->alert.service, alert_time_str);
                }
                current = current->next;
            }
            
            if (found_any) {
                fprintf(file, "\n");
            }
        }
    }
    
    fclose(file);
    return 0;
}

AlertNode* alert_manager_get_alerts(AlertManager *manager) {
    return manager ? manager->head : NULL;
}
