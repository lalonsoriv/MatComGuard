/*
 * Report Generator - Generador de reportes PDF
 */

#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include "alert_manager.h"

typedef struct {
    AlertManager *alert_manager;
} ReportGenerator;

// Funciones públicas
ReportGenerator* report_generator_create(AlertManager *alert_manager);
void report_generator_destroy(ReportGenerator *generator);
int report_generator_create_pdf(ReportGenerator *generator, const char *target, 
                               const char *port_range, char *output_path, size_t path_size);

// Funciones auxiliares para generar HTML y convertir a PDF
int generate_html_report(ReportGenerator *generator, const char *target, 
                        const char *port_range, const char *html_path);
int convert_html_to_pdf(const char *html_path, const char *pdf_path);

#endif
