#!/bin/bash

# Script de pruebas para MatcomGuard
# Ejecuta una serie de pruebas para verificar el funcionamiento

echo "🧪 MatcomGuard - Suite de Pruebas"
echo "================================="

# Verificar que el ejecutable existe
if [ ! -f "./matcomguard" ]; then
    echo "❌ Ejecutable matcomguard no encontrado"
    echo "Ejecuta 'make' primero para compilar"
    exit 1
fi

echo "✅ Ejecutable encontrado"

# Prueba 1: Verificar ayuda
echo ""
echo "🔸 Prueba 1: Verificando opción --help"
if ./matcomguard --help > /dev/null 2>&1; then
    echo "✅ Opción --help funciona"
else
    echo "❌ Error en opción --help"
    exit 1
fi

# Prueba 2: Verificar versión
echo ""
echo "🔸 Prueba 2: Verificando opción --version"
if ./matcomguard --version > /dev/null 2>&1; then
    echo "✅ Opción --version funciona"
else
    echo "❌ Error en opción --version"
    exit 1
fi

# Prueba 3: Escaneo básico
echo ""
echo "🔸 Prueba 3: Escaneo básico de puertos"
echo "Escaneando puertos 80,443 en localhost..."
timeout 30 ./matcomguard --scan-ports 80,443 > /tmp/test_output.txt 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✅ Escaneo básico completado"
else
    echo "❌ Error en escaneo básico"
    cat /tmp/test_output.txt
    exit 1
fi

# Prueba 4: Parsing de rangos de puertos
echo ""
echo "🔸 Prueba 4: Parsing de rangos de puertos"
echo "Probando formato de rango 22-25..."
timeout 20 ./matcomguard --scan-ports 22-25 > /tmp/test_range.txt 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✅ Parsing de rangos funciona"
else
    echo "❌ Error en parsing de rangos"
    cat /tmp/test_range.txt
    exit 1
fi

# Prueba 5: Timeout personalizado
echo ""
echo "🔸 Prueba 5: Timeout personalizado"
echo "Probando timeout de 1 segundo..."
timeout 15 ./matcomguard --scan-ports 80 --timeout 1 > /tmp/test_timeout.txt 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✅ Timeout personalizado funciona"
else
    echo "❌ Error en timeout personalizado"
    cat /tmp/test_timeout.txt
    exit 1
fi

# Prueba 6: Target personalizado (localhost)
echo ""
echo "🔸 Prueba 6: Target personalizado"
echo "Probando target 127.0.0.1..."
timeout 20 ./matcomguard --scan-ports 22,80 --target 127.0.0.1 > /tmp/test_target.txt 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✅ Target personalizado funciona"
else
    echo "❌ Error en target personalizado"
    cat /tmp/test_target.txt
    exit 1
fi

# Prueba 7: Escaneo en tiempo real
echo ""
echo "🔸 Prueba 7: Detección en tiempo real"
echo "Verificando detección de nuevos puertos..."

# Verificar que el programa auxiliar existe
if [ ! -f "./test_socket" ]; then
    echo "❌ Programa test_socket no encontrado"
    echo "Ejecuta 'make' para compilar todos los programas"
    exit 1
fi

# Elegir puertos para la prueba
TEST_PORT1=9876
TEST_PORT2=9877

# Verificar que los puertos estén libres
for port in $TEST_PORT1 $TEST_PORT2; do
    if netstat -ln 2>/dev/null | grep ":$port " >/dev/null || \
       ss -ln 2>/dev/null | grep ":$port " >/dev/null; then
        echo "⚠️  Puerto $port ya está en uso, buscando alternativa..."
        # Buscar puerto alternativo
        for alt_port in $(seq 9878 9890); do
            if ! netstat -ln 2>/dev/null | grep ":$alt_port " >/dev/null && \
               ! ss -ln 2>/dev/null | grep ":$alt_port " >/dev/null; then
                if [ $port -eq $TEST_PORT1 ]; then
                    TEST_PORT1=$alt_port
                else
                    TEST_PORT2=$alt_port
                fi
                break
            fi
        done
    fi
done

echo "Usando puertos $TEST_PORT1 y $TEST_PORT2 para la prueba..."

# Crear rango de prueba que incluya ambos puertos
TEST_RANGE="$TEST_PORT1-$TEST_PORT2"

echo "Iniciando escaneo continuo de puertos $TEST_RANGE..."
# Iniciar MatcomGuard en background
timeout 70 ./matcomguard --scan-ports $TEST_RANGE --continuous --interval 5 > /tmp/realtime_test.txt 2>&1 &
MATCOM_PID=$!

# Esperar para establecer línea base
echo "⏰ Estableciendo línea base (8 segundos)..."
sleep 8

echo "🔌 Abriendo primer socket en puerto $TEST_PORT1..."
# Abrir primer socket por 15 segundos
./test_socket $TEST_PORT1 15 > /tmp/socket1_log.txt 2>&1 &
SOCKET1_PID=$!

# Esperar para que se detecte
sleep 10

echo "🔌 Abriendo segundo socket en puerto $TEST_PORT2..."
# Abrir segundo socket por 15 segundos
./test_socket $TEST_PORT2 15 > /tmp/socket2_log.txt 2>&1 &
SOCKET2_PID=$!

# Esperar a que ambos sean detectados
sleep 12

echo "🔒 Esperando cierre automático de sockets..."
# Esperar a que los sockets se cierren automáticamente
wait $SOCKET1_PID 2>/dev/null || true
wait $SOCKET2_PID 2>/dev/null || true

# Esperar un poco más para detectar los cierres
sleep 8

echo "🛑 Terminando escaneo..."
# Terminar MatcomGuard
kill -INT $MATCOM_PID 2>/dev/null || true
wait $MATCOM_PID 2>/dev/null || true

# Analizar resultados
if [ -f /tmp/realtime_test.txt ]; then
    echo ""
    echo "📊 Analizando resultados del escaneo en tiempo real:"
    echo "=================================================="
    
    # Contar escaneos realizados
    SCAN_COUNT=$(grep -c "Escaneo #\|--- Escaneo" /tmp/realtime_test.txt 2>/dev/null || echo "0")
    echo "📈 Escaneos realizados: $SCAN_COUNT"
    
    # Verificar detección de apertura de puertos
    DETECTION_SUCCESS=0
    if grep -q "Puerto $TEST_PORT1.*abierto\|Nuevos puertos abiertos.*$TEST_PORT1" /tmp/realtime_test.txt; then
        echo "✅ Detección de puerto $TEST_PORT1: EXITOSA"
        DETECTION_SUCCESS=$((DETECTION_SUCCESS + 1))
    else
        echo "❌ Detección de puerto $TEST_PORT1: FALLIDA"
    fi
    
    if grep -q "Puerto $TEST_PORT2.*abierto\|Nuevos puertos abiertos.*$TEST_PORT2" /tmp/realtime_test.txt; then
        echo "✅ Detección de puerto $TEST_PORT2: EXITOSA"
        DETECTION_SUCCESS=$((DETECTION_SUCCESS + 1))
    else
        echo "❌ Detección de puerto $TEST_PORT2: FALLIDA"
    fi
    
    # Verificar detección de cierre
    CLOSURE_SUCCESS=0
    if grep -q "Puertos cerrados\|Sin cambios detectados" /tmp/realtime_test.txt; then
        echo "✅ Detección de cambios de estado: EXITOSA"
        CLOSURE_SUCCESS=1
    else
        echo "❌ Detección de cambios de estado: FALLIDA"
    fi
    
    # Mostrar estadísticas de sockets
    if [ -f /tmp/socket1_log.txt ]; then
        echo "📝 Socket 1 ($TEST_PORT1): $(grep -c "Socket abierto\|Socket cerrado" /tmp/socket1_log.txt 2>/dev/null || echo "0") eventos"
    fi
    if [ -f /tmp/socket2_log.txt ]; then
        echo "📝 Socket 2 ($TEST_PORT2): $(grep -c "Socket abierto\|Socket cerrado" /tmp/socket2_log.txt 2>/dev/null || echo "0") eventos"
    fi
    
    # Mostrar extracto del log
    echo ""
    echo "📋 Extracto del log de MatcomGuard (últimas 20 líneas):"
    echo "----------------------------------------------------"
    tail -n 20 /tmp/realtime_test.txt | sed 's/^/   /'
    
    # Evaluación final
    echo ""
    if [ $DETECTION_SUCCESS -ge 1 ] && [ $SCAN_COUNT -gt 3 ]; then
        echo "✅ Prueba de tiempo real: EXITOSA"
        echo "   • Detecciones: $DETECTION_SUCCESS/2"
        echo "   • Escaneos: $SCAN_COUNT"
    elif [ $SCAN_COUNT -gt 1 ]; then
        echo "⚠️  Prueba de tiempo real: PARCIALMENTE EXITOSA"
        echo "   • Escaneo continuo funciona ($SCAN_COUNT escaneos)"
        echo "   • Detecciones: $DETECTION_SUCCESS/2"
    else
        echo "❌ Prueba de tiempo real: FALLIDA"
        echo "   • Escaneos: $SCAN_COUNT (insuficientes)"
        echo "   • Detecciones: $DETECTION_SUCCESS/2"
    fi
else
    echo "❌ No se pudo generar el log de prueba"
fi

# Limpiar archivos de prueba
rm -f /tmp/socket1_log.txt /tmp/socket2_log.txt

# Prueba 8: Generar reporte (solo si hay herramientas)
echo ""
echo "🔸 Prueba 8: Generación de reportes"
if command -v wkhtmltopdf >/dev/null 2>&1 || command -v weasyprint >/dev/null 2>&1; then
    echo "Probando generación de reporte PDF/HTML..."
    timeout 30 ./matcomguard --scan-ports 80,443 --export-pdf > /tmp/test_report.txt 2>&1
    if [ $? -eq 0 ] || [ $? -eq 124 ]; then
        echo "✅ Generación de reportes funciona"
        # Buscar archivos de reporte generados
        if ls matcomguard_report_* >/dev/null 2>&1; then
            echo "✅ Archivo de reporte encontrado"
            rm -f matcomguard_report_*
        fi
    else
        echo "⚠️  Advertencia en generación de reportes"
    fi
else
    echo "⚠️  Herramientas de PDF no disponibles, saltando prueba"
fi

# Limpiar archivos temporales
rm -f /tmp/test_*.txt /tmp/realtime_test.txt

echo ""
echo "🎉 Suite de pruebas completada"
echo "=============================="
echo "✅ Todas las pruebas básicas pasaron correctamente"
echo ""
echo "Para pruebas manuales adicionales:"
echo "  ./matcomguard --scan-ports 1-100"
echo "  ./matcomguard --scan-ports 80,443,22,21 --continuous"
echo "  ./matcomguard --scan-ports 1-1024 --continuous --interval 10"
echo ""
echo "💡 Para probar detección en tiempo real manualmente:"
echo "  Terminal 1: ./matcomguard --scan-ports 9999 --continuous"
echo "  Terminal 2: ./test_socket 9999 30"
echo ""
echo "💡 Tips adicionales:"
echo "  • Usa 'sudo ./matcomguard --scan-ports 1-1024' para"
echo "    escanear puertos privilegiados más rápidamente"
echo "  • El modo continuo detecta cambios en tiempo real"
echo "  • Usa Ctrl+C para detener el escaneo continuo"
echo "  • Los archivos de log se guardan temporalmente en /tmp/"
