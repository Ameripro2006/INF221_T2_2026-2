/*
 * ============================================================================
 * Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
 * Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
 * Archivo: code/grid_pathfinding/algorithms/greedy.cpp
 *
 * Referencias bibliográficas consultadas:
 *   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
 *     Introduction to Algorithms (4th ed.), Cap. 15: Greedy Algorithms. MIT Press.
 *   - Erickson, J. (2019). Algorithms, Cap. 4: Greedy Algorithms. University of Illinois.
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2), §2.1 y §3.3.
 *
 * Decisiones de diseño:
 *   - Implementación exacta del protocolo de navegación local del rover:
 *       * Regla 1: Una mina (t == -1) otorga exactamente -P como ganancia
 *         inmediata, ignorando el valor de la grilla en esa celda.
 *       * Regla 2: Una mina no es un vecino elegible si las vidas actuales
 *         son <= 1.
 *       * Regla 3: Si no existen vecinos válidos antes de llegar a la meta
 *         (n - 1, m - 1), retorna found = false (SIN SOLUCION).
 *       * Ante empate exacto de ganancia inmediata entre Derecha ('R') y
 *         Abajo ('D'), prioriza Derecha ('R').
 * ============================================================================
 */

#include "path_result.h"

#include <string>
#include <vector>

PathResult solveGreedy(int n, int m, int v, int p,
                       const std::vector<std::vector<int>>& types,
                       const std::vector<std::vector<long long>>& values) {
    if (n <= 0 || m <= 0 || v <= 0) {
        return {false, 0, "", 0};
    }
    if (static_cast<int>(types.size()) < n || static_cast<int>(values.size()) < n) {
        return {false, 0, "", 0};
    }
    if (static_cast<int>(types[0].size()) < m || static_cast<int>(values[0].size()) < m) {
        return {false, 0, "", 0};
    }

    int start_type = types[0][0];
    if (start_type == 0 || (start_type == -1 && v <= 1)) {
        return {false, 0, "", 0};
    }
    if (start_type != 1 && start_type != -1) {
        return {false, 0, "", 0};
    }

    long long penalty = -static_cast<long long>(p);
    int i = 0;
    int j = 0;
    int lives = (start_type == -1) ? (v - 1) : v;
    long long score = (start_type == -1) ? penalty : values[0][0];

    std::string path;

    while (i != n - 1 || j != m - 1) {
        bool can_r = false;
        long long gain_r = 0;
        if (j + 1 < m) {
            int tr = types[i][j + 1];
            if (tr == 1) {
                can_r = true;
                gain_r = values[i][j + 1];
            } else if (tr == -1 && lives > 1) {
                // Regla 1 y Regla 2: mina otorga -P y solo es válida si lives > 1
                can_r = true;
                gain_r = penalty;
            }
        }

        bool can_d = false;
        long long gain_d = 0;
        if (i + 1 < n) {
            int td = types[i + 1][j];
            if (td == 1) {
                can_d = true;
                gain_d = values[i + 1][j];
            } else if (td == -1 && lives > 1) {
                // Regla 1 y Regla 2: mina otorga -P y solo es válida si lives > 1
                can_d = true;
                gain_d = penalty;
            }
        }

        // Regla 3: atrapado sin vecinos válidos antes de llegar a la meta
        if (!can_r && !can_d) {
            return {false, 0, "", 0};
        }

        // Prioridad estricta a Derecha ('R') en caso de empate en ganancia inmediata
        bool go_right = (can_r && !can_d) || (can_r && can_d && gain_r >= gain_d);

        if (go_right) {
            j++;
            score += gain_r;
            if (types[i][j] == -1) {
                lives--;
            }
            path.push_back('R');
        } else {
            i++;
            score += gain_d;
            if (types[i][j] == -1) {
                lives--;
            }
            path.push_back('D');
        }
    }

    return {true, score, path, lives};
}
