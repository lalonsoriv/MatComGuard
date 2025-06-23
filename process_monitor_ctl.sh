#!/bin/bash

DAEMON="./process_monitor_daemon"
PID_FILE="/tmp/process_monitor.pid"
STATS_FILE="/tmp/process_monitor_stats.log"
ALERTS_FILE="/tmp/process_monitor_cpu_alerts.log"
ALERTS_PID_FILE="/tmp/process_monitor_alert.pid"
ALERTS_FIFO="/tmp/process_monitor_alert_pipe"

start() {
    if [ -f "$PID_FILE" ]; then
        echo "El monitor ya está en ejecución (PID $(cat $PID_FILE))"
        return 1
    fi
    
    cpu_threshold="$1"
    ram_threshold="$2"
    interval="$3"
    min_seconds_alert="${4:-2}"  # Valor por defecto: 2 segundos
    
    # Crear FIFO si no existe
    if [ ! -p "$ALERTS_FIFO" ]; then
        mkfifo "$ALERTS_FIFO"
    fi
    
    echo "Iniciando monitor de procesos..."
    nohup $DAEMON --daemon "$cpu_threshold" "$ram_threshold" "$interval" "$min_seconds_alert" >/dev/null 2>&1 &
    
    sleep 1  # Esperar breve momento para que el demonio inicie
    
    if [ -f "$PID_FILE" ]; then
        echo "Monitor iniciado (PID $(cat $PID_FILE))"
        # Iniciar visualizador de alertas en segundo plano
        nohup bash -c "while true; do if read -r line < \"$ALERTS_FIFO\"; then echo \"\$line\"; fi done" >/dev/null 2>&1 &
        echo $! > "$ALERTS_PID_FILE"
        echo "Visualizador de alertas iniciado (PID $(cat $ALERTS_PID_FILE))"
    else
        echo "Error al iniciar el monitor"
        return 1
    fi
}

stop() {
    if [ ! -f "$PID_FILE" ]; then
        echo "El monitor no está en ejecución"
        return 1
    fi
    
    # Detener visualizador de alertas si está corriendo
    if [ -f "$ALERTS_PID_FILE" ]; then
        alert_pid=$(cat "$ALERTS_PID_FILE")
        echo "Deteniendo visualizador de alertas (PID $alert_pid)..."
        kill -TERM $alert_pid
        rm -f "$ALERTS_PID_FILE"
    fi
    
    pid=$(cat "$PID_FILE")
    echo "Deteniendo monitor (PID $pid)..."
    kill -TERM $pid
    
    for i in {1..10}; do
        if [ ! -f "$PID_FILE" ]; then
            echo "Monitor detenido"
            # Eliminar FIFO al detener
            rm -f "$ALERTS_FIFO"
            return 0
        fi
        sleep 1
    done
    
    echo "No se pudo detener el monitor"
    return 1
}

status() {
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
        if ps -p $pid > /dev/null; then
            echo "Monitor en ejecución (PID $pid)"
            # Mostrar estado del visualizador de alertas
            if [ -f "$ALERTS_PID_FILE" ]; then
                alert_pid=$(cat "$ALERTS_PID_FILE")
                if ps -p $alert_pid > /dev/null; then
                    echo "Visualizador de alertas activo (PID $alert_pid)"
                else
                    echo "Visualizador de alertas inactivo"
                fi
            fi
            return 0
        else
            echo "Archivo PID existe pero proceso no encontrado"
            return 1
        fi
    else
        echo "Monitor no está en ejecución"
        return 1
    fi
}

stats() {
    if [ -f "$STATS_FILE" ]; then
        cat "$STATS_FILE"
    else
        echo "No hay datos de estadísticas disponibles"
        return 1
    fi
}

view_alerts() {
    # Mostrar alertas históricas si existen
    if [ -f "$ALERTS_FILE" ]; then
        echo "--- Alertas históricas ---"
        cat "$ALERTS_FILE"
        echo "--------------------------"
    fi
        
    while true; do
        if read -r line < "$ALERTS_FIFO"; then
            echo "$line"
        fi
    done
}

case "$1" in
    start)
        start "$2" "$3" "$4" "$5"
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        start "$2" "$3" "$4" "$5"
        ;;
    status)
        status
        ;;
    stats)
        stats
        ;;
    tail)
        view_alerts
        ;;
    *)
        echo "Uso: $0 {start [cpu_threshold] [ram_threshold] [interval] [min_seconds_alert] | stop | restart | status | stats | tail}"
        echo "Ejemplos:"
        echo "  $0 start 70 50 1 2  # Inicia con CPU 70%, RAM 50%, intervalo 1s, alerta tras 2s"
        echo "  $0 stop             # Detiene el monitor"
        echo "  $0 stats            # Muestra estadísticas actuales"
        echo "  $0 tail             # Muestra las alertas en tiempo real"
        exit 1
esac

exit $?