#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASE_DIR="${SCRIPT_DIR}/releases"
TMP_DIR="${SCRIPT_DIR}/build"

echo -e "\e[1;34m==> Preparando entorno de compilación...\e[0m"
mkdir -p "${TMP_DIR}" "${RELEASE_DIR}"

MOC_BIN="$(which moc-qt6 2>/dev/null || which /usr/lib/qt6/moc 2>/dev/null || which moc 2>/dev/null)"
if [ -z "$MOC_BIN" ]; then
    echo -e "\e[1;31mError: No se encontró 'moc' de Qt6 en el sistema.\e[0m"
    exit 1
fi

echo -e "\e[1;34m==> Generando archivos MOC con ${MOC_BIN}...\e[0m"
"$MOC_BIN" "${SCRIPT_DIR}/src/linux/main.cpp" -o "${TMP_DIR}/main.moc"

echo -e "\e[1;34m==> Compilando binario para Linux (C++17, Qt6, PulseAudio)...\e[0m"
g++ -std=c++17 -O2 -fPIC \
    -I"${TMP_DIR}" \
    $(pkg-config --cflags --libs Qt6Widgets Qt6Network keybinder-3.0 libpulse libpulse-mainloop-glib glib-2.0) \
    "${SCRIPT_DIR}/src/linux/main.cpp" \
    -o "${RELEASE_DIR}/microphone-indicator-linux"

cp -r "${SCRIPT_DIR}/resources" "${RELEASE_DIR}/"

echo -e "\e[1;32m==> ¡Compilación exitosa!\e[0m"
echo -e "\e[1;32m==> Binario listo en: ${RELEASE_DIR}/microphone-indicator-linux\e[0m"
