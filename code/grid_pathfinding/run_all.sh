#!/usr/bin/env bash
set -e

# Normalizar directorio de trabajo a code/grid_pathfinding/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Seleccionar python3 o python
if command -v python3 >/dev/null 2>&1; then
    PYTHON="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON="python"
else
    echo "[ERROR] No se encontro python3 ni python en PATH." >&2
    exit 1
fi

# Asegurar que el binario exista
if [ ! -x ./grid_pathfinding ]; then
    make
fi

# Generar las 288 grillas de entrada con semilla fija 2026
"$PYTHON" scripts/grid_generator.py --seed 2026

# Inicializar directorios y encabezado CSV de mediciones
mkdir -p data/grid_output data/measurements
echo "instance,n,m,tipo,dominio,muestra,algoritmo,found,score,vidas,tiempo_ms,mem_kb" > data/measurements/measurements.csv

# Ejecutar todos los algoritmos respetando el orden: dp -> bruteforce -> greedy
shopt -s nullglob
for archivo in data/grid_input/*.txt; do
    ./grid_pathfinding "$archivo" dp
    ./grid_pathfinding "$archivo" bruteforce
    ./grid_pathfinding "$archivo" greedy
done

# Verificar que las salidas conocidas (en especial 4_4 y 4_8) no hayan cambiado
if command -v git >/dev/null 2>&1; then
    REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
    if [ -n "$REPO_ROOT" ]; then
        DIFF_OUT="$(git -C "$REPO_ROOT" diff --stat -- code/grid_pathfinding/data/grid_output/)"
        if echo "$DIFF_OUT" | grep -qE '(^|/)4_(4|8)_'; then
            echo "[ADVERTENCIA] Cambiaron salidas conocidas de 4_4 o 4_8 en code/grid_pathfinding/data/grid_output/:" >&2
            echo "$DIFF_OUT" | grep -E '(^|/)4_(4|8)_' >&2
        elif [ -n "$DIFF_OUT" ]; then
            echo "[ADVERTENCIA] Cambiaron salidas existentes en code/grid_pathfinding/data/grid_output/:" >&2
            echo "$DIFF_OUT" >&2
        else
            echo "[OK] Sin diferencias en code/grid_pathfinding/data/grid_output/."
        fi
    fi
fi
