/*
 * ============================================================================
 * Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
 * Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
 * Archivo: code/grid_pathfinding/algorithms/brute_force.cpp
 *
 * Referencias bibliográficas consultadas:
 *   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
 *     Introduction to Algorithms (4th ed.). MIT Press. (Búsqueda exhaustiva / DFS)
 *   - Erickson, J. (2019). Algorithms. University of Illinois. (Backtracking)
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2), §3.1.
 *
 * Decisiones de diseño:
 *   - Búsqueda exhaustiva DFS desde (0, 0) hasta (n - 1, m - 1) en orden
 *     determinista Derecha ('R') antes que Abajo ('D').
 *   - Poda clásica estricta: se descartan únicamente ramas inválidas (fuera de
 *     límites, muralla t == 0, o mina t == -1 cuando las vidas restantes son
 *     <= 1, ya que al pisarla quedarían 0 vidas). No existe poda por cota de
 *     puntaje ni memorización de estados.
 *   - Al pisar una mina (t == -1), su valor científico values[i][j] se ignora
 *     estrictamente y se aplica una penalización de -p.
 *   - Se pasan las matrices y las cadenas de camino por referencia para evitar
 *     copias innecesarias durante la recursión.
 * ============================================================================
 */

#include "path_result.h"

#include <limits>
#include <string>
#include <vector>

namespace {

void dfs(int i, int j, int lives, long long score,
         int n, int m, long long penalty,
         const std::vector<std::vector<int>>& types,
         const std::vector<std::vector<long long>>& values,
         std::string& curr_path,
         std::string& best_path,
         bool& found,
         long long& best_score,
         int& best_lives) {
    if (i == n - 1 && j == m - 1) {
        if (!found || score > best_score ||
            (score == best_score && lives > best_lives)) {
            found = true;
            best_score = score;
            best_lives = lives;
            best_path = curr_path;
        }
        return;
    }

    int depth = i + j;

    // Orden determinista: primero Derecha ('R'), luego Abajo ('D')
    if (j + 1 < m) {
        int t = types[i][j + 1];
        if (t == 1) {
            curr_path[depth] = 'R';
            dfs(i, j + 1, lives, score + values[i][j + 1],
                n, m, penalty, types, values,
                curr_path, best_path, found, best_score, best_lives);
        } else if (t == -1 && lives > 1) {
            curr_path[depth] = 'R';
            dfs(i, j + 1, lives - 1, score - penalty,
                n, m, penalty, types, values,
                curr_path, best_path, found, best_score, best_lives);
        }
    }

    if (i + 1 < n) {
        int t = types[i + 1][j];
        if (t == 1) {
            curr_path[depth] = 'D';
            dfs(i + 1, j, lives, score + values[i + 1][j],
                n, m, penalty, types, values,
                curr_path, best_path, found, best_score, best_lives);
        } else if (t == -1 && lives > 1) {
            curr_path[depth] = 'D';
            dfs(i + 1, j, lives - 1, score - penalty,
                n, m, penalty, types, values,
                curr_path, best_path, found, best_score, best_lives);
        }
    }
}

} // namespace

PathResult solveBruteForce(int n, int m, int v, int p,
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
    int end_type = types[n - 1][m - 1];
    if (start_type == 0 || end_type == 0) {
        return {false, 0, "", 0};
    }

    int start_lives = v;
    long long start_score = 0;
    long long penalty = static_cast<long long>(p);

    if (start_type == 1) {
        start_score = values[0][0];
    } else if (start_type == -1) {
        if (v <= 1) {
            return {false, 0, "", 0};
        }
        start_lives = v - 1;
        start_score = -penalty;
    } else {
        return {false, 0, "", 0};
    }

    int path_len = n + m - 2;
    std::string curr_path(path_len, ' ');
    std::string best_path(path_len, ' ');

    bool found = false;
    long long best_score = std::numeric_limits<long long>::min() / 4;
    int best_lives = 0;

    dfs(0, 0, start_lives, start_score,
        n, m, penalty, types, values,
        curr_path, best_path, found, best_score, best_lives);

    if (!found) {
        return {false, 0, "", 0};
    }

    return {true, best_score, best_path, best_lives};
}
