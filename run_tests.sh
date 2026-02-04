#!/bin/bash

# Script principal para ejecutar todos los tests de fisopfs
# Uso: ./tests/run_tests.sh

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Contadores
PASSED=0
FAILED=0

echo "========================================="
echo "  FISOPFS - Tests de Operaciones de FS   "
echo "========================================="
echo ""

# Verifico que el binario existe
if [ ! -f "./fisopfs" ]; then
    echo -e "${RED} ERROR: El binario 'fisopfs' no existe${NC}"
    echo "   Ejecutá 'make' primero para compilar."
    exit 1
fi

# Busco y ejecuto cada test en la carpeta tests/
for test_script in tests/test_*.sh; do
    if [ -f "$test_script" ]; then
        # Extraigo los nombres de los tests
        test_name=$(basename "$test_script" .sh | sed 's/test_//')
        
        echo -e "${BLUE}▶ Ejecutando test: $test_name${NC}"
        
        # Ejecuto cada test
        bash "$test_script"
        exit_code=$?
        
        # Verifico el resultado
        if [ $exit_code -eq 0 ]; then
            echo -e "${GREEN}  PASÓ${NC}"
            ((PASSED++))
        else
            echo -e "${RED} FALLÓ${NC}"
            ((FAILED++))
        fi
        echo ""
    fi
done

echo "========================================="
echo "           Resumen de Tests             "
echo "========================================="
echo -e "${GREEN} Tests pasados: $PASSED${NC}"
echo -e "${RED} Tests fallados: $FAILED${NC}"
echo ""

# Exit code según elresultado final de todos los tests
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN} ¡Todos los tests pasaron!${NC}"
    exit 0
else
    echo -e "${RED} Hay tests fallidos. Revisar implementación.${NC}"
    exit 1
fi
