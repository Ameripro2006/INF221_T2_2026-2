/*
 * ============================================================================
 * Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
 * Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
 * Archivo: code/grid_pathfinding/algorithms/path_result.h
 *
 * Referencias bibliográficas consultadas:
 *   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
 *     Introduction to Algorithms (4th ed.). MIT Press.
 *   - Erickson, J. (2019). Algorithms. University of Illinois.
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2),
 *     Departamento de Informática, Universidad Técnica Federico Santa María.
 *
 * Decisiones de diseño:
 *   - Se mantiene exactamente la guarda de inclusión PATH_RESULT_H, la
 *     estructura PathResult y las tres firmas de resolución establecidas en la
 *     plantilla oficial de grid_pathfinding.cpp para garantizar compatibilidad
 *     directa con pruebas automatizadas y evaluación externa.
 *   - Se expone la función auxiliar getLastBruteForceStackBytes() para reportar
 *     la memoria de pila máxima utilizada durante la recursión DFS de Fuerza
 *     Bruta sin alterar la firma pública de solveBruteForce().
 * ============================================================================
 */

#ifndef PATH_RESULT_H
#define PATH_RESULT_H

#include <cstddef>
#include <string>
#include <vector>

struct PathResult {
    bool found;
    long long score;
    std::string path;
    int remaining_lives;
};

PathResult solveBruteForce(int n, int m, int v, int p,
                           const std::vector<std::vector<int>>& types,
                           const std::vector<std::vector<long long>>& values);

PathResult solveDP(int n, int m, int v, int p,
                   const std::vector<std::vector<int>>& types,
                   const std::vector<std::vector<long long>>& values);

PathResult solveGreedy(int n, int m, int v, int p,
                       const std::vector<std::vector<int>>& types,
                       const std::vector<std::vector<long long>>& values);

// Telemetría auxiliar de pila de recursión para Fuerza Bruta (bytes en la profundidad máxima)
std::size_t getLastBruteForceStackBytes();

#endif // PATH_RESULT_H
