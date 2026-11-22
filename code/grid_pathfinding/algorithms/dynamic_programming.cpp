/*
 * ============================================================================
 * Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
 * Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
 * Archivo: code/grid_pathfinding/algorithms/dynamic_programming.cpp
 *
 * Referencias bibliográficas consultadas:
 *   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
 *     Introduction to Algorithms (4th ed.), Cap. 14: Dynamic Programming. MIT Press.
 *   - Erickson, J. (2019). Algorithms, Cap. 3: Dynamic Programming. University of Illinois.
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2), §3.2.
 *
 * Decisiones de diseño:
 *   - Formulación bottom-up con estado (i, j, lv), donde lv = vidas - 1 en
 *     [0, v - 1] representa las vidas restantes tras ingresar a la celda (i, j).
 *   - Tablas aplanadas 1D en std::vector con indexación (i * m + j) * v + lv
 *     para mantener localidad de caché y eficiencia en grillas grandes.
 *   - Uso de NEG_INF = LLONG_MIN / 4 para distinguir estados inalcanzables de
 *     puntajes óptimos negativos sin riesgo de underflow.
 *   - Tabla de predecesores parent (0 = ninguno, 1 = desde izquierda 'R',
 *     2 = desde arriba 'D') que reconstruye el camino óptimo y las vidas
 *     restantes en O(N + M).
 *   - En celdas mina (t == -1) se ignora estrictamente values[i][j] y se
 *     transiciona desde lv + 1 con penalización -p.
 * ============================================================================
 */

#include "path_result.h"

#include <limits>
#include <string>
#include <vector>

PathResult solveDP(int n, int m, int v, int p,
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
    if (start_type == -1 && v <= 1) {
        return {false, 0, "", 0};
    }
    if (start_type != 1 && start_type != -1) {
        return {false, 0, "", 0};
    }

    int row_stride = m * v;
    int total_states = n * row_stride;

    const long long NEG_INF = std::numeric_limits<long long>::min() / 4;
    long long penalty = -static_cast<long long>(p);

    std::vector<long long> dp(total_states, NEG_INF);
    // 0 = sin predecesor, 1 = desde izquierda ('R'), 2 = desde arriba ('D')
    std::vector<char> parent(total_states, 0);

    // Caso base en (0, 0)
    if (start_type == 1) {
        dp[v - 1] = values[0][0];
    } else {
        dp[v - 2] = penalty;
    }

    // Llenado bottom-up fila por fila, columna por columna
    for (int i = 0; i < n; i++) {
        int row_base = i * row_stride;

        for (int j = 0; j < m; j++) {
            if (i == 0 && j == 0) {
                continue;
            }
            int cell_type = types[i][j];
            if (cell_type == 0) {
                continue; // Muralla: todos los estados permanecen en NEG_INF
            }

            int cell_base = row_base + j * v;

            if (cell_type == 1) {
                long long gain = values[i][j];
                for (int lv = 0; lv < v; lv++) {
                    long long best_prev = NEG_INF;
                    char best_dir = 0;

                    if (j > 0) {
                        long long cand = dp[cell_base - v + lv];
                        if (cand != NEG_INF) {
                            best_prev = cand;
                            best_dir = 1;
                        }
                    }
                    if (i > 0) {
                        long long cand = dp[cell_base - row_stride + lv];
                        if (cand != NEG_INF && (best_dir == 0 || cand > best_prev)) {
                            best_prev = cand;
                            best_dir = 2;
                        }
                    }

                    if (best_dir != 0) {
                        dp[cell_base + lv] = best_prev + gain;
                        parent[cell_base + lv] = best_dir;
                    }
                }
            } else if (cell_type == -1) {
                // Al pisar una mina: vidas decrecen en 1 (src_lv = lv + 1 -> lv), se ignora values[i][j]
                for (int lv = 0; lv + 1 < v; lv++) {
                    int src_lv = lv + 1;
                    long long best_prev = NEG_INF;
                    char best_dir = 0;

                    if (j > 0) {
                        long long cand = dp[cell_base - v + src_lv];
                        if (cand != NEG_INF) {
                            best_prev = cand;
                            best_dir = 1;
                        }
                    }
                    if (i > 0) {
                        long long cand = dp[cell_base - row_stride + src_lv];
                        if (cand != NEG_INF && (best_dir == 0 || cand > best_prev)) {
                            best_prev = cand;
                            best_dir = 2;
                        }
                    }

                    if (best_dir != 0) {
                        dp[cell_base + lv] = best_prev + penalty;
                        parent[cell_base + lv] = best_dir;
                    }
                }
            }
        }
    }

    // Selección del mejor estado alcanzable en (n - 1, m - 1)
    int end_base = ((n - 1) * m + (m - 1)) * v;
    bool found = false;
    long long best_score = NEG_INF;
    int best_lv = 0;

    for (int lv = v - 1; lv >= 0; lv--) {
        long long val = dp[end_base + lv];
        if (val != NEG_INF && (!found || val > best_score)) {
            found = true;
            best_score = val;
            best_lv = lv;
        }
    }

    if (!found) {
        return {false, 0, "", 0};
    }

    // Reconstrucción del camino óptimo desde (n - 1, m - 1) hacia (0, 0)
    int path_len = n + m - 2;
    std::string path(path_len, ' ');

    int ci = n - 1;
    int cj = m - 1;
    int cur_lv = best_lv;

    for (int step = path_len - 1; step >= 0; step--) {
        int cur_idx = (ci * m + cj) * v + cur_lv;
        char dir = parent[cur_idx];

        if (types[ci][cj] == -1) {
            cur_lv++;
        }

        if (dir == 1) {
            path[step] = 'R';
            cj--;
        } else if (dir == 2) {
            path[step] = 'D';
            ci--;
        } else {
            return {false, 0, "", 0};
        }
    }

    return {true, best_score, path, best_lv + 1};
}
