#!/bin/bash

# Test: creación de archivos en fisopfs (mknod/creat/open O_CREAT)

GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[33m"
NC="\e[0m"

ok()   { echo -e "  ${GREEN} ${NC} $1"; }
fail() { echo -e "  ${RED} ${NC} $1"; exit 1; }

MOUNT_POINT="/tmp/fisopfs_test_creat"
FILE1="test1.txt"
FILE2="test2.txt"
FILE3="nested.txt"
DIR1="subdir"

cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    rm -rf "$MOUNT_POINT"
}
trap cleanup EXIT

mkdir -p "$MOUNT_POINT"

./fisopfs -f "$MOUNT_POINT" &
FUSE_PID=$!
sleep 1

echo -e "${YELLOW}== Test 1: Crear archivo vacío ==${NC}"
touch "$MOUNT_POINT/$FILE1" 2>/dev/null
if [ ! -f "$MOUNT_POINT/$FILE1" ]; then fail "No se creó archivo vacío"
else ok "Archivo vacío creado correctamente"
fi


echo -e "${YELLOW}== Test 2: Crear archivo con contenido ==${NC}"
echo "hola" > "$MOUNT_POINT/$FILE2"

if [ ! -f "$MOUNT_POINT/$FILE2" ]; then fail "No se creó archivo con contenido"; fi

SIZE=$(stat -c%s "$MOUNT_POINT/$FILE2")
if [ "$SIZE" -ne 5 ]; then fail "Tamaño incorrecto (esperado 5, obtenido $SIZE)"
else ok "Archivo con contenido creado correctamente"
fi


echo -e "${YELLOW}== Test 3: Crear archivo que ya existe (touch no debe modificarlo) ==${NC}"
touch "$MOUNT_POINT/$FILE1" 2>/dev/null

SIZE=$(stat -c%s "$MOUNT_POINT/$FILE1")
if [ "$SIZE" -ne 0 ]; then fail "touch modificó un archivo existente"
else ok "Archivo existente no se modificó"
fi


echo -e "${YELLOW}== Test 4: Crear archivo en subdirectorio ==${NC}"
mkdir "$MOUNT_POINT/$DIR1"
echo "xyz" > "$MOUNT_POINT/$DIR1/$FILE3"

if [ ! -f "$MOUNT_POINT/$DIR1/$FILE3" ]; then fail "No se creó archivo en subdirectorio"
else ok "Archivo creado en subdirectorio correctamente"
fi


echo -e "${YELLOW}== Test 5: Crear muchos archivos (1..50) ==${NC}"
for i in $(seq 1 50); do
    echo "$i" > "$MOUNT_POINT/file_$i" || fail "No se pudo crear file_$i"
done

for i in $(seq 1 50); do
    if [ ! -f "$MOUNT_POINT/file_$i" ]; then fail "Archivo file_$i desapareció"
    fi
done
ok "50 archivos creados y verificados correctamente"

exit 0
