#!/bin/bash

GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[33m"
NC="\e[0m"

ok()   { echo -e "  ${GREEN} ${NC} $1"; }
fail() { echo -e "  ${RED} ${NC} $1"; exit 1; }

MOUNT="/tmp/fisopfs_test_unlink"

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


# -----------------------------------------
echo -e "${YELLOW}== Test 1: Eliminar archivo existente ==${NC}"

echo "hola" > "$MOUNT/file1.txt"

rm "$MOUNT/file1.txt" 2>/dev/null

if [ -f "$MOUNT/file1.txt" ]; then
    fail "file1.txt todavía existe luego de rm"
else
    ok "Archivo eliminado correctamente"
fi


# -----------------------------------------
echo -e "${YELLOW}== Test 2: Eliminar archivo que no existe ==${NC}"

rm "$MOUNT/noexiste.txt" 2>/dev/null
if [ $? -eq 0 ]; then
    fail "rm no devolvió error al eliminar archivo inexistente"
else
    ok "rm devolvió error correctamente al no existir el archivo"
fi


# -----------------------------------------
echo -e "${YELLOW}== Test 3: Intentar eliminar un directorio con rm ==${NC}"

mkdir "$MOUNT/dir1"

rm "$MOUNT/dir1" 2>/dev/null
if [ $? -eq 0 ]; then
    fail "rm debería fallar cuando intenta eliminar un directorio"
else
    ok "rm falló correctamente al intentar borrar un directorio"
fi


# -----------------------------------------
echo -e "${YELLOW}== Test 4: Crear → borrar → crear nuevamente ==${NC}"

echo "123" > "$MOUNT/reuse.txt"
rm "$MOUNT/reuse.txt"

echo "456" > "$MOUNT/reuse.txt"

CONTENT=$(cat "$MOUNT/reuse.txt")
if [ "$CONTENT" != "456" ]; then
    fail "El archivo recreado no contiene el contenido esperado"
else
    ok "Archivo recreado correctamente después de rm"
fi

exit 0
