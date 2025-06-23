/*
 * Report Generator - Implementación del generador de reportes PDF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "report_generator.h"

ReportGenerator* report_generator_create(AlertManager *alert_manager) {
    if (!alert_manager) return NULL;
    
    ReportGenerator *generator = malloc(sizeof(ReportGenerator));
    if (!generator) return NULL;
    
    generator->alert_manager = alert_manager;
    return generator;
}

void report_generator_destroy(ReportGenerator *generator) {
    if (generator) {
        free(generator);
    }
}

int generate_html_report(ReportGenerator *generator, const char *target, 
                        const char *port_range, const char *html_path) {
    if (!generator || !target || !port_range || !html_path) return -1;
    
    FILE *file = fopen(html_path, "w");
    if (!file) return -1;
    
    time_t now = time(NULL);
    char time_str[64];
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Escribir HTML
    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html lang=\"es\">\n");
    fprintf(file, "<head>\n");
    fprintf(file, "    <meta charset=\"UTF-8\">\n");
    fprintf(file, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(file, "    <title>MatcomGuard - Reporte de Seguridad</title>\n");
    fprintf(file, "    <style>\n");
    fprintf(file, "        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }\n");
    fprintf(file, "        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n");
    fprintf(file, "        .header { text-align: center; border-bottom: 3px solid #2c3e50; padding-bottom: 20px; margin-bottom: 30px; }\n");
    fprintf(file, "        .header h1 { color: #2c3e50; margin: 0; }\n");
    fprintf(file, "        .header h2 { color: #7f8c8d; margin: 5px 0; }\n");
    fprintf(file, "        .info-section { background: #ecf0f1; padding: 15px; border-radius: 5px; margin-bottom: 20px; }\n");
    fprintf(file, "        .info-section h3 { margin-top: 0; color: #2c3e50; }\n");
    fprintf(file, "        .summary { display: flex; justify-content: space-around; margin-bottom: 30px; }\n");
    fprintf(file, "        .summary-item { text-align: center; padding: 15px; border-radius: 5px; }\n");
    fprintf(file, "        .summary-total { background: #3498db; color: white; }\n");
    fprintf(file, "        .summary-high { background: #e74c3c; color: white; }\n");
    fprintf(file, "        .summary-medium { background: #f39c12; color: white; }\n");
    fprintf(file, "        .summary-low { background: #27ae60; color: white; }\n");
    fprintf(file, "        .alert-section { margin-bottom: 25px; }\n");
    fprintf(file, "        .alert-high { border-left: 5px solid #e74c3c; background: #fdf2f2; }\n");
    fprintf(file, "        .alert-medium { border-left: 5px solid #f39c12; background: #fef9e7; }\n");
    fprintf(file, "        .alert-low { border-left: 5px solid #27ae60; background: #eafaf1; }\n");
    fprintf(file, "        .alert-item { padding: 15px; margin-bottom: 10px; border-radius: 5px; }\n");
    fprintf(file, "        .alert-header { font-weight: bold; margin-bottom: 5px; }\n");
    fprintf(file, "        .alert-details { font-size: 0.9em; color: #666; }\n");
    fprintf(file, "        .footer { text-align: center; margin-top: 30px; padding-top: 20px; border-top: 1px solid #bdc3c7; color: #7f8c8d; }\n");
    fprintf(file, "    </style>\n");
    fprintf(file, "</head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "    <div class=\"container\">\n");
    fprintf(file, "        <div class=\"header\">\n");
    fprintf(file, "            <h1>🛡️ MATCOMGUARD</h1>\n");
    fprintf(file, "            <h2>Reporte de Seguridad - Escaneo de Puertos</h2>\n");
    fprintf(file, "        </div>\n");
    
    // Información del escaneo
    fprintf(file, "        <div class=\"info-section\">\n");
    fprintf(file, "            <h3>📋 Información del Escaneo</h3>\n");
    fprintf(file, "            <p><strong>Objetivo:</strong> %s</p>\n", target);
    fprintf(file, "            <p><strong>Rango de puertos:</strong> %s</p>\n", port_range);
    fprintf(file, "            <p><strong>Fecha y hora:</strong> %s</p>\n", time_str);
    fprintf(file, "        </div>\n");
    
    // Resumen de alertas
    AlertManager *manager = generator->alert_manager;
    fprintf(file, "        <div class=\"summary\">\n");
    fprintf(file, "            <div class=\"summary-item summary-total\">\n");
    fprintf(file, "                <h3>%d</h3>\n", manager->total_alerts);
    fprintf(file, "                <p>Total Alertas</p>\n");
    fprintf(file, "            </div>\n");
    fprintf(file, "            <div class=\"summary-item summary-high\">\n");
    fprintf(file, "                <h3>%d</h3>\n", manager->high_alerts);
    fprintf(file, "                <p>Críticas</p>\n");
    fprintf(file, "            </div>\n");
    fprintf(file, "            <div class=\"summary-item summary-medium\">\n");
    fprintf(file, "                <h3>%d</h3>\n", manager->medium_alerts);
    fprintf(file, "                <p>Medias</p>\n");
    fprintf(file, "            </div>\n");
    fprintf(file, "            <div class=\"summary-item summary-low\">\n");
    fprintf(file, "                <h3>%d</h3>\n", manager->low_alerts);
    fprintf(file, "                <p>Bajas</p>\n");
    fprintf(file, "            </div>\n");
    fprintf(file, "        </div>\n");
    
    // Detalle de alertas
    if (manager->total_alerts > 0) {
        const AlertLevel priorities[] = {ALERT_HIGH, ALERT_MEDIUM, ALERT_LOW};
        const char* priority_names[] = {"🔴 Alertas Críticas", "🟡 Alertas Medias", "🟢 Alertas Bajas"};
        const char* css_classes[] = {"alert-high", "alert-medium", "alert-low"};
        
        for (int p = 0; p < 3; p++) {
            AlertLevel priority = priorities[p];
            int found_any = 0;
            
            // Contar alertas de esta prioridad
            AlertNode *current = manager->head;
            while (current) {
                if (current->alert.level == priority) {
                    if (!found_any) {
                        fprintf(file, "        <div class=\"alert-section\">\n");
                        fprintf(file, "            <h3>%s</h3>\n", priority_names[p]);
                        found_any = 1;
                    }
                    
                    char alert_time_str[64];
                    struct tm *alert_tm_info = localtime(&current->alert.timestamp);
                    strftime(alert_time_str, sizeof(alert_time_str), "%H:%M:%S", alert_tm_info);
                    
                    fprintf(file, "            <div class=\"alert-item %s\">\n", css_classes[p]);
                    fprintf(file, "                <div class=\"alert-header\">%s</div>\n", current->alert.message);
                    fprintf(file, "                <div class=\"alert-details\">\n");
                    fprintf(file, "                    <strong>Puerto:</strong> %d | ", current->alert.port);
                    fprintf(file, "                    <strong>Servicio:</strong> %s | ", current->alert.service);
                    fprintf(file, "                    <strong>Hora:</strong> %s\n", alert_time_str);
                    fprintf(file, "                </div>\n");
                    fprintf(file, "            </div>\n");
                }
                current = current->next;
            }
            
            if (found_any) {
                fprintf(file, "        </div>\n");
            }
        }
    } else {
        fprintf(file, "        <div class=\"alert-section\">\n");
        fprintf(file, "            <h3>✅ Sin alertas de seguridad</h3>\n");
        fprintf(file, "            <p>No se detectaron puertos sospechosos durante el escaneo.</p>\n");
        fprintf(file, "        </div>\n");
    }
    
    // Footer
    fprintf(file, "        <div class=\"footer\">\n");
    fprintf(file, "            <p>Generado por MatcomGuard v1.0.0</p>\n");
    fprintf(file, "            <p>Sistema de Monitoreo de Seguridad</p>\n");
    fprintf(file, "        </div>\n");
    fprintf(file, "    </div>\n");
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");
    
    fclose(file);
    return 0;
}

int convert_html_to_pdf(const char *html_path, const char *pdf_path) {
    // Intentar diferentes herramientas para convertir HTML a PDF
    char command[1024];
    
    // Opción 1: wkhtmltopdf (más común en sistemas Unix)
    snprintf(command, sizeof(command), 
             "wkhtmltopdf --page-size A4 --margin-top 20mm --margin-bottom 20mm "
             "--margin-left 15mm --margin-right 15mm '%s' '%s' 2>/dev/null", 
             html_path, pdf_path);
    
    if (system(command) == 0) {
        return 0;
    }
    
    // Opción 2: weasyprint
    snprintf(command, sizeof(command), "weasyprint '%s' '%s' 2>/dev/null", html_path, pdf_path);
    if (system(command) == 0) {
        return 0;
    }
    
    // Opción 3: chromium/chrome headless
    snprintf(command, sizeof(command), 
             "chromium-browser --headless --disable-gpu --print-to-pdf='%s' '%s' 2>/dev/null || "
             "google-chrome --headless --disable-gpu --print-to-pdf='%s' '%s' 2>/dev/null",
             pdf_path, html_path, pdf_path, html_path);
    
    if (system(command) == 0) {
        return 0;
    }
    
    // Si no se puede convertir a PDF, al menos exportar el HTML
    printf("[ADVERTENCIA] No se pudo convertir a PDF. Reporte HTML disponible en: %s\n", html_path);
    return -1;
}

int report_generator_create_pdf(ReportGenerator *generator, const char *target, 
                               const char *port_range, char *output_path, size_t path_size) {
    if (!generator || !target || !port_range || !output_path) return -1;
    
    // Crear nombres de archivos únicos
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
    
    char html_path[512];
    char pdf_path[512];
    snprintf(html_path, sizeof(html_path), "/tmp/matcomguard_report_%s.html", timestamp);
    snprintf(pdf_path, sizeof(pdf_path), "./matcomguard_report_%s.pdf", timestamp);
    
    // Generar reporte HTML
    if (generate_html_report(generator, target, port_range, html_path) != 0) {
        return -1;
    }
    
    // Intentar convertir a PDF
    if (convert_html_to_pdf(html_path, pdf_path) == 0) {
        // PDF generado exitosamente
        strncpy(output_path, pdf_path, path_size - 1);
        output_path[path_size - 1] = '\0';
        
        // Limpiar archivo HTML temporal
        unlink(html_path);
        return 0;
    } else {
        // No se pudo generar PDF, mantener HTML
        char final_html_path[512];
        snprintf(final_html_path, sizeof(final_html_path), "./matcomguard_report_%s.html", timestamp);
        rename(html_path, final_html_path);
        
        strncpy(output_path, final_html_path, path_size - 1);
        output_path[path_size - 1] = '\0';
        return -1;
    }
}
