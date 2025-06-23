#!/bin/bash

# Script de instalación automática para MatcomGuard
# Instala dependencias y compila el proyecto

echo "🛡️  MatcomGuard - Script de Instalación Automática"
echo "=================================================="

# Función para detectar la distribución
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo $ID
    else
        echo "unknown"
    fi
}

# Función para instalar dependencias
install_dependencies() {
    local distro=$(detect_distro)
    
    echo "🔍 Distribución detectada: $distro"
    
    case $distro in
        "ubuntu"|"debian")
            echo "📦 Instalando dependencias para Ubuntu/Debian..."
            sudo apt-get update
            sudo apt-get install -y gcc build-essential wkhtmltopdf
            ;;
        "centos"|"rhel")
            echo "📦 Instalando dependencias para CentOS/RHEL..."
            sudo yum install -y gcc wkhtmltopdf
            ;;
        "fedora")
            echo "📦 Instalando dependencias para Fedora..."
            sudo dnf install -y gcc wkhtmltopdf
            ;;
        "arch")
            echo "📦 Instalando dependencias para Arch Linux..."
            sudo pacman -S gcc wkhtmltopdf
            ;;
        *)
            echo "⚠️  Distribución no reconocida. Instalando dependencias manualmente..."
            echo "Por favor, instala gcc y wkhtmltopdf manualmente"
            ;;
    esac
}

# Verificar si se ejecuta como root
if [ "$EUID" -eq 0 ]; then
    echo "❌ No ejecutes este script como root"
    exit 1
fi

# Verificar si el directorio contiene los archivos fuente
if [ ! -f "matcomguard.c" ]; then
    echo "❌ No se encontraron los archivos fuente de MatcomGuard"
    echo "Ejecuta este script desde el directorio del proyecto"
    exit 1
fi

# Preguntar si instalar dependencias
echo ""
read -p "¿Deseas instalar las dependencias del sistema? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    install_dependencies
fi

# Compilar el proyecto
echo ""
echo "🔨 Compilando MatcomGuard..."
if make clean && make; then
    echo "✅ Compilación exitosa!"
else
    echo "❌ Error durante la compilación"
    exit 1
fi

# Preguntar si instalar en el sistema
echo ""
read -p "¿Deseas instalar MatcomGuard en el sistema? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if make install; then
        echo "✅ MatcomGuard instalado exitosamente!"
        echo "Ahora puedes ejecutar 'matcomguard' desde cualquier ubicación"
    else
        echo "❌ Error durante la instalación"
        exit 1
    fi
fi

# Ejecutar pruebas básicas
echo ""
echo "🧪 Ejecutando pruebas básicas..."
if ./matcomguard --version; then
    echo "✅ MatcomGuard está funcionando correctamente!"
else
    echo "❌ Error en las pruebas básicas"
    exit 1
fi

# Mostrar información final
echo ""
echo "🎉 ¡Instalación completada!"
echo "================================"
echo "Para usar MatcomGuard:"
echo "  ./matcomguard --scan-ports 1-1024"
echo ""
echo "Para ver todas las opciones:"
echo "  ./matcomguard --help"
echo ""
echo "Ejemplos de uso:"
echo "  ./matcomguard --scan-ports 1-1024 --export-pdf"
echo "  ./matcomguard --scan-ports 80,443,22 --continuous"
echo ""
echo "📖 Consulta README.md para más información"
