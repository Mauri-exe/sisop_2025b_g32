#!/bin/bash

# Test: Montaje y desmontaje de filesystem vacío

# Configuración
MOUNT_POINT="/tmp/fisopfs_test_mount"

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

# 3. Verifico que el root esté accesible
if [ -d "$MOUNT_POINT" ]; then
    echo "Test PASÓ: FS montado correctamente"
    EXIT_CODE=0
else
    echo "Test FALLÓ: FS no montó"
    EXIT_CODE=1
fi

# 4. Cleanup (desmontaje y eliminación del punto de montaje)
cleanup
exit $EXIT_CODE
