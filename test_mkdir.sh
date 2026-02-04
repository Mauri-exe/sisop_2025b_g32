#!/bin/bash

# Test: Crea el directorio en la raíz del filesystem

# Configuración
MOUNT_POINT="/tmp/fisopfs_test_mkdir"
TEST_DIR="test_directory"

# Función de cleanup (se ejecuta al terminar)
cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    rm -rf "$MOUNT_POINT"
}

# Asegura el cleanup al salir
trap cleanup EXIT

# 1. Preparo el punto de montaje
mkdir -p "$MOUNT_POINT"

# 2. Monto el filesystem en el background
./fisopfs -f "$MOUNT_POINT" &
FUSE_PID=$!

# Espero a que FUSE esté listo
sleep 1

# 3. Ejecuto el test: creo el directorio
mkdir "$MOUNT_POINT/$TEST_DIR" 2>/dev/null

# 4. Verifico el resultado
if [ -d "$MOUNT_POINT/$TEST_DIR" ]; then
    exit 0  # Test PASÓ
else
    exit 1  # Test FALLÓ
fi
