#!/bin/bash

# Test: Elimina directorios vacíos y verifica que NO se puedan borrar directorios con contenido

# Configuración
MOUNT_POINT="/tmp/fisopfs_test_rmdir"
TEST_DIR1="empty_dir"
TEST_DIR2="no_empty_dir"
TEST_DIR3="subdir"

# Función de cleanup (se ejecuta al terminar)
cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    rm -rf "$MOUNT_POINT"
}

# Asegura el cleanup al salir
trap cleanup EXIT

# 1. Prepara el punto de montaje
mkdir -p "$MOUNT_POINT"

# 2. Monta el filesystem en el background
./fisopfs -f "$MOUNT_POINT" &
FUSE_PID=$!

# Espera a que FUSE esté listo
sleep 1

# 3. Crea directorios de prueba
mkdir "$MOUNT_POINT/$TEST_DIR1" 2>/dev/null        # Directorio vacío
mkdir "$MOUNT_POINT/$TEST_DIR2" 2>/dev/null        # Directorio que tendrá contenido
mkdir "$MOUNT_POINT/$TEST_DIR2/$TEST_DIR3" 2>/dev/null  # Subdirectorio

# 4. Test 1: Elimina directorio vacío (debe funcionar)
rmdir "$MOUNT_POINT/$TEST_DIR1" 2>/dev/null
if [ -d "$MOUNT_POINT/$TEST_DIR1" ]; then
    exit 1  # FALLÓ: el directorio vacío NO se borró
fi

# 5. Test 2: Intenta eliminar directorio NO vacío (debe fallar)
rmdir "$MOUNT_POINT/$TEST_DIR2" 2>/dev/null
if [ ! -d "$MOUNT_POINT/$TEST_DIR2" ]; then
    exit 1  # FALLÓ: el directorio NO vacío se borró (no debería)
fi

# 6. Test 3: Verifica que el subdirectorio sigue existiendo
if [ ! -d "$MOUNT_POINT/$TEST_DIR2/$TEST_DIR3" ]; then
    exit 1  # FALLÓ: el subdirectorio desapareció
fi

# 7. Test 4: Borra el subdirectorio y luego el padre (debe funcionar)
rmdir "$MOUNT_POINT/$TEST_DIR2/$TEST_DIR3" 2>/dev/null
rmdir "$MOUNT_POINT/$TEST_DIR2" 2>/dev/null

if [ -d "$MOUNT_POINT/$TEST_DIR2" ]; then
    exit 1  # FALLÓ: el directorio padre no se borró después de vaciar
fi

exit 0  # Todos los tests PASARON
