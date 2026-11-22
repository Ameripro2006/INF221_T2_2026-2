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
 *   - Formulación bottom-up con estado (i, j, v), donde lv = v - 1 en [0, V - 1]
 *     representa las vidas restantes tras ingresar a la celda (i, j).
 *   - Tablas aplanadas 1D contiguas en memoria con indexación std::size_t:
 *       idx(i, j, lv) = (i * M + j) * V + lv
 *     para maximizar localidad espacial en caché L1/L2 y evitar sobrecarga de
 *     vectores anidados (~54 MiB para 1024x2048 con V = 3).
 *   - Uso de NEG_INF = LLONG_MIN / 4 para distinguir estados inalcanzables de
 *     puntajes óptimos negativos sin riesgo de underflow aritmético.
 *   - Tabla compacta de predecesores parent (uint8_t: 0=ninguno, 1=izquierda 'R',
 *     2=arriba 'D') que permite reconstruir el camino óptimo en O(N + M).
 *   - En celdas mina (t == -1) se ignora estrictamente values[i][j] y se
 *     transiciona desde src_lv = lv + 1 con penalización -static_cast<long long>(p).
 *   - En (N - 1, M - 1) se elige el máximo puntaje entre todos los lv alcanzables,
 *     priorizando mayor cantidad de vidas restantes ante empates exactos.
 * ============================================================================
 */

#include "path_result.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
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

    const int start_type = types[0][0];
    const int end_type = types[n - 1][m - 1];
    if (start_type == 0 || end_type == 0) {
        return {false, 0, "", 0};
    }
    if (start_type == -1 && v <= 1) {
        return {false, 0, "", 0};
    }
    if (start_type != 1 && start_type != -1) {
        return {false, 0, "", 0};
    }

    const std::size_t N = static_cast<std::size_t>(n);
    const std::size_t M = static_cast<std::size_t>(m);
    const std::size_t V = static_cast<std::size_t>(v);
    const std::size_t row_stride = M * V;
    const std::size_t total_states = N * row_stride;

    constexpr long long NEG_INF = std::numeric_limits<long long>::min() / 4;
    const long long penalty = -static_cast<long long>(p);

    std::vector<long long> dp(total_states, NEG_INF);
    // 0 = sin predecesor, 1 = desde izquierda ('R'), 2 = desde arriba ('D')
    std::vector<std::uint8_t> parent(total_states, 0);

    // Caso base en (0, 0)
    if (start_type == 1) {
        dp[V - 1] = values[0][0];
    } else {
        dp[V - 2] = penalty;
    }

    // Llenado bottom-up fila por fila, columna por columna
    for (int i = 0; i < n; ++i) {
        const int* t_row = types[static_cast<std::size_t>(i)].data();
        const long long* v_row = values[static_cast<std::size_t>(i)].data();
        const std::size_t row_base = static_cast<std::size_t>(i) * row_stride;
        const bool has_up = (i > 0);

        for (int j = 0; j < m; ++j) {
            if (i == 0 && j == 0) {
                continue;
            }
            const int cell_type = t_row[j];
            if (cell_type == 0) {
                continue; // Muralla: todos los estados permanecen en NEG_INF
            }

            const std::size_t cell_base = row_base + static_cast<std::size_t>(j) * V;
            const bool has_left = (j > 0);

            if (cell_type == 1) {
                const long long gain = v_row[j];
                for (std::size_t lv = 0; lv < V; ++lv) {
                    long long best_prev = NEG_INF;
                    std::uint8_t best_dir = 0;

                    if (has_left) {
                        const long long cand = dp[cell_base - V + lv];
                        if (cand != NEG_INF) {
                            best_prev = cand;
                            best_dir = 1;
                        }
                    }
                    if (has_up) {
                        const long long cand = dp[cell_base - row_stride + lv];
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
                // Al pisar una mina: vidas decrecen en 1 (src_lv = lv + 1 -> lv), se ignora v_row[j]
                for (std::size_t lv = 0; lv + 1 < V; ++lv) {
                    const std::size_t src_lv = lv + 1;
                    long long best_prev = NEG_INF;
                    std::uint8_t best_dir = 0;

                    if (has_left) {
                        const long long cand = dp[cell_base - V + src_lv];
                        if (cand != NEG_INF) {
                            best_prev = cand;
                            best_dir = 1;
                        }
                    }
                    if (has_up) {
                        const long long cand = dp[cell_base - row_stride + src_lv];
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
    const std::size_t end_base = (static_cast<std::size_t>(n - 1) * M +
                                  static_cast<std::size_t>(m - 1)) * V;
    bool found = false;
    long long best_score = NEG_INF;
    std::size_t best_lv = 0;

    for (std::size_t rem = V; rem > 0; --rem) {
        const std::size_t lv = rem - 1;
        const long long val = dp[end_base + lv];
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
    const std::size_t path_len = static_cast<std::size_t>(n + m - 2);
    std::string path(path_len, '\0');

    int ci = n - 1;
    int cj = m - 1;
    std::size_t cur_lv = best_lv;

    for (std::size_t step = path_len; step > 0; --step) {
        const std::size_t cur_idx =
            (static_cast<std::size_t>(ci) * M + static_cast<std::size_t>(cj)) * V + cur_lv;
        const std::uint8_t dir = parent[cur_idx];

        if (types[static_cast<std::size_t>(ci)][static_cast<std::size_t>(cj)] == -1) {
            ++cur_lv;
        }

        if (dir == 1) {
            path[step - 1] = 'R';
            --cj;
        } else if (dir == 2) {
            path[step - 1] = 'D';
            --ci;
        } else {
            return {false, 0, "", 0};
        }
    }

    return {true, best_score, std::move(path), static_cast<int>(best_lv + 1)};
}
