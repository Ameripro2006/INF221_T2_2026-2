# Tarea 2 - Grid pathfinding

Algoritmos: Fuerza Bruta (DFS con poda simple: muros y minas letales), Programación Dinámica (bottom-up, reconstruye ruta y vidas) y Greedy.

## Compilar
```
cd code/grid_pathfinding
make
```

## Ejecutar todo el pipeline
```
make run      # o ./run_all.sh
```
Genera las 288 grillas (`scripts/grid_generator.py --seed 2026`), ejecuta `dp -> bruteforce -> greedy` por instancia, escribe `data/grid_output/` y `data/measurements/measurements.csv`. BF solo corre con N+M <= 32. Una ejecución individual: `./grid_pathfinding data/grid_input/4_4_libre_D10_a.txt dp`.

## Gráficos
```
python3 scripts/plot_generator.py
```
Lee `measurements.csv` y deja 4 PNG en `data/plots/`.
