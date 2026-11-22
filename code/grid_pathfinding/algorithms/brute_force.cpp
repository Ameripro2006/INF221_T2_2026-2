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
 *   - Cumplimiento estricto del enunciado: se descartan ÚNICAMENTE ramas
 *     inválidas (fuera de límites, muralla t == 0, o mina que deja lives == 0).
 *     NO existe ninguna poda por cota de puntaje ni memorización de estados.
 *   - Al pisar una mina (t == -1), su valor científico values[i][j] se ignora
 *     estrictamente y se aplica una penalización de -static_cast<long long>(p).
 *   - Memoria auxiliar estrictamente O(N + M):
 *       * Dos caches de punteros a filas (O(N)) para evitar indirecciones
 *         repetidas sobre std::vector<std::vector<...>> sin copiar la matriz.
 *       * Dos buffers std::string preasignados (curr_path y best_path) de
 *         longitud N + M - 2, copiando el mejor camino con std::memcpy sin
 *         realizar asignaciones dinámicas dentro de la recursión.
 *   - Medición real del tamaño de marco de pila (stack frame) entre profundidad
 *     0 y 1 (con respaldo de 64 bytes si no se alcanza profundidad 1), reportado
 *     mediante getLastBruteForceStackBytes().
 * ============================================================================
 */

#include "path_result.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

#if defined(__GNUC__) || defined(__clang__)
#define BF_NOINLINE __attribute__((noinline))
#else
#define BF_NOINLINE
#endif

std::size_t g_last_bf_stack_bytes = 0;

struct DfsContext {
    int n1;
    int m1;
    long long penalty;
    const int* const* type_rows;
    const long long* const* val_rows;
    char* curr_buf;
    char* best_buf;
    std::size_t path_len;
    bool found;
    long long best_score;
    int best_lives;
    int max_depth;
    std::uintptr_t addr_d0;
    std::size_t frame_bytes;

    BF_NOINLINE void dfs(int i, int j, int lives, long long score) {
        char frame_anchor = 0;
        const int depth = i + j;
        if (depth + 1 > max_depth) {
            max_depth = depth + 1;
        }
        if (depth <= 1) {
            volatile char* volatile_anchor = &frame_anchor;
            const std::uintptr_t cur =
                reinterpret_cast<std::uintptr_t>(const_cast<const char*>(volatile_anchor));
            if (depth == 0) {
                addr_d0 = cur;
            } else if (frame_bytes == 0 && addr_d0 != 0) {
                frame_bytes = (addr_d0 > cur)
                                  ? static_cast<std::size_t>(addr_d0 - cur)
                                  : static_cast<std::size_t>(cur - addr_d0);
            }
        }

        if (i == n1 && j == m1) {
            if (!found || score > best_score ||
                (score == best_score && lives > best_lives)) {
                found = true;
                best_score = score;
                best_lives = lives;
                if (path_len > 0) {
                    std::memcpy(best_buf, curr_buf, path_len);
                }
            }
            return;
        }

        // Orden determinista: primero Derecha ('R'), luego Abajo ('D')
        if (j < m1) {
            const int nj = j + 1;
            const int t = type_rows[i][nj];
            if (t == 1) {
                curr_buf[depth] = 'R';
                dfs(i, nj, lives, score + val_rows[i][nj]);
            } else if (t == -1 && lives > 1) {
                curr_buf[depth] = 'R';
                dfs(i, nj, lives - 1, score - penalty);
            }
        }

        if (i < n1) {
            const int ni = i + 1;
            const int t = type_rows[ni][j];
            if (t == 1) {
                curr_buf[depth] = 'D';
                dfs(ni, j, lives, score + val_rows[ni][j]);
            } else if (t == -1 && lives > 1) {
                curr_buf[depth] = 'D';
                dfs(ni, j, lives - 1, score - penalty);
            }
        }
    }
};

} // namespace

std::size_t getLastBruteForceStackBytes() {
    return g_last_bf_stack_bytes;
}

PathResult solveBruteForce(int n, int m, int v, int p,
                           const std::vector<std::vector<int>>& types,
                           const std::vector<std::vector<long long>>& values) {
    g_last_bf_stack_bytes = 0;

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

    int start_lives = v;
    long long start_score = 0;
    if (start_type == 1) {
        start_score = values[0][0];
    } else if (start_type == -1) {
        if (v <= 1) {
            return {false, 0, "", 0};
        }
        start_lives = v - 1;
        start_score = -static_cast<long long>(p);
    } else {
        return {false, 0, "", 0};
    }

    // Cache O(N) de punteros por fila para evitar indirección de vectores en la recursión
    std::vector<const int*> type_rows(static_cast<std::size_t>(n));
    std::vector<const long long*> val_rows(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        type_rows[static_cast<std::size_t>(i)] = types[static_cast<std::size_t>(i)].data();
        val_rows[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(i)].data();
    }

    const std::size_t path_len = static_cast<std::size_t>(n + m - 2);
    std::string curr_path(path_len, '\0');
    std::string best_path(path_len, '\0');

    constexpr long long NEG_INF = std::numeric_limits<long long>::min() / 4;

    DfsContext ctx{};
    ctx.n1 = n - 1;
    ctx.m1 = m - 1;
    ctx.penalty = static_cast<long long>(p);
    ctx.type_rows = type_rows.data();
    ctx.val_rows = val_rows.data();
    ctx.curr_buf = path_len > 0 ? &curr_path[0] : nullptr;
    ctx.best_buf = path_len > 0 ? &best_path[0] : nullptr;
    ctx.path_len = path_len;
    ctx.found = false;
    ctx.best_score = NEG_INF;
    ctx.best_lives = 0;
    ctx.max_depth = 0;
    ctx.addr_d0 = 0;
    ctx.frame_bytes = 0;

    ctx.dfs(0, 0, start_lives, start_score);

    const std::size_t eff_frame_bytes =
        (ctx.frame_bytes > 0) ? ctx.frame_bytes : static_cast<std::size_t>(64);
    g_last_bf_stack_bytes = static_cast<std::size_t>(ctx.max_depth) * eff_frame_bytes;

    if (!ctx.found) {
        return {false, 0, "", 0};
    }

    return {true, ctx.best_score, std::move(best_path), ctx.best_lives};
}
