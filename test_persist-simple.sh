#!/bin/bash

# Test: Montaje y desmontaje del filesystem con persistencia de un archivo

# Configuración
MOUNT_POINT="/tmp/fisopfs_test_mount"
TEST_FILE="hello.txt"
FILE_DISK="test.fisopfs"

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

# 2. Creo archivo de prueba dentro del FS
echo "Hola FUSE" > /tmp/$TEST_FILE

# 3. Monto el filesystem en background con archivo de disco
./fisopfs "$MOUNT_POINT" --filedisk "$FILE_DISK" &
FUSE_PID=$!

# Espero a que FUSE esté listo
sleep 1

# 4. Creo un archivo dentro del FS
cp /tmp/$TEST_FILE "$MOUNT_POINT/$TEST_FILE"

# 5. Desmonto para persistir
fusermount -u "$MOUNT_POINT"
wait $FUSE_PID 2>/dev/null

# 6. Monte nuevamente para verificar persistencia
mkdir -p "$MOUNT_POINT"
./fisopfs "$MOUNT_POINT" --filedisk "$FILE_DISK" &
FUSE_PID=$!
sleep 1

# 7. Verifico que el archivo persista
if [ -f "$MOUNT_POINT/$TEST_FILE" ]; then
    echo "Test PASÓ: archivo persistió"
    EXIT_CODE=0
else
    echo "Test FALLÓ: archivo no se encontró"
    EXIT_CODE=1
fi

# Cleanup final
fusermount -u "$MOUNT_POINT" 2>/dev/null
kill $FUSE_PID 2>/dev/null
rm -rf "$MOUNT_POINT" /tmp/$TEST_FILE

exit $EXIT_CODE
