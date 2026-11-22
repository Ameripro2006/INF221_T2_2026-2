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
 *   - Implementación exacta del protocolo de navegación local del rover (§2.1):
 *       * Candidatas válidas: Derecha (i, j + 1) y Abajo (i + 1, j) dentro de
 *         límites que no sean murallas (t != 0) y que no sean minas cuando v == 1.
 *       * Ganancia neta: values[ni][nj] si es celda normal (t == 1), o
 *         -static_cast<long long>(p) si es mina (t == -1), ignorando values[ni][nj].
 *       * Selección: avanza hacia la candidata de mayor ganancia neta; ante
 *         cualquier empate exacto (incluso mina -P vs normal g == -P), prioriza
 *         estrictamente Derecha ('R').
 *       * Si no existe ninguna candidata válida antes de (n - 1, m - 1), retorna
 *         found = false (SIN SOLUCION).
 *   - Uso de std::vector<char> pre-reservado a longitud N + M - 2 para registrar
 *     los movimientos sin sufrir el efecto de Small String Optimization (SSO <= 15
 *     bytes), reflejando fielmente la memoria auxiliar Theta(N + M) incluso en
 *     grillas pequeñas (4x4, 4x8, 8x8) y en fallas tempranas.
 * ============================================================================
 */

#include "path_result.h"

#include <cstddef>
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

    const int start_type = types[0][0];
    if (start_type == 0 || (start_type == -1 && v <= 1)) {
        return {false, 0, "", 0};
    }
    if (start_type != 1 && start_type != -1) {
        return {false, 0, "", 0};
    }

    const long long penalty = -static_cast<long long>(p);
    int i = 0;
    int j = 0;
    int lives = (start_type == -1) ? (v - 1) : v;
    long long score = (start_type == -1) ? penalty : values[0][0];

    const std::size_t path_len = static_cast<std::size_t>(n + m - 2);
    std::vector<char> moves;
    if (path_len > 0) {
        moves.reserve(path_len);
    }

    while (i != n - 1 || j != m - 1) {
        bool can_r = false;
        long long gain_r = 0;
        if (j + 1 < m) {
            const int tr = types[static_cast<std::size_t>(i)][static_cast<std::size_t>(j + 1)];
            if (tr == 1) {
                can_r = true;
                gain_r = values[static_cast<std::size_t>(i)][static_cast<std::size_t>(j + 1)];
            } else if (tr == -1 && lives > 1) {
                can_r = true;
                gain_r = penalty;
            }
        }

        bool can_d = false;
        long long gain_d = 0;
        if (i + 1 < n) {
            const int td = types[static_cast<std::size_t>(i + 1)][static_cast<std::size_t>(j)];
            if (td == 1) {
                can_d = true;
                gain_d = values[static_cast<std::size_t>(i + 1)][static_cast<std::size_t>(j)];
            } else if (td == -1 && lives > 1) {
                can_d = true;
                gain_d = penalty;
            }
        }

        if (!can_r && !can_d) {
            return {false, 0, "", 0};
        }

        // Prioridad estricta a Derecha ('R') en caso de empate en ganancia neta
        const bool go_right = (can_r && !can_d) || (can_r && can_d && gain_r >= gain_d);

        if (go_right) {
            ++j;
            score += gain_r;
            if (types[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] == -1) {
                --lives;
            }
            moves.push_back('R');
        } else {
            ++i;
            score += gain_d;
            if (types[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] == -1) {
                --lives;
            }
            moves.push_back('D');
        }
    }

    return {true, score, std::string(moves.begin(), moves.end()), lives};
}
