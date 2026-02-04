#!/bin/bash

GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[33m"
NC="\e[0m"

ok()   { echo -e "  ${GREEN} ${NC} $1"; }
fail() { echo -e "  ${RED} ${NC} $1"; exit 1; }

MOUNT_POINT="/tmp/fisopfs_test_rw"
FILE="test_rw.txt"

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

echo -e "${YELLOW}== Test 1: Escribir y leer archivo simple ==${NC}"

echo -n "hola" > "$MOUNT_POINT/$FILE"
CONTENT=$(cat "$MOUNT_POINT/$FILE")

if [ "$CONTENT" != "hola" ]; then fail "Read devuelve contenido incorrecto"
else ok "Escritura y lectura simple funcionan"
fi


echo -e "${YELLOW}== Test 2: Sobrescribir contenido (truncate + write) ==${NC}"

echo -n "abc123" > "$MOUNT_POINT/$FILE"
CONTENT=$(cat "$MOUNT_POINT/$FILE")

if [ "$CONTENT" != "abc123" ]; then fail "Overwrite falló"
else ok "Overwrite funciona correctamente"
fi


echo -e "${YELLOW}== Test 3: Append (escritura con offset automático) ==${NC}"

echo -n "XYZ" >> "$MOUNT_POINT/$FILE"
CONTENT=$(cat "$MOUNT_POINT/$FILE")

if [ "$CONTENT" != "abc123XYZ" ]; then fail "Append falló"
else ok "Append funciona correctamente"
fi


echo -e "${YELLOW}== Test 4: Leer parcialmente (offsets de read) ==${NC}"

PART=$(dd if="$MOUNT_POINT/$FILE" bs=1 skip=3 count=3 2>/dev/null)

if [ "$PART" != "123" ]; then fail "Lectura parcial incorrecta (esperado 123, obtenido '$PART')"
else ok "Read parcial funciona"
fi


echo -e "${YELLOW}== Test 5: Escribir en el medio del archivo (write con offset) ==${NC}"

# Escribe "ZZZ" a partir de offset 3 usando dd
printf "ZZZ" | dd of="$MOUNT_POINT/$FILE" bs=1 seek=3 conv=notrunc 2>/dev/null

CONTENT=$(cat "$MOUNT_POINT/$FILE")

if [ "$CONTENT" != "abcZZZXYZ" ]; then fail "Write con offset falló"
else ok "Write con offset funciona correctamente"
fi


echo -e "${YELLOW}== Test 6: Escribir más allá del tamaño actual (expansión con holes) ==${NC}"

# Creamos un salto a offset 20
printf "END" | dd of="$MOUNT_POINT/$FILE" bs=1 seek=20 conv=notrunc 2>/dev/null

SIZE=$(stat -c%s "$MOUNT_POINT/$FILE")
if [ "$SIZE" -ne 23 ]; then fail "Tamaño incorrecto tras escribir con hole (esperado 23, obtenido $SIZE)"
fi

# El contenido entre medio DEBE ser ceros
MID=$(dd if="$MOUNT_POINT/$FILE" bs=1 skip=9 count=11 2>/dev/null | xxd -p)

# Esperamos 11 bytes de "00"
EXPECTED=$(printf '00%.0s' {1..11})

if [ "$MID" != "$EXPECTED" ]; then fail "El hole no fue rellenado con ceros"
else ok "Expansión con holes funciona correctamente"
fi


echo -e "${YELLOW}== Test 7: Leer más allá del tamaño (read debe truncar)==${NC}"

PART=$(dd if="$MOUNT_POINT/$FILE" bs=1 skip=22 count=10 2>/dev/null)

if [ "$PART" != "D" ]; then fail "Read fuera de rango está devolviendo basura"
else ok "Read fuera de rango se trunca correctamente"
fi

exit 0
