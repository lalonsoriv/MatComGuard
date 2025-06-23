/*
 * Alert Manager - Gestor de alertas para el sistema de monitoreo
 */

#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <time.h>

typedef enum {
    ALERT_LOW,
    ALERT_MEDIUM,
    ALERT_HIGH
} AlertLevel;

typedef struct {
    AlertLevel level;
    char message[512];
    int port;
    char service[64];
    time_t timestamp;
} Alert;

typedef struct AlertNode {
    Alert alert;
    struct AlertNode *next;
} AlertNode;

typedef struct {
    AlertNode *head;
    int total_alerts;
    int high_alerts;
    int medium_alerts;
    int low_alerts;
} AlertManager;

// Funciones públicas
AlertManager* alert_manager_create();
void alert_manager_destroy(AlertManager *manager);
int alert_manager_add_alert(AlertManager *manager, Alert *alert);
void alert_manager_show_summary(AlertManager *manager);
void alert_manager_clear_alerts(AlertManager *manager);
int alert_manager_export_to_file(AlertManager *manager, const char *filename);
AlertNode* alert_manager_get_alerts(AlertManager *manager);

// Funciones auxiliares
const char* alert_level_to_string(AlertLevel level);
void print_alert(Alert *alert);

#endif
