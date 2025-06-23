/**
 * Sistema Completo de Monitoreo de Dispositivos USB
 * 
 * Este programa monitorea dispositivos USB conectados, detecta cambios en sus archivos
 * y alerta cuando no hay dispositivos conectados.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <openssl/sha.h>
#include <mntent.h>

// ================= CONFIGURACIÓN =================
#define MAX_PATH_LEN 256
#define MAX_DEVNAME_LEN 64
#define MAX_USB_DEVICES 10
#define DEFAULT_SCAN_INTERVAL 5    // segundos
#define DEFAULT_CHANGE_THRESHOLD 10 // porcentaje
#define SOCKET_PATH "/tmp/usb_monitor_socket"

// ================= ESTRUCTURAS DE DATOS =================

// Estructura para información de archivos
typedef struct FileEntry {
    char path[MAX_PATH_LEN];       // Ruta completa del archivo
    uint8_t hash[32];              // Hash SHA-256 del contenido
    time_t last_modified;          // Fecha última modificación
    off_t size;                    // Tamaño en bytes
    mode_t permissions;            // Permisos del archivo
    struct FileEntry *next;        // Para la tabla hash
} FileEntry;

// Estructura para dispositivos USB
typedef struct USBDevice {
    char mount_point[MAX_PATH_LEN]; // Punto de montaje
    char dev_name[MAX_DEVNAME_LEN]; // Nombre del dispositivo
    FileEntry *file_snapshot;       // Snapshot actual de archivos
    pthread_mutex_t lock;           // Para acceso concurrente
    int is_scanning;                // Flag de estado
    struct USBDevice *next;         // Para la tabla hash
} USBDevice;

// ================= VARIABLES GLOBALES =================
static USBDevice *active_devices = NULL;
static int alert_socket = -1;

// ================= FUNCIONES AUXILIARES =================

/**
 * Calcula el hash SHA-256 de un archivo
 * @param path Ruta del archivo
 * @param hash Buffer para almacenar el hash (32 bytes)
 * @return 0 en éxito, -1 en error
 */
int calculate_file_hash(const char *path, uint8_t hash[32]) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    unsigned char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file))) {
        SHA256_Update(&sha256, buffer, bytes);
    }

    SHA256_Final(hash, &sha256);
    fclose(file);
    return 0;
}

/**
 * Envía una alerta/mensaje del sistema
 * @param message Mensaje a enviar
 */
void send_alert(const char *message) {
    // Filtrar mensajes no deseados
    if (strstr(message, "Open folder") || strstr(message, "tmpfs e~")) {
        return;
    }

    printf("[ALERTA] %s\n", message);
    fflush(stdout);

    if (alert_socket >= 0) {
        size_t len = strlen(message);
        write(alert_socket, message, len);
    }
}

// ================= DETECCIÓN DE DISPOSITIVOS =================

/**
 * Detecta dispositivos USB montados actualmente
 * @param devices Array para almacenar la información
 * @param max_count Capacidad del array
 * @return Número de dispositivos encontrados
 */
int detect_usb_devices(char devices[][MAX_PATH_LEN], char dev_names[][MAX_DEVNAME_LEN], int max_count) {
    FILE *mtab = setmntent("/etc/mtab", "r");
    if (!mtab) {
        perror("Error al abrir /etc/mtab");
        return 0;
    }

    int count = 0;
    struct mntent *ent;

    while ((ent = getmntent(mtab)) != NULL && count < max_count) {
        // Filtro más estricto para dispositivos USB
        if ((strstr(ent->mnt_dir, "/media/") || strstr(ent->mnt_dir, "/mnt/")) &&
            (strstr(ent->mnt_fsname, "/dev/sd") || strstr(ent->mnt_fsname, "/dev/mmc"))) {
            
            // Verificar que el punto de montaje existe y es accesible
            if (access(ent->mnt_dir, F_OK) == 0) {
                strncpy(devices[count], ent->mnt_dir, MAX_PATH_LEN);
                strncpy(dev_names[count], ent->mnt_fsname, MAX_DEVNAME_LEN);
                count++;
            }
        }
    }

    endmntent(mtab);
    return count;
}
/**
 * Busca un dispositivo en la lista activa por punto de montaje
 */
USBDevice *find_device(const char *mount_point) {
    USBDevice *dev;
    for (dev = active_devices; dev != NULL; dev = dev->next) {
        if (strcmp(dev->mount_point, mount_point) == 0) {
            return dev;
        }
    }
    return NULL;
}

/**
 * Agrega un nuevo dispositivo a la lista activa
 */
void add_device(const char *mount_point, const char *dev_name) {
    USBDevice *dev = malloc(sizeof(USBDevice));
    if (!dev) {
        send_alert("Error: Memoria insuficiente para nuevo dispositivo");
        return;
    }

    strncpy(dev->mount_point, mount_point, MAX_PATH_LEN);
    strncpy(dev->dev_name, dev_name, MAX_DEVNAME_LEN);
    dev->file_snapshot = NULL;
    pthread_mutex_init(&dev->lock, NULL);
    dev->is_scanning = 0;
    dev->next = active_devices;
    active_devices = dev;

    char alert_msg[MAX_PATH_LEN + MAX_DEVNAME_LEN + 50];
    snprintf(alert_msg, sizeof(alert_msg), 
         "[ALERTA] Nuevo dispositivo USB conectado: %s (%s)",
         dev->dev_name, dev->mount_point);
    send_alert(alert_msg);
}

/**
 * Elimina un dispositivo de la lista activa
 */
void remove_device(USBDevice *dev) {
    if (!dev) return;

    // Buscar el dispositivo en la lista
    USBDevice **pdev;
    for (pdev = &active_devices; *pdev != NULL; pdev = &(*pdev)->next) {
        if (*pdev == dev) {
            *pdev = dev->next; // Eliminar de la lista
            
            char alert_msg[MAX_PATH_LEN + 50];
            snprintf(alert_msg, sizeof(alert_msg),
                     "Dispositivo desconectado: %s",
                     dev->mount_point);
            send_alert(alert_msg);

            // Liberar recursos del dispositivo
            pthread_mutex_lock(&dev->lock);
            FileEntry *entry, *tmp_entry;
            for (entry = dev->file_snapshot; entry != NULL; entry = tmp_entry) {
                tmp_entry = entry->next;
                free(entry);
            }
            pthread_mutex_unlock(&dev->lock);
            
            pthread_mutex_destroy(&dev->lock);
            free(dev);
            return;
        }
    }
}

/**
 * Actualiza la lista de dispositivos activos
 * @return Número de dispositivos detectados
 */
int update_device_list() {
    char devices[MAX_USB_DEVICES][MAX_PATH_LEN];
    char dev_names[MAX_USB_DEVICES][MAX_DEVNAME_LEN];
    int count = detect_usb_devices(devices, dev_names, MAX_USB_DEVICES);

    // Marcar todos como no vistos
    USBDevice *dev;
    for (dev = active_devices; dev != NULL; dev = dev->next) {
        dev->is_scanning = -1;
    }

    // Procesar dispositivos detectados
    for (int i = 0; i < count; i++) {
        dev = find_device(devices[i]);
        if (dev) {
            // Dispositivo ya conocido
            dev->is_scanning = 0;
        } else {
            // Nuevo dispositivo
            add_device(devices[i], dev_names[i]);
        }
    }

    // Eliminar dispositivos desconectados
    USBDevice *tmp;
    dev = active_devices;
    while (dev != NULL) {
        tmp = dev->next;
        if (dev->is_scanning == -1) {
            remove_device(dev);
        }
        dev = tmp;
    }

    // Notificar si no hay dispositivos
    if (count == 0) {
        send_alert("Estado: No hay dispositivos USB conectados");
    }

    return count;
}

// ================= ESCANEO DE ARCHIVOS =================

/**
 * Escanea recursivamente un directorio y crea un snapshot de archivos
 * @param path Ruta a escanear
 * @return Lista de entradas de archivos
 */
FileEntry *scan_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return NULL;

    FileEntry *snapshot = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Saltar . y ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat stat_buf;
        if (lstat(full_path, &stat_buf) != 0)
            continue;

        if (S_ISDIR(stat_buf.st_mode)) {
            // Directorio - escanear recursivamente
            FileEntry *subdir = scan_directory(full_path);
            // Concatenar resultados
            if (subdir) {
                FileEntry *last = subdir;
                while (last->next != NULL) last = last->next;
                last->next = snapshot;
                snapshot = subdir;
            }
        } else if (S_ISREG(stat_buf.st_mode)) {
            // Archivo regular - agregar al snapshot
            FileEntry *file_entry = malloc(sizeof(FileEntry));
            if (!file_entry) continue;

            strncpy(file_entry->path, full_path, MAX_PATH_LEN);
            file_entry->last_modified = stat_buf.st_mtime;
            file_entry->size = stat_buf.st_size;
            file_entry->permissions = stat_buf.st_mode;
            file_entry->next = snapshot;

            // Calcular hash del archivo
            if (calculate_file_hash(full_path, file_entry->hash) != 0) {
                memset(file_entry->hash, 0, 32);
            }

            snapshot = file_entry;
        }
    }

    closedir(dir);
    return snapshot;
}

/**
 * Compara dos snapshots y reporta cambios
 */
void compare_snapshots(FileEntry *old, FileEntry *new, int threshold) {
    // Implementación simplificada - compara archivos y detecta cambios
    FileEntry *current;
    int changes = 0, total = 0;

    // Contar archivos en el snapshot antiguo
    for (current = old; current != NULL; current = current->next) {
        total++;
    }

    // Comparar con el nuevo snapshot
    for (current = new; current != NULL; current = current->next) {
        FileEntry *found = NULL;
        for (FileEntry *old_entry = old; old_entry != NULL; old_entry = old_entry->next) {
            if (strcmp(current->path, old_entry->path) == 0) {
                found = old_entry;
                break;
            }
        }

        if (found) {
            // Archivo existente - verificar cambios
            if (memcmp(current->hash, found->hash, 32) != 0) {
                char msg[MAX_PATH_LEN + 100];
                snprintf(msg, sizeof(msg), "Archivo modificado: %s", current->path);
                send_alert(msg);
                changes++;
            }
        } else {
            // Archivo nuevo
            char msg[MAX_PATH_LEN + 100];
            snprintf(msg, sizeof(msg), "Archivo nuevo detectado: %s", current->path);
            send_alert(msg);
            changes++;
        }
    }

    // Verificar archivos eliminados
    for (FileEntry *old_entry = old; old_entry != NULL; old_entry = old_entry->next) {
        int exists = 0;
        for (current = new; current != NULL; current = current->next) {
            if (strcmp(current->path, old_entry->path) == 0) {
                exists = 1;
                break;
            }
        }

        if (!exists) {
            char msg[MAX_PATH_LEN + 100];
            snprintf(msg, sizeof(msg), "Archivo eliminado: %s", old_entry->path);
            send_alert(msg);
            changes++;
        }
    }

    // Verificar si se supera el umbral de cambios
    if (total > 0 && (changes * 100 / total) >= threshold) {
        char msg[100];
        snprintf(msg, sizeof(msg), 
                "ALERTA: Umbral de cambios superado (%d%% de %d archivos)", 
                (changes * 100 / total), total);
        send_alert(msg);
    }
}

/**
 * Realiza el escaneo completo de un dispositivo
 */
void *perform_scan(void *arg) {
    USBDevice *dev = (USBDevice *)arg;
    pthread_mutex_lock(&dev->lock);

    // Crear nuevo snapshot
    FileEntry *new_snapshot = scan_directory(dev->mount_point);

    if (dev->file_snapshot) {
        // Comparar con el snapshot anterior
        compare_snapshots(dev->file_snapshot, new_snapshot, DEFAULT_CHANGE_THRESHOLD);

        // Liberar snapshot anterior
        FileEntry *entry, *tmp;
        for (entry = dev->file_snapshot; entry != NULL; entry = tmp) {
            tmp = entry->next;
            free(entry);
        }
    } else {
        // Primer escaneo del dispositivo
        char msg[MAX_PATH_LEN + 50];
        snprintf(msg, sizeof(msg),
                "[ALERTA] Baseline creado para dispositivo USB en %s",
                dev->mount_point);
        send_alert(msg);
    }

    // Actualizar snapshot
    dev->file_snapshot = new_snapshot;
    dev->is_scanning = 0;
    pthread_mutex_unlock(&dev->lock);
    return NULL;
}

/**
 * Inicia el escaneo de un dispositivo en un hilo separado
 */
void start_device_scan(USBDevice *dev) {
    if (dev->is_scanning) return;

    dev->is_scanning = 1;
    pthread_t thread;
    if (pthread_create(&thread, NULL, perform_scan, dev) != 0) {
        dev->is_scanning = 0;
        send_alert("Error: No se pudo crear hilo de escaneo");
    } else {
        pthread_detach(thread); // No necesitamos hacer join
    }
}

// ================= FUNCIONES PRINCIPALES =================

/**
 * Inicializa el sistema de alertas (socket)
 */
void init_alert_system() {
    alert_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (alert_socket < 0) {
        perror("Advertencia: No se pudo crear socket de alertas");
        alert_socket = -1;
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(alert_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        // Cambiar esto de error a advertencia
        printf("Advertencia: No se pudo conectar al socket de alertas. Usando salida estándar\n");
        close(alert_socket);
        alert_socket = -1;
    }
}

/**
 * Limpia recursos del sistema
 */
void cleanup_system() {
    // Cerrar socket de alertas
    if (alert_socket >= 0) {
        close(alert_socket);
    }

    // Liberar dispositivos
    USBDevice *dev, *tmp;
    for (dev = active_devices; dev != NULL; dev = tmp) {
        tmp = dev->next;
        remove_device(dev);
    }
}

/**
 * Función principal de monitoreo
 */
void run_monitoring(int interval) {
    printf("Iniciando sistema de monitoreo USB...\n");
    printf("Intervalo de escaneo: %d segundos\n", interval);

    while (1) {
        // Actualizar lista de dispositivos
        update_device_list();

        // Programar escaneos para dispositivos activos
        USBDevice *dev;
        for (dev = active_devices; dev != NULL; dev = dev->next) {
            if (!dev->is_scanning) {
                start_device_scan(dev);
            }
        }

        sleep(interval);
    }
}

// ================= PUNTO DE ENTRADA =================

int main(int argc, char *argv[]) {
    // Configuración
    int scan_interval = DEFAULT_SCAN_INTERVAL;

    // Parsear argumentos (opcional)
    if (argc >= 2) {
        scan_interval = atoi(argv[1]);
        if (scan_interval < 1 || scan_interval > 3600) {
            fprintf(stderr, "Intervalo inválido. Usar valor entre 1-3600 segundos\n");
            return 1;
        }
    }

    // Configurar manejo de terminación
    atexit(cleanup_system);

    // Inicializar subsistemas
    init_alert_system();

    // Iniciar monitoreo
    run_monitoring(scan_interval);

    return 0;
}