# Makefile para MatcomGuard

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lpthread
TARGET = matcomguard
SOURCES = matcomguard.c port_scanner.c alert_manager.c report_generator.c
OBJECTS = $(SOURCES:.c=.o)

# Regla principal
all: $(TARGET) test_socket

# Regla para crear el ejecutable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✅ MatcomGuard compilado exitosamente!"

# Programa auxiliar para pruebas de sockets
test_socket: test_socket.c
	$(CC) $(CFLAGS) test_socket.c -o test_socket
	@echo "✅ test_socket compilado exitosamente!"

# Regla para compilar archivos .c a .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Instalación (requiere permisos de administrador)
install: $(TARGET)
	@echo "📦 Instalando MatcomGuard..."
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	@echo "✅ MatcomGuard instalado en /usr/local/bin/"

# Desinstalación
uninstall:
	@echo "🗑️  Desinstalando MatcomGuard..."
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "✅ MatcomGuard desinstalado"

# Limpiar archivos compilados
clean:
	rm -f $(OBJECTS) $(TARGET) test_socket
	rm -f *.html *.pdf
	@echo "🧹 Archivos temporales eliminados"

# Limpiar completamente
distclean: clean
	rm -f *~ *.bak
	@echo "🧹 Limpieza completa realizada"

# Ejecutar pruebas básicas
test: $(TARGET) test_socket
	@echo "🧪 Ejecutando pruebas básicas..."
	./$(TARGET) --help
	@echo "✅ Pruebas básicas completadas"

# Verificar dependencias
check-deps:
	@echo "🔍 Verificando dependencias..."
	@command -v gcc >/dev/null 2>&1 || { echo "❌ GCC no encontrado. Instalar con: sudo apt-get install gcc"; exit 1; }
	@echo "✅ GCC encontrado"
	@command -v wkhtmltopdf >/dev/null 2>&1 && echo "✅ wkhtmltopdf encontrado" || echo "⚠️  wkhtmltopdf no encontrado (opcional para PDF)"
	@echo "✅ Verificación de dependencias completada"

# Crear paquete de distribución
dist: clean
	@echo "📦 Creando paquete de distribución..."
	tar -czf matcomguard-1.0.0.tar.gz *.c *.h Makefile README.md
	@echo "✅ Paquete creado: matcomguard-1.0.0.tar.gz"

# Mostrar ayuda
help:
	@echo "MatcomGuard - Makefile"
	@echo "===================="
	@echo "Objetivos disponibles:"
	@echo "  all        - Compilar MatcomGuard (por defecto)"
	@echo "  install    - Instalar en /usr/local/bin (requiere sudo)"
	@echo "  uninstall  - Desinstalar del sistema"
	@echo "  clean      - Limpiar archivos compilados"
	@echo "  distclean  - Limpieza completa"
	@echo "  test       - Ejecutar pruebas básicas"
	@echo "  check-deps - Verificar dependencias del sistema"
	@echo "  dist       - Crear paquete de distribución"
	@echo "  help       - Mostrar esta ayuda"

# Desarrollo: compilar con flags de debug
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)
	@echo "🐛 MatcomGuard compilado con información de debug"

# Reglas que no crean archivos
.PHONY: all clean distclean install uninstall test check-deps dist help debug
