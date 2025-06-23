#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/file.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_PROCESSES 512
#define SAMPLE_INTERVAL 1
#define DEFAULT_CPU_THRESHOLD 70.0
#define DEFAULT_RAM_THRESHOLD 50.0
#define MIN_SECONDS_FOR_ALERT 2
#define CONFIG_FILE "monitor_config.conf"
#define DEFAULT_SAMPLE_INTERVAL 1
#define PID_FILE "/tmp/process_monitor.pid"
volatile sig_atomic_t running = 1;

typedef struct
{
    int pid;
    float last_total_time;
    struct timespec last_timestamp;
    time_t first_exceed_time_cpu;
    time_t first_exceed_time_ram;
    int seen_this_cycle; // <-- CAMBIO: Flag para detectar procesos terminados
} ProcessHistoryTime;

static ProcessHistoryTime process_dict[MAX_PROCESSES];
int process_count = 0;

typedef struct
{
    float cpu_threshold;
    float ram_threshold;
    int min_seconds_for_alert;
    int sample_interval;
} MonitorConfig;

typedef struct
{
    int pid;
    char name[256];
    float cpu_usage;
    float memory_usage_percent;
    unsigned long memory_kb;
    time_t first_exceed_time_cpu;
    time_t first_exceed_time_ram;
} ProcessInfo;

typedef struct
{
    unsigned long total_memory_kb;
    unsigned long free_memory_kb;
} SystemMemory;

typedef struct
{
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
} SystemCPUStats;

ProcessHistoryTime *get_process_history(int pid, ProcessInfo *process)
{
    for (int i = 0; i < process_count; i++)
    {
        if (process_dict[i].pid == pid)
        {
            process_dict[i].seen_this_cycle = 1; // <-- CAMBIO: Marcar como visto
            return &process_dict[i];
        }
    }

    if (process_count < MAX_PROCESSES)
    {
        ProcessHistoryTime *process_history = &process_dict[process_count++];
        process_history->pid = pid;
        process_history->first_exceed_time_cpu = process->first_exceed_time_cpu;
        process_history->first_exceed_time_ram = process->first_exceed_time_ram;
        process_history->last_total_time = 0;
        process_history->seen_this_cycle = 1; // <-- CAMBIO: Marcar como visto al crear
        return process_history;
    }

    return NULL;
}

MonitorConfig load_configuration()
{
    MonitorConfig config;
    config.cpu_threshold = DEFAULT_CPU_THRESHOLD;
    config.ram_threshold = DEFAULT_RAM_THRESHOLD;
    config.sample_interval = SAMPLE_INTERVAL;
    config.min_seconds_for_alert = MIN_SECONDS_FOR_ALERT;

    FILE *file = fopen("./monitor_config.conf", "r");
    if (file == NULL)
    {
        return config;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0 || line[0] == '#')
        {
            continue;
        }

        char key[MAX_BUFFER_SIZE];
        char value[MAX_BUFFER_SIZE];

        if (sscanf(line, "%s = %s", key, value) == 2)
        {
            if (strcmp(key, "CPU_THRESHOLD") == 0)
            {
                config.cpu_threshold = atof(value);
            }
            else if (strcmp(key, "RAM_THRESHOLD") == 0)
            {
                config.ram_threshold = atof(value);
            }
            else if (strcmp(key, "SAMPLE_INTERVAL") == 0)
            {
                config.sample_interval = atoi(value);
            }
            else if (strcmp(key, "MIN_SECONDS_FOR_ALERT") == 0)
            {
                config.min_seconds_for_alert = atoi(value);
            }
        }
        else
        {
            fprintf(stderr, "Advertencia: Línea de configuración inválida en '%s': %s\n", CONFIG_FILE, line);
        }
    }

    fclose(file);
    printf("Configuración cargada desde '%s'.\n", CONFIG_FILE);
    return config;
}

void get_process_name(int pid, char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE *file = fopen(path, "r");
    if (!file)
    {
        strcpy(name, "[desconocido]");
        return;
    }

    if (fgets(name, 256, file))
    {
        name[strcspn(name, "\n")] = '\0';
    }
    else
    {
        strcpy(name, "[desconocido]");
    }

    fclose(file);
}

unsigned long get_process_memory(int pid)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");
    if (!file)
        return 0;

    char line[MAX_BUFFER_SIZE];
    unsigned long memory = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            sscanf(line + 6, "%lu", &memory);
            break;
        }
    }

    fclose(file);
    return memory;
}

SystemMemory get_system_memory()
{
    SystemMemory mem = {0, 0};
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file)
        return mem;

    char line[MAX_BUFFER_SIZE];

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "MemTotal:", 9) == 0)
        {
            sscanf(line + 9, "%lu", &mem.total_memory_kb);
        }
        else if (strncmp(line, "MemFree:", 8) == 0)
        {
            sscanf(line + 8, "%lu", &mem.free_memory_kb);
        }
    }

    fclose(file);
    return mem;
}

SystemCPUStats get_system_cpu_stats()
{
    SystemCPUStats stats = {0};
    FILE *file = fopen("/proc/stat", "r");
    if (!file)
        return stats;

    char line[MAX_BUFFER_SIZE];
    if (fgets(line, sizeof(line), file))
    {
        sscanf(line + 5, "%lu %lu %lu %lu %lu %lu %lu",
               &stats.user, &stats.nice, &stats.system, &stats.idle,
               &stats.iowait, &stats.irq, &stats.softirq);
    }

    fclose(file);
    return stats;
}

float calculate_system_cpu_usage(const SystemCPUStats *prev, const SystemCPUStats *current)
{
    unsigned long prev_total = prev->user + prev->nice + prev->system +
                               prev->idle + prev->iowait + prev->irq + prev->softirq;
    unsigned long current_total = current->user + current->nice + current->system +
                                  current->idle + current->iowait + current->irq + current->softirq;

    unsigned long total_diff = current_total - prev_total;
    if (total_diff == 0)
        return 0.0;

    unsigned long work_diff = (current_total - current->idle) - (prev_total - prev->idle);

    return 100.0 * work_diff / (float)total_diff;
}

void calculate_process_cpu_usage(int pid, ProcessInfo *process)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");
    if (!file)
        return;

    unsigned long utime = 0, stime = 0, cutime = 0, cstime = 0;
    char line[MAX_BUFFER_SIZE];

    if (fgets(line, sizeof(line), file))
    {
        char *token = strtok(line, " ");
        for (int field = 1; token != NULL && field <= 22; field++)
        {
            switch (field)
            {
            case 14:
                utime = strtoul(token, NULL, 10);
                break;
            case 15:
                stime = strtoul(token, NULL, 10);
                break;
            case 16:
                cutime = strtoul(token, NULL, 10);
                break;
            case 17:
                cstime = strtoul(token, NULL, 10);
                break;
            }
            token = strtok(NULL, " ");
        }
    }
    fclose(file);

    unsigned long total_time = utime + stime + cutime + cstime;
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    ProcessHistoryTime *history = get_process_history(pid, process);
    if (!history)
        return;

    if (history->last_total_time > 0)
    {
        unsigned long time_diff = total_time - history->last_total_time;
        double elapsed_seconds = (current_time.tv_sec - history->last_timestamp.tv_sec) +
                                 (current_time.tv_nsec - history->last_timestamp.tv_nsec) / 1e9;

        if (elapsed_seconds > 0)
        {
            process->cpu_usage = (time_diff / (float)sysconf(_SC_CLK_TCK)) / elapsed_seconds * 100;
        }
    }

    history->last_total_time = total_time;
    history->last_timestamp = current_time;
}

void print_system_stats(const SystemMemory *mem, float cpu_percent, int process_count)
{
    printf("\nEstado del sistema:\n");
    printf("----------------------------------------\n");
    printf("CPU: %5.1f%% | Memoria Total: %6.2f MB | Procesos Activos: %d\n",
           cpu_percent, mem->total_memory_kb / 1024.0, process_count);
    printf("Memoria Libre: %6.2f MB (%5.1f%% usado)\n",
           mem->free_memory_kb / 1024.0,
           100.0 - (mem->free_memory_kb * 100.0 / mem->total_memory_kb));
    printf("----------------------------------------\n\n");
}

void print_process_table(const ProcessInfo *processes, int count)
{
    printf("%-8s %-25s %-8s %-12s %-10s\n",
           "PID", "Nombre", "CPU%", "Memoria%", "Memoria (MB)");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < count; i++)
    {
        printf("%-8d %-25s %-8.1f %-12.2f %-10.2f\n",
               processes[i].pid,
               processes[i].name,
               processes[i].cpu_usage,
               processes[i].memory_usage_percent,
               processes[i].memory_kb / 1024.0);
    }
}

void check_for_anomalies(ProcessInfo *process, MonitorConfig config) {
    time_t now = time(NULL);
    ProcessHistoryTime *history = get_process_history(process->pid, process);
    if (!history) return;

    // Abrir archivo de alertas con manejo de errores robusto
    FILE *alert_file = fopen("/tmp/process_monitor_cpu_alerts.log", "a");
    if (!alert_file) {
        // Intentar recrear el archivo si falla la apertura
        alert_file = fopen("/tmp/process_monitor_cpu_alerts.log", "w");
        if (!alert_file) {
            return;  // Si sigue fallando, salir
        }
        fclose(alert_file);
        alert_file = fopen("/tmp/process_monitor_cpu_alerts.log", "a");
        if (!alert_file) return;
    }

    // Abrir FIFO para alertas en tiempo real (solo escritura, no bloqueante)
    FILE *fifo = NULL;
    int fifo_fd = open("/tmp/process_monitor_alert_pipe", O_WRONLY | O_NONBLOCK);
    if (fifo_fd != -1) {
        fifo = fdopen(fifo_fd, "w");
    }

    // Buffer para mensajes de alerta
    char alert_msg[512];
    
    // --- Alerta de CPU ---
    if (process->cpu_usage > config.cpu_threshold) {
        if (history->first_exceed_time_cpu == 0) {
            history->first_exceed_time_cpu = now;
        } else if (now - history->first_exceed_time_cpu >= config.min_seconds_for_alert) {
            snprintf(alert_msg, sizeof(alert_msg), 
                    "[ALERTA CPU] PID:%-6d %-25s >%-5.1f%% por %-3ld segundos (Actual:%.1f%%)\n",
                    process->pid, process->name, config.cpu_threshold,
                    now - history->first_exceed_time_cpu, process->cpu_usage);
            
            // Escribir en archivo de log
            fprintf(alert_file, "%s", alert_msg);
            
            // Escribir en FIFO si está disponible
            if (fifo) {
                fprintf(fifo, "%s", alert_msg);
                fflush(fifo); // Asegurar envío inmediato
            }
        }
    } else {
        history->first_exceed_time_cpu = 0;
    }

    // --- Alerta de RAM ---
    if (process->memory_usage_percent > config.ram_threshold) {
        if (history->first_exceed_time_ram == 0) {
            history->first_exceed_time_ram = now;
        } else if (now - history->first_exceed_time_ram >= config.min_seconds_for_alert) {
            snprintf(alert_msg, sizeof(alert_msg),
                    "[ALERTA RAM] PID:%-6d %-25s >%-5.1f%% por %-3ld segundos (Actual:%.1f%%)\n",
                    process->pid, process->name, config.ram_threshold,
                    now - history->first_exceed_time_ram, process->memory_usage_percent);
            
            // Escribir en archivo de log
            fprintf(alert_file, "%s", alert_msg);
            
            // Escribir en FIFO si está disponible
            if (fifo) {
                fprintf(fifo, "%s", alert_msg);
                fflush(fifo);
            }
        }
    } else {
        history->first_exceed_time_ram = 0;
    }

    // Cerrar archivos
    fclose(alert_file);
    if (fifo) {
        fclose(fifo);
    } else if (fifo_fd != -1) {
        close(fifo_fd);  // Cerrar descriptor si fopen falló
    }
}

// <-- CAMBIO: Nueva función para purgar procesos que ya no existen.
void purge_stale_processes()
{
    int i = 0;
    while (i < process_count)
    {
        if (process_dict[i].seen_this_cycle == 0)
        {
            // Este proceso no fue visto en el último ciclo, así que ha terminado.
            // Lo eliminamos reemplazándolo con el último elemento del array.
            process_dict[i] = process_dict[process_count - 1];
            process_count--;
            // No incrementamos 'i' porque necesitamos revisar el nuevo elemento
            // que acabamos de mover a la posición actual.
        }
        else
        {
            i++;
        }
    }
}

void write_config_file(float cpu, float ram, int interval, int min_seconds_alert)
{
    FILE *file = fopen(CONFIG_FILE, "w");
    if (file == NULL)
    {
        perror("Error al crear archivo de configuración");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "CPU_THRESHOLD = %.1f\n", cpu);
    fprintf(file, "RAM_THRESHOLD = %.1f\n", ram);
    fprintf(file, "SAMPLE_INTERVAL = %d\n", interval);
    fprintf(file, "MIN_SECONDS_FOR_ALERT = %d\n", min_seconds_alert);

    fclose(file);
    printf("Archivo de configuración creado/actualizado: %s\n", CONFIG_FILE);
}

void delete_config_file()
{
    if (unlink(CONFIG_FILE) == 0)
    {
        printf("Archivo de configuración eliminado: %s\n", CONFIG_FILE);
    }
    else
    {
        perror("Error al eliminar archivo de configuración");
    }
}

void write_pid_file()
{
    FILE *file = fopen(PID_FILE, "w");
    if (file)
    {
        fprintf(file, "%d", getpid());
        fclose(file);
    }
}

void daemonize()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("Error al hacer fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0)
    {
        perror("Error al crear nueva sesión");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0)
    {
        perror("Error en segundo fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close(x);
    }
}

void handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT)
    {
        running = 0;
    }
}

void monitor_processes(MonitorConfig config)
{
    SystemCPUStats prev_cpu_stats = get_system_cpu_stats();

    while (running)
    {
        for (int i = 0; i < process_count; i++)
        {
            process_dict[i].seen_this_cycle = 0;
        }

        SystemMemory mem_info = get_system_memory();
        SystemCPUStats current_cpu_stats = get_system_cpu_stats();
        float system_cpu_usage = calculate_system_cpu_usage(&prev_cpu_stats, &current_cpu_stats);
        prev_cpu_stats = current_cpu_stats;

        DIR *dir = opendir("/proc");
        if (!dir)
        {
            perror("Error al abrir /proc");
            sleep(config.sample_interval);
            continue;
        }

        ProcessInfo processes[MAX_PROCESSES] = {0};
        int current_scan_count = 0;
        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL && current_scan_count < MAX_PROCESSES)
        {
            if (entry->d_type == DT_DIR)
            {
                char *endptr;
                int pid = (int)strtol(entry->d_name, &endptr, 10);

                if (*endptr == '\0')
                {
                    ProcessInfo *current = &processes[current_scan_count++];
                    current->pid = pid;

                    get_process_name(pid, current->name);
                    current->memory_kb = get_process_memory(pid);

                    if (mem_info.total_memory_kb > 0)
                    {
                        current->memory_usage_percent =
                            (current->memory_kb * 100.0) / mem_info.total_memory_kb;
                    }

                    calculate_process_cpu_usage(pid, current);
                    check_for_anomalies(current, config);
                }
            }
        }
        closedir(dir);

        purge_stale_processes();

        // Escribir estadísticas a un archivo para que el comando de control las lea
        FILE *stats_file = fopen("/tmp/process_monitor_stats.log", "w");
        if (stats_file)
        {
            fprintf(stats_file, "System CPU Usage: %.1f%%\n", system_cpu_usage);
            fprintf(stats_file, "Total Memory: %.2f MB\n", mem_info.total_memory_kb / 1024.0);
            fprintf(stats_file, "Free Memory: %.2f MB (%.1f%% used)\n",
                    mem_info.free_memory_kb / 1024.0,
                    100.0 - (mem_info.free_memory_kb * 100.0 / mem_info.total_memory_kb));
            fprintf(stats_file, "Active Processes: %d\n", current_scan_count);

            fprintf(stats_file, "\nProcess List:\n");
            fprintf(stats_file, "%-8s %-25s %-8s %-12s %-10s\n",
                    "PID", "Nombre", "CPU%", "Memoria%", "Memoria (MB)");

            for (int i = 0; i < current_scan_count; i++)
            {
                fprintf(stats_file, "%-8d %-25s %-8.1f %-12.2f %-10.2f\n",
                        processes[i].pid,
                        processes[i].name,
                        processes[i].cpu_usage,
                        processes[i].memory_usage_percent,
                        processes[i].memory_kb / 1024.0);
            }

            fclose(stats_file);
        }

        sleep(config.sample_interval);
    }

    remove(PID_FILE);
    remove("/tmp/process_monitor_stats.log");
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "--daemon") == 0)
    {
        float cpu_threshold = DEFAULT_CPU_THRESHOLD;
        float ram_threshold = DEFAULT_RAM_THRESHOLD;
        int interval = DEFAULT_SAMPLE_INTERVAL;
        int min_seconds_alert = MIN_SECONDS_FOR_ALERT;

        if (argc > 2)
            cpu_threshold = atof(argv[2]);
        if (argc > 3)
            ram_threshold = atof(argv[3]);
        if (argc > 4)
            interval = atoi(argv[4]);
        if (argc > 5)
            min_seconds_alert = atoi(argv[5]);

        write_config_file(cpu_threshold, ram_threshold, interval, min_seconds_alert);

        daemonize();
        write_pid_file();

        FILE *alert_file = fopen("/tmp/process_monitor_cpu_alerts.log", "w");
        if (alert_file)
        {
            fclose(alert_file);
        }
        else
        {
            perror("Error al crear archivo de alertas");
        }

        signal(SIGTERM, handle_signal);
        signal(SIGINT, handle_signal);

        MonitorConfig config = load_configuration();
        monitor_processes(config);

        return EXIT_SUCCESS;
    }
    else
    {
        printf("Este programa debe ser ejecutado por el script de control\n");
        return EXIT_FAILURE;
    }
}