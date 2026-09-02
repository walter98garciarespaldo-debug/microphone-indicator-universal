#!/usr/bin/env bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo -e "\e[1;31mError: Por favor ejecuta este instalador como root o con sudo:\e[0m"
    echo "sudo $0"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "\e[1;34m==> Instalando Microphone Indicator en el sistema...\e[0m"

# 1. Crear directorios
mkdir -p /opt/microphone-indicator/resources /usr/local/bin /usr/share/applications

# 2. Copiar binario y recursos
if [ ! -f "${SCRIPT_DIR}/releases/microphone-indicator-linux" ]; then
    echo "Compilando binario primero..."
    bash "${SCRIPT_DIR}/build.sh"
fi

cp -f "${SCRIPT_DIR}/releases/microphone-indicator-linux" /opt/microphone-indicator/microphone-indicator
cp -rf "${SCRIPT_DIR}/resources/"*.png /opt/microphone-indicator/resources/
chmod +x /opt/microphone-indicator/microphone-indicator

# 3. Crear enlace simbólico
ln -sf /opt/microphone-indicator/microphone-indicator /usr/local/bin/microphone-indicator

# 4. Crear Desktop Entry
cat << 'DESK' > /usr/share/applications/microphone-indicator.desktop
[Desktop Entry]
Type=Application
Name=Microphone Indicator
GenericName=Indicador de Micrófono
Comment=Muestra el estado del micrófono y permite mutearlo
Exec=/opt/microphone-indicator/microphone-indicator
Icon=/opt/microphone-indicator/resources/mic-on.png
Terminal=false
Categories=Utility;Audio;
Keywords=microphone;indicator;mute;audio;
StartupNotify=false
DESK

echo -e "\e[1;32m==> ¡Instalación completada con éxito!\e[0m"
echo -e "Puedes iniciarlo desde el menú de aplicaciones de KDE o ejecutando: \e[1;33mmicrophone-indicator\e[0m"
