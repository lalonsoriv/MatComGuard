# MatcomGuard - Sistema de Monitoreo de Seguridad

**MatcomGuard** es un escáner de puertos en tiempo real desarrollado en C para sistemas Unix-like, diseñado para detectar puertos abiertos y identificar posibles amenazas de seguridad.

## 🚀 Características

- **Escaneo TCP en tiempo real**: Detecta puertos abiertos usando conexiones TCP
- **Detección de servicios**: Identifica servicios comunes asociados a puertos específicos
- **Alertas de seguridad**: Clasifica puertos como normales, sospechosos o potencialmente comprometidos
- **Monitoreo continuo**: Opción de escaneo periódico con detección de cambios
- **Generación de reportes**: Exporta alertas a PDF/HTML
- **Gestión de alertas**: Sistema completo de manejo y clasificación de alertas

## 📋 Requisitos

### Dependencias del sistema:
- **Compilador GCC**
- **pthread** (para multithreading)
- **Herramientas opcionales para PDF** (una de las siguientes):
  - `wkhtmltopdf` (recomendado)
  - `weasyprint`
  - `chromium-browser` o `google-chrome`

### Instalación de dependencias:

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc build-essential wkhtmltopdf
```

**CentOS/RHEL/Fedora:**
```bash
sudo yum install gcc wkhtmltopdf
# o para Fedora:
sudo dnf install gcc wkhtmltopdf
```

**macOS:**
```bash
brew install gcc wkhtmltopdf
```

## 🔧 Compilación

```bash
# Compilar el proyecto completo
gcc -o matcomguard matcomguard.c port_scanner.c alert_manager.c report_generator.c -lpthread

# O usar el Makefile (si está disponible)
make
```

## 💻 Uso

### Sintaxis básica:
```bash
./matcomguard --scan-ports RANGO [OPCIONES]
```

### Opciones disponibles:
- `--scan-ports RANGO`: Rango de puertos a escanear (requerido)
- `--target IP`: IP objetivo (por defecto: 127.0.0.1)
- `--continuous`: Monitoreo continuo en tiempo real
- `--interval SEGUNDOS`: Intervalo entre escaneos (por defecto: 30)
- `--timeout SEGUNDOS`: Timeout para conexiones TCP (por defecto: 3)
- `--export-pdf`: Exportar alertas a PDF al finalizar
- `--help`: Mostrar ayuda
- `--version`: Mostrar versión

### Ejemplos de uso:

**Escaneo básico:**
```bash
./matcomguard --scan-ports 1-1024
```

**Escaneo de IP remota:**
```bash
./matcomguard --scan-ports 1-65535 --target 192.168.1.1
```

**Monitoreo continuo:**
```bash
./matcomguard --scan-ports 80,443,22,21 --continuous --interval 60
```

**Escaneo con reporte PDF:**
```bash
./matcomguard --scan-ports 1-1024 --export-pdf
```

**Escaneo avanzado:**
```bash
./matcomguard --scan-ports 1-65535 --target 10.0.0.1 --continuous --interval 30 --timeout 5 --export-pdf
```

## 🔍 Tipos de Alertas

### 🔴 **Alertas Críticas (ALTA)**
Puertos conocidos por ser utilizados por malware o backdoors:
- Puerto 31337 (Backdoor común)
- Puerto 12345 (NetBus)
- Puerto 54321 (Back Orifice)
- Puerto 4444 (Metasploit)
- Y otros puertos sospechosos...

### 🟡 **Alertas Medias (MEDIA)**
- Puertos altos (>1024) sin servicio conocido
- Puertos bajos sin servicio común asociado

### 🟢 **Alertas Bajas (BAJA)**
- Puertos con servicios comunes y esperados (SSH, HTTP, HTTPS, etc.)

## 📊 Ejemplo de Salida

```
============================================================
        MATCOMGUARD - ESCÁNER DE PUERTOS v1.0.0
============================================================
Objetivo: 127.0.0.1
Puertos: 1-1024
Modo: Único
Timeout: 3s
============================================================

[INFO] Escaneando 1024 puertos en 127.0.0.1...
[INFO] Progreso: 1024/1024 puertos escaneados

[RESULTADO] 3 puertos abiertos encontrados:
🟢 [OK] Puerto 22/tcp (SSH) abierto
🟢 [OK] Puerto 80/tcp (HTTP) abierto
🔴 [ALERTA] Puerto 31337/tcp abierto (Backdoor común)

============================================================
            RESUMEN FINAL
============================================================
Total de alertas: 1
  - Alertas ALTAS: 1
  - Alertas MEDIAS: 0
  - Alertas BAJAS: 0

Detalle de alertas:
  [ALTA] [ALERTA] Puerto 31337/tcp abierto (Backdoor común) - Puerto: 31337, Servicio: Backdoor común - 2025-06-22 15:30:45
```

## 🛡️ Servicios Detectados

El sistema reconoce automáticamente los siguientes servicios:
- **FTP** (21), **SSH** (22), **Telnet** (23)
- **SMTP** (25), **DNS** (53), **HTTP** (80)
- **POP3** (110), **IMAP** (143), **HTTPS** (443)
- **SMB** (445), **RDP** (3389)
- **MySQL** (3306), **PostgreSQL** (5432)
- **Redis** (6379), **MongoDB** (27017)
- Y muchos más...

## 📄 Generación de Reportes

Cuando se usa la opción `--export-pdf`, MatcomGuard genera un reporte detallado que incluye:
- Información del escaneo (objetivo, rango, fecha/hora)
- Resumen estadístico de alertas
- Detalle completo de todas las alertas clasificadas por prioridad
- Formato profesional con código de colores

## 🔧 Personalización

### Agregar nuevos servicios:
Edita el array `common_services` en `port_scanner.c`:
```c
static ServiceMapping common_services[] = {
    {8080, "HTTP Alternativo"},
    {9200, "Elasticsearch"},
    // Agregar nuevos servicios aquí
    {0, NULL} // Manténer el terminador
};
```

### Agregar puertos sospechosos:
Edita el array `suspicious_ports` en `port_scanner.c`:
```c
static SuspiciousPort suspicious_ports[] = {
    {1337, "Posible backdoor"},
    {9999, "Trojan común"},
    // Agregar nuevos puertos sospechosos aquí
    {0, NULL} // Mantener el terminador
};
```

