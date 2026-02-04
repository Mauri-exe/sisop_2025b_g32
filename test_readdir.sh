#!/bin/bash

# Test: Lista el contenido de un directorio

# Configuración
MOUNT_POINT="/tmp/fisopfs_test_readdir"
TEST_DIR1="dir1"
TEST_DIR2="dir2"
TEST_DIR3="subdir"

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

# 3. Creo varios directorios para listar
mkdir "$MOUNT_POINT/$TEST_DIR1" 2>/dev/null
mkdir "$MOUNT_POINT/$TEST_DIR2" 2>/dev/null 
mkdir "$MOUNT_POINT/$TEST_DIR1/$TEST_DIR3" 2>/dev/null

# 4. Listo el contenido de la raíz y capturo la salida
ROOT_CONTENT=$(ls "$MOUNT_POINT" 2>/dev/null)

# 5. Listo el contenido de dir1 y capturo la salida
DIR1_CONTENT=$(ls "$MOUNT_POINT/$TEST_DIR1" 2>/dev/null)

# 6. Verifico los resultados
# Debe aparecer dir1 y dir2 en la raíz
if echo "$ROOT_CONTENT" | grep -q "$TEST_DIR1" && \
   echo "$ROOT_CONTENT" | grep -q "$TEST_DIR2"; then
    # Debe aparecer subdir dentro de dir1
    if echo "$DIR1_CONTENT" | grep -q "$TEST_DIR3"; then
        exit 0  # Test PASÓ
    fi
fi

exit 1  # Test FALLÓ
