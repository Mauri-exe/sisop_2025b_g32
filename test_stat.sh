#!/bin/bash

# Test: Valida metadatos de archivos y directorios (stat, getattr)
# Verifica: UID/GID, permisos, timestamps, tamaño

GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[33m"
NC="\e[0m"

ok()   { echo -e "  ${GREEN} ${NC} $1"; }
fail() { echo -e "  ${RED} ${NC} $1"; exit 1; }

MOUNT="/tmp/fisopfs_test_stat"

cleanup() {
    fusermount -u "$MOUNT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    rm -rf "$MOUNT"
}
trap cleanup EXIT

echo -e "${YELLOW}== Preparando entorno ==${NC}"
mkdir -p "$MOUNT"

./fisopfs -f "$MOUNT" &
FUSE_PID=$!
sleep 1


echo -e "${YELLOW}== Test 1: UID/GID del usuario actual ==${NC}"

echo "test" > "$MOUNT/testfile.txt"

EXPECTED_UID=$(id -u)
ACTUAL_UID=$(stat -c%u "$MOUNT/testfile.txt")

if [ "$ACTUAL_UID" != "$EXPECTED_UID" ]; then
    fail "UID incorrecto (esperado $EXPECTED_UID, obtenido $ACTUAL_UID)"
else
    ok "UID correcto (getuid)"
fi

EXPECTED_GID=$(id -g)
ACTUAL_GID=$(stat -c%g "$MOUNT/testfile.txt")

if [ "$ACTUAL_GID" != "$EXPECTED_GID" ]; then
    fail "GID incorrecto (esperado $EXPECTED_GID, obtenido $ACTUAL_GID)"
else
    ok "GID correcto (getgid)"
fi


echo -e "${YELLOW}== Test 2: Permisos por defecto ==${NC}"

# Archivos deben tener 0644 (-rw-r--r--)
FILE_PERMS=$(stat -c%a "$MOUNT/testfile.txt")
if [ "$FILE_PERMS" != "644" ]; then
    fail "Permisos de archivo incorrectos (esperado 644, obtenido $FILE_PERMS)"
else
    ok "Permisos de archivo correctos (0644)"
fi

# Directorios deben tener 0755 (drwxr-xr-x)
mkdir "$MOUNT/testdir"
DIR_PERMS=$(stat -c%a "$MOUNT/testdir")
if [ "$DIR_PERMS" != "755" ]; then
    fail "Permisos de directorio incorrectos (esperado 755, obtenido $DIR_PERMS)"
else
    ok "Permisos de directorio correctos (0755)"
fi


echo -e "${YELLOW}== Test 3: Actualización de atime (access time) ==${NC}"

echo "contenido inicial" > "$MOUNT/atime_test.txt"
sleep 1

# Captura atime antes de leer
ATIME_BEFORE=$(stat -c%X "$MOUNT/atime_test.txt")
sleep 1

# Lee el archivo (debe actualizar atime)
cat "$MOUNT/atime_test.txt" > /dev/null

# Captura atime después de leer
ATIME_AFTER=$(stat -c%X "$MOUNT/atime_test.txt")

if [ "$ATIME_AFTER" -le "$ATIME_BEFORE" ]; then
    fail "atime no se actualizó tras lectura (antes: $ATIME_BEFORE, después: $ATIME_AFTER)"
else
    ok "atime se actualiza correctamente al leer"
fi


echo -e "${YELLOW}== Test 4: Actualización de mtime/ctime (modification time) ==${NC}"

echo "inicial" > "$MOUNT/mtime_test.txt"
sleep 1

# Captura mtime y ctime antes de escribir
MTIME_BEFORE=$(stat -c%Y "$MOUNT/mtime_test.txt")
CTIME_BEFORE=$(stat -c%Z "$MOUNT/mtime_test.txt")
sleep 1

# Modifica el archivo (debe actualizar mtime y ctime)
echo "modificado" > "$MOUNT/mtime_test.txt"

# Captura mtime y ctime después de escribir
MTIME_AFTER=$(stat -c%Y "$MOUNT/mtime_test.txt")
CTIME_AFTER=$(stat -c%Z "$MOUNT/mtime_test.txt")

if [ "$MTIME_AFTER" -le "$MTIME_BEFORE" ]; then
    fail "mtime no se actualizó tras escritura"
else
    ok "mtime se actualiza correctamente al escribir"
fi

if [ "$CTIME_AFTER" -le "$CTIME_BEFORE" ]; then
    fail "ctime no se actualizó tras modificación"
else
    ok "ctime se actualiza correctamente al modificar"
fi


echo -e "${YELLOW}== Test 5: Tamaño de archivo correcto ==${NC}"

echo -n "1234567890" > "$MOUNT/size_test.txt"
SIZE=$(stat -c%s "$MOUNT/size_test.txt")

if [ "$SIZE" -ne 10 ]; then
    fail "Tamaño incorrecto (esperado 10, obtenido $SIZE)"
else
    ok "Tamaño de archivo correcto"
fi

# Directorio debe tener tamaño 0
mkdir "$MOUNT/size_dir"
DIR_SIZE=$(stat -c%s "$MOUNT/size_dir")

if [ "$DIR_SIZE" -ne 0 ]; then
    fail "Tamaño de directorio incorrecto (esperado 0, obtenido $DIR_SIZE)"
else
    ok "Tamaño de directorio correcto (0)"
fi

exit 0
