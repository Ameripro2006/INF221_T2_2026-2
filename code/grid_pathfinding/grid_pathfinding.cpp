/*
 * ============================================================================
 * Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
 * Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
 * Archivo: code/grid_pathfinding/grid_pathfinding.cpp
 *
 * Referencias bibliográficas consultadas:
 *   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
 *     Introduction to Algorithms (4th ed.). MIT Press.
 *   - Erickson, J. (2019). Algorithms. University of Illinois.
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2),
 *     Departamento de Informática, Universidad Técnica Federico Santa María.
 *
 * Decisiones de diseño:
 *   - Medición portable de memoria auxiliar en C++17: sobrecarga global de los
 *     20 operadores estándar operator new / operator delete (incluyendo formas
 *     con tamaño, nothrow y std::align_val_t) respaldada por una tabla hash de
 *     direccionamiento abierto basada exclusivamente en std::calloc / std::free
 *     (sin recursión y 100% segura frente a DLLs en Windows/MinGW).
 *   - El rastreador se activa únicamente mediante RAII (ScopedMemTracker) en una
 *     llamada dedicada fuera del bucle cronometrado, garantizando sobrecarga nula
 *     durante las mediciones de tiempo con std::chrono::steady_clock. Para Fuerza
 *     Bruta se suma además getLastBruteForceStackBytes().
 *   - Carga rápida de grillas en buffer binario con escáner manual de enteros
 *     (compatible con fin de línea LF y CRLF), reduciendo la lectura de los 422 MiB
 *     del benchmark a ~1-2 segundos y aislando completamente la E/S de las métricas.
 *   - Validación independiente de cada solución (longitud N+M-2, límites, sin
 *     murallas, vidas >= 1 en cada paso, recálculo exacto de puntaje y vidas) y
 *     verificación estricta BF == DP en el 100% de las instancias con N + M <= 32.
 *   - Interfaz CLI de 4 modos: ejecución por lotes por defecto, --self-test
 *     (Figura 1 y casos borde), --input (archivo individual) y
 *     --input-dir/--output-dir/--measurements-dir (lotes aislados).
 * ============================================================================
 */

#include "algorithms/path_result.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

// ============================================================================
// Rastreador portable de memoria auxiliar (operator new / operator delete)
// ============================================================================
namespace mem_tracker {

struct Slot {
    void* ptr;
    std::size_t size;
    std::uint8_t state; // 0 = vacío, 1 = ocupado, 2 = lápida (tombstone)
};

static bool g_tracking = false;
static std::size_t g_current = 0;
static std::size_t g_peak = 0;
static Slot* g_slots = nullptr;
static std::size_t g_cap = 0;
static std::size_t g_count = 0;
static std::size_t g_used = 0;

static inline std::size_t hash_ptr(void* p) noexcept {
    std::uint64_t h = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
    h >>= 4;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return static_cast<std::size_t>(h);
}

static void rehash(std::size_t new_cap) noexcept {
    Slot* new_slots = static_cast<Slot*>(std::calloc(new_cap, sizeof(Slot)));
    if (new_slots == nullptr) {
        return;
    }
    const std::size_t mask = new_cap - 1;
    if (g_slots != nullptr) {
        for (std::size_t i = 0; i < g_cap; ++i) {
            if (g_slots[i].state == 1) {
                std::size_t idx = hash_ptr(g_slots[i].ptr) & mask;
                while (new_slots[idx].state == 1) {
                    idx = (idx + 1) & mask;
                }
                new_slots[idx] = g_slots[i];
            }
        }
        std::free(g_slots);
    }
    g_slots = new_slots;
    g_cap = new_cap;
    g_used = g_count;
}

static inline void record_alloc(void* ptr, std::size_t sz) noexcept {
    if (!g_tracking || ptr == nullptr) {
        return;
    }
    if (g_cap == 0) {
        rehash(1024);
    } else if ((g_used + 1) * 2 >= g_cap) {
        const std::size_t next_cap = (g_count * 4 >= g_cap) ? (g_cap * 2) : g_cap;
        rehash(next_cap);
    }
    if (g_slots == nullptr) {
        return;
    }

    const std::size_t mask = g_cap - 1;
    std::size_t idx = hash_ptr(ptr) & mask;
    std::size_t first_tomb = g_cap;

    while (g_slots[idx].state != 0) {
        if (g_slots[idx].state == 1 && g_slots[idx].ptr == ptr) {
            // Reemplazo defensivo si el mismo puntero ya estuviera registrado
            if (g_current >= g_slots[idx].size) {
                g_current -= g_slots[idx].size;
            } else {
                g_current = 0;
            }
            g_slots[idx].size = sz;
            g_current += sz;
            if (g_current > g_peak) {
                g_peak = g_current;
            }
            return;
        }
        if (g_slots[idx].state == 2 && first_tomb == g_cap) {
            first_tomb = idx;
        }
        idx = (idx + 1) & mask;
    }

    const std::size_t target = (first_tomb != g_cap) ? first_tomb : idx;
    if (g_slots[target].state == 0) {
        ++g_used;
    }
    g_slots[target].ptr = ptr;
    g_slots[target].size = sz;
    g_slots[target].state = 1;
    ++g_count;

    g_current += sz;
    if (g_current > g_peak) {
        g_peak = g_current;
    }
}

static inline void record_free(void* ptr) noexcept {
    if (g_count == 0 || ptr == nullptr || g_slots == nullptr) {
        return;
    }
    const std::size_t mask = g_cap - 1;
    std::size_t idx = hash_ptr(ptr) & mask;

    while (g_slots[idx].state != 0) {
        if (g_slots[idx].state == 1 && g_slots[idx].ptr == ptr) {
            if (g_current >= g_slots[idx].size) {
                g_current -= g_slots[idx].size;
            } else {
                g_current = 0;
            }
            g_slots[idx].ptr = nullptr;
            g_slots[idx].size = 0;
            g_slots[idx].state = 2;
            --g_count;
            return;
        }
        idx = (idx + 1) & mask;
    }
}

static void* alloc_unaligned(std::size_t sz, bool throw_on_fail) {
    const std::size_t req = (sz == 0) ? 1 : sz;
    for (;;) {
        void* p = std::malloc(req);
        if (p != nullptr) {
            if (g_tracking) {
                record_alloc(p, sz);
            }
            return p;
        }
        std::new_handler nh = std::get_new_handler();
        if (nh != nullptr) {
            nh();
        } else {
            if (throw_on_fail) {
                throw std::bad_alloc();
            }
            return nullptr;
        }
    }
}

static void* alloc_aligned(std::size_t sz, std::size_t align, bool throw_on_fail) {
    const std::size_t req = (sz == 0) ? 1 : sz;
    const std::size_t eff_align = (align < sizeof(void*)) ? sizeof(void*) : align;
    for (;;) {
        void* p = nullptr;
#ifdef _WIN32
        p = _aligned_malloc(req, eff_align);
#else
        if (posix_memalign(&p, eff_align, req) != 0) {
            p = nullptr;
        }
#endif
        if (p != nullptr) {
            if (g_tracking) {
                record_alloc(p, sz);
            }
            return p;
        }
        std::new_handler nh = std::get_new_handler();
        if (nh != nullptr) {
            nh();
        } else {
            if (throw_on_fail) {
                throw std::bad_alloc();
            }
            return nullptr;
        }
    }
}

static inline void free_unaligned(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    if (g_count != 0) {
        record_free(ptr);
    }
    std::free(ptr);
}

static inline void free_aligned(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    if (g_count != 0) {
        record_free(ptr);
    }
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

class ScopedMemTracker {
public:
    ScopedMemTracker() noexcept {
        g_current = 0;
        g_peak = 0;
        g_tracking = true;
    }

    ~ScopedMemTracker() noexcept {
        g_tracking = false;
    }

    ScopedMemTracker(const ScopedMemTracker&) = delete;
    ScopedMemTracker& operator=(const ScopedMemTracker&) = delete;

    std::size_t peak_bytes() const noexcept {
        return g_peak;
    }
};

} // namespace mem_tracker

// Todas las 20 sobrecargas estándar C++17 reemplazables de operator new / delete
void* operator new(std::size_t sz) {
    return mem_tracker::alloc_unaligned(sz, true);
}
void* operator new[](std::size_t sz) {
    return mem_tracker::alloc_unaligned(sz, true);
}
void* operator new(std::size_t sz, const std::nothrow_t&) noexcept {
    return mem_tracker::alloc_unaligned(sz, false);
}
void* operator new[](std::size_t sz, const std::nothrow_t&) noexcept {
    return mem_tracker::alloc_unaligned(sz, false);
}
void* operator new(std::size_t sz, std::align_val_t al) {
    return mem_tracker::alloc_aligned(sz, static_cast<std::size_t>(al), true);
}
void* operator new[](std::size_t sz, std::align_val_t al) {
    return mem_tracker::alloc_aligned(sz, static_cast<std::size_t>(al), true);
}
void* operator new(std::size_t sz, std::align_val_t al, const std::nothrow_t&) noexcept {
    return mem_tracker::alloc_aligned(sz, static_cast<std::size_t>(al), false);
}
void* operator new[](std::size_t sz, std::align_val_t al, const std::nothrow_t&) noexcept {
    return mem_tracker::alloc_aligned(sz, static_cast<std::size_t>(al), false);
}

void operator delete(void* ptr) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete[](void* ptr) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    mem_tracker::free_unaligned(ptr);
}
void operator delete(void* ptr, std::align_val_t) noexcept {
    mem_tracker::free_aligned(ptr);
}
void operator delete[](void* ptr, std::align_val_t) noexcept {
    mem_tracker::free_aligned(ptr);
}
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept {
    mem_tracker::free_aligned(ptr);
}
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept {
    mem_tracker::free_aligned(ptr);
}
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
    mem_tracker::free_aligned(ptr);
}
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept {
    mem_tracker::free_aligned(ptr);
}

// ============================================================================
// Estructuras auxiliares, carga de grillas, validación y benchmarking
// ============================================================================
namespace fs = std::filesystem;

namespace {

struct GridData {
    int n = 0;
    int m = 0;
    int v = 0;
    int p = 0;
    std::vector<std::vector<int>> types;
    std::vector<std::vector<long long>> values;
};

struct InstanceMeta {
    fs::path filepath;
    std::string instance; // stem sin .txt
    int n = 0;
    int m = 0;
    int v = 0;
    int p = 0;
    std::string grid_type;
    std::string domain;
    std::string sample;
    bool canonical = false;
};

struct BenchStats {
    PathResult result;
    int reps = 0;
    double time_median_ms = 0.0;
    double time_mean_ms = 0.0;
    double time_stddev_ms = 0.0;
    std::size_t aux_memory_bytes = 0;
};

struct MeasurementRow {
    std::string instance;
    int n = 0;
    int m = 0;
    long long nm = 0;
    int v = 0;
    int p = 0;
    std::string grid_type;
    std::string domain;
    std::string sample;
    std::string algorithm;
    bool found = false;
    long long score = 0;
    int remaining_lives = 0;
    std::size_t path_len = 0;
    int reps = 0;
    double time_median_ms = 0.0;
    double time_mean_ms = 0.0;
    double time_stddev_ms = 0.0;
    std::size_t aux_memory_bytes = 0;
    bool has_dp_score = false;
    long long dp_score = 0;
    bool has_gap = false;
    long long optimality_gap = 0;
    int greedy_failed = 0;
};

static inline bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static bool parseNextLL(const char*& p, const char* end, long long& out) noexcept {
    while (p < end && isSpace(*p)) {
        ++p;
    }
    if (p >= end) {
        return false;
    }
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    if (p >= end || *p < '0' || *p > '9') {
        return false;
    }
    unsigned long long acc = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        acc = acc * 10ULL + static_cast<unsigned long long>(*p - '0');
        ++p;
    }
    if (neg) {
        out = -static_cast<long long>(acc);
    } else {
        out = static_cast<long long>(acc);
    }
    return true;
}

static bool loadGridHeader(const fs::path& filepath, int& n, int& m, int& v, int& p) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    char buf[256];
    in.read(buf, sizeof(buf));
    const std::streamsize got = in.gcount();
    if (got <= 0) {
        return false;
    }
    const char* ptr = buf;
    const char* end = buf + got;
    long long ln = 0, lm = 0, lv = 0, lp = 0;
    if (!parseNextLL(ptr, end, ln) || !parseNextLL(ptr, end, lm) ||
        !parseNextLL(ptr, end, lv) || !parseNextLL(ptr, end, lp)) {
        return false;
    }
    n = static_cast<int>(ln);
    m = static_cast<int>(lm);
    v = static_cast<int>(lv);
    p = static_cast<int>(lp);
    return true;
}

static bool loadGrid(const fs::path& filepath, GridData& grid, std::string& err) {
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        err = "No se pudo abrir el archivo: " + filepath.string();
        return false;
    }
    const std::streamoff fsize = in.tellg();
    if (fsize <= 0) {
        err = "Archivo vacío o ilegible: " + filepath.string();
        return false;
    }
    std::string buf(static_cast<std::size_t>(fsize), '\0');
    in.seekg(0, std::ios::beg);
    if (!in.read(&buf[0], fsize)) {
        err = "Error al leer el archivo: " + filepath.string();
        return false;
    }

    const char* ptr = buf.data();
    const char* end = ptr + buf.size();
    long long ln = 0, lm = 0, lv = 0, lp = 0;
    if (!parseNextLL(ptr, end, ln) || !parseNextLL(ptr, end, lm) ||
        !parseNextLL(ptr, end, lv) || !parseNextLL(ptr, end, lp)) {
        err = "Encabezado N M V P inválido en: " + filepath.string();
        return false;
    }
    if (ln <= 0 || lm <= 0) {
        err = "Dimensiones N, M deben ser positivas en: " + filepath.string();
        return false;
    }

    grid.n = static_cast<int>(ln);
    grid.m = static_cast<int>(lm);
    grid.v = static_cast<int>(lv);
    grid.p = static_cast<int>(lp);

    grid.types.assign(static_cast<std::size_t>(grid.n),
                      std::vector<int>(static_cast<std::size_t>(grid.m), 0));
    grid.values.assign(static_cast<std::size_t>(grid.n),
                       std::vector<long long>(static_cast<std::size_t>(grid.m), 0));

    for (int i = 0; i < grid.n; ++i) {
        int* row = grid.types[static_cast<std::size_t>(i)].data();
        for (int j = 0; j < grid.m; ++j) {
            long long val = 0;
            if (!parseNextLL(ptr, end, val)) {
                err = "Matriz de tipos incompleta en: " + filepath.string();
                return false;
            }
            row[j] = static_cast<int>(val);
        }
    }

    for (int i = 0; i < grid.n; ++i) {
        long long* row = grid.values[static_cast<std::size_t>(i)].data();
        for (int j = 0; j < grid.m; ++j) {
            long long val = 0;
            if (!parseNextLL(ptr, end, val)) {
                err = "Matriz de valores incompleta en: " + filepath.string();
                return false;
            }
            row[j] = val;
        }
    }

    return true;
}

static bool validatePath(int n, int m, int v, int p,
                         const std::vector<std::vector<int>>& types,
                         const std::vector<std::vector<long long>>& values,
                         const PathResult& res,
                         std::string& err) {
    if (!res.found) {
        return true;
    }
    if (n <= 0 || m <= 0 || v <= 0) {
        err = "Se reportó solución con parámetros inválidos (n, m o v <= 0)";
        return false;
    }
    const std::size_t expected_len = static_cast<std::size_t>(n + m - 2);
    if (res.path.size() != expected_len) {
        err = "Longitud de camino inválida: obtenida=" + std::to_string(res.path.size()) +
              ", esperada=" + std::to_string(expected_len);
        return false;
    }

    int i = 0;
    int j = 0;
    int lives = v;
    long long score = 0;

    if (types[0][0] == 1) {
        score = values[0][0];
    } else if (types[0][0] == -1) {
        --lives;
        score = -static_cast<long long>(p);
    } else {
        err = "Celda inicial (0,0) inválida (muralla o tipo desconocido)";
        return false;
    }
    if (lives < 1) {
        err = "Vidas agotadas en celda inicial (0,0)";
        return false;
    }

    for (std::size_t k = 0; k < res.path.size(); ++k) {
        const char mv = res.path[k];
        if (mv == 'R') {
            ++j;
        } else if (mv == 'D') {
            ++i;
        } else {
            err = std::string("Movimiento inválido en paso ") + std::to_string(k) + ": '" + mv + "'";
            return false;
        }
        if (i < 0 || i >= n || j < 0 || j >= m) {
            err = "Camino fuera de límites en paso " + std::to_string(k);
            return false;
        }
        const int t = types[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        if (t == 0) {
            err = "Camino ingresa a una muralla en (" + std::to_string(i) + "," + std::to_string(j) + ")";
            return false;
        } else if (t == 1) {
            score += values[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        } else if (t == -1) {
            --lives;
            score -= static_cast<long long>(p);
        } else {
            err = "Tipo de celda inválido en (" + std::to_string(i) + "," + std::to_string(j) + ")";
            return false;
        }
        if (lives < 1) {
            err = "Vidas llegaron a 0 en (" + std::to_string(i) + "," + std::to_string(j) + ")";
            return false;
        }
    }

    if (i != n - 1 || j != m - 1) {
        err = "Camino no termina en (N-1, M-1)";
        return false;
    }
    if (score != res.score) {
        err = "Puntaje recomputado (" + std::to_string(score) +
              ") difiere del reportado (" + std::to_string(res.score) + ")";
        return false;
    }
    if (lives != res.remaining_lives) {
        err = "Vidas recomputadas (" + std::to_string(lives) +
              ") difieren de las reportadas (" + std::to_string(res.remaining_lives) + ")";
        return false;
    }

    return true;
}

static bool writeOutputFile(const fs::path& filepath, const PathResult& res) {
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (res.found) {
        out << res.score << '\n' << res.path << '\n' << res.remaining_lives << '\n';
    } else {
        out << "SIN SOLUCION\n";
    }
    return out.good();
}

static bool parsePositiveInt(const std::string& s, int& out) {
    if (s.empty()) {
        return false;
    }
    long long acc = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        acc = acc * 10 + (c - '0');
        if (acc > std::numeric_limits<int>::max()) {
            return false;
        }
    }
    if (acc <= 0) {
        return false;
    }
    out = static_cast<int>(acc);
    return true;
}

static int gridTypeRank(const std::string& t) {
    if (t == "libre") return 0;
    if (t == "murallas") return 1;
    if (t == "minas") return 2;
    if (t == "mixta") return 3;
    return 4;
}

static int domainRank(const std::string& d) {
    if (d.size() >= 2 && d[0] == 'D') {
        int val = 0;
        if (parsePositiveInt(d.substr(1), val)) {
            return val;
        }
    }
    return std::numeric_limits<int>::max();
}

static int algRank(const std::string& a) {
    if (a == "bruteforce") return 0;
    if (a == "dp") return 1;
    if (a == "greedy") return 2;
    return 3;
}

static bool parseInstanceMeta(const fs::path& filepath, InstanceMeta& meta) {
    meta = InstanceMeta{};
    meta.filepath = filepath;
    meta.instance = filepath.stem().string();

    std::vector<std::string> parts;
    std::stringstream ss(meta.instance);
    std::string tok;
    while (std::getline(ss, tok, '_')) {
        parts.push_back(tok);
    }

    if (parts.size() == 5) {
        int fn = 0, fm = 0, dom_val = 0;
        const bool valid_type = (parts[2] == "libre" || parts[2] == "murallas" ||
                                 parts[2] == "minas" || parts[2] == "mixta");
        const bool valid_dom = (parts[3].size() >= 2 && parts[3][0] == 'D' &&
                                parsePositiveInt(parts[3].substr(1), dom_val));
        if (parsePositiveInt(parts[0], fn) && parsePositiveInt(parts[1], fm) &&
            valid_type && valid_dom && !parts[4].empty()) {
            meta.n = fn;
            meta.m = fm;
            meta.grid_type = parts[2];
            meta.domain = parts[3];
            meta.sample = parts[4];
            meta.canonical = true;
            return true;
        }
    }

    // Respaldo seguro para nombres arbitrarios (ej. pruebas con --input-dir)
    int hn = 0, hm = 0, hv = 0, hp = 0;
    if (!loadGridHeader(filepath, hn, hm, hv, hp)) {
        return false;
    }
    meta.n = hn;
    meta.m = hm;
    meta.v = hv;
    meta.p = hp;
    meta.grid_type = "custom";
    meta.domain = "D" + std::to_string(hp);
    meta.sample = meta.instance;
    meta.canonical = false;
    return true;
}

static double computeMedian(std::vector<double> vals) {
    if (vals.empty()) {
        return 0.0;
    }
    std::sort(vals.begin(), vals.end());
    const std::size_t n = vals.size();
    if (n % 2 == 1) {
        return vals[n / 2];
    }
    return 0.5 * (vals[n / 2 - 1] + vals[n / 2]);
}

template <typename SolverFn>
static BenchStats benchmarkAlgorithm(const std::string& alg,
                                     int n, int m,
                                     SolverFn&& solver) {
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::duration<double, std::milli>;

    const bool is_bf = (alg == "bruteforce");
    const long long nm = static_cast<long long>(n) * static_cast<long long>(m);

    // 1) Llamada dedicada NO cronometrada para medición exacta de memoria auxiliar
    std::size_t peak_heap = 0;
    std::size_t bf_stack = 0;
    {
        mem_tracker::ScopedMemTracker tracker;
        PathResult tmp = solver();
        peak_heap = tracker.peak_bytes();
        if (is_bf) {
            bf_stack = getLastBruteForceStackBytes();
        }
        (void)tmp;
    } // tmp se destruye antes que tracker -> g_count vuelve a 0 antes de medir tiempo

    // 2) Esquema de repeticiones adaptativas
    int reps = 1;
    if (alg == "greedy") {
        reps = 30;
    } else if (alg == "dp") {
        if (nm <= 4096) {
            reps = 30;
        } else if (nm <= 131072) {
            reps = 10;
        } else {
            reps = 5;
        }
    } else { // bruteforce
        if (n + m <= 16) {
            reps = 10;
        } else if (n + m <= 24) {
            reps = 3;
        } else {
            reps = 1;
        }
    }

    // 3) Warmup + selección de micro-batch B para llamadas < 50 us
    int batch = 1;
    PathResult final_res{};
    bool have_res = false;

    if (!(is_bf && n + m >= 32)) {
        const auto w0 = Clock::now();
        final_res = solver();
        const auto w1 = Clock::now();
        have_res = true;
        const double w_ms = Ms(w1 - w0).count();
        if (reps > 1) {
            if (w_ms < 0.005) {
                batch = 100;
            } else if (w_ms < 0.05) {
                batch = 10;
            }
        }
    }

    // 4) Repeticiones cronometradas (sin sobrecarga del tracker)
    volatile long long sink = 0;
    std::vector<double> samples(static_cast<std::size_t>(reps), 0.0);

    for (int r = 0; r < reps; ++r) {
        const auto t0 = Clock::now();
        for (int b = 0; b < batch; ++b) {
            PathResult cur = solver();
            sink ^= cur.score ^ static_cast<long long>(cur.remaining_lives) ^
                    static_cast<long long>(cur.path.size());
            if (!have_res && r == 0 && b == 0) {
                final_res = std::move(cur);
                have_res = true;
            }
        }
        const auto t1 = Clock::now();
        samples[static_cast<std::size_t>(r)] = Ms(t1 - t0).count() / static_cast<double>(batch);
    }
    (void)sink;

    double sum = 0.0;
    for (double x : samples) {
        sum += x;
    }
    const double mean = sum / static_cast<double>(reps);

    double var_sum = 0.0;
    if (reps > 1) {
        for (double x : samples) {
            const double d = x - mean;
            var_sum += d * d;
        }
    }
    const double stddev = (reps > 1) ? std::sqrt(var_sum / static_cast<double>(reps - 1)) : 0.0;

    BenchStats st;
    st.result = std::move(final_res);
    st.reps = reps;
    st.time_median_ms = computeMedian(samples);
    st.time_mean_ms = mean;
    st.time_stddev_ms = stddev;
    st.aux_memory_bytes = peak_heap + bf_stack;
    return st;
}

static bool writeMeasurementsCSV(const fs::path& filepath,
                                 const std::vector<MeasurementRow>& rows) {
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << "instance,n,m,nm,v,p,grid_type,domain,sample,algorithm,found,score,"
           "remaining_lives,path_len,reps,time_median_ms,time_mean_ms,"
           "time_stddev_ms,aux_memory_bytes,dp_score,optimality_gap,greedy_failed\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& r : rows) {
        out << r.instance << ','
            << r.n << ','
            << r.m << ','
            << r.nm << ','
            << r.v << ','
            << r.p << ','
            << r.grid_type << ','
            << r.domain << ','
            << r.sample << ','
            << r.algorithm << ','
            << (r.found ? 1 : 0) << ',';

        if (r.found) {
            out << r.score << ',' << r.remaining_lives << ',' << r.path_len << ',';
        } else {
            out << ",,,";
        }

        out << r.reps << ','
            << r.time_median_ms << ','
            << r.time_mean_ms << ','
            << r.time_stddev_ms << ','
            << r.aux_memory_bytes << ',';

        if (r.has_dp_score) {
            out << r.dp_score;
        }
        out << ',';

        if (r.has_gap) {
            out << r.optimality_gap;
        }
        out << ',' << r.greedy_failed << '\n';
    }
    return out.good();
}

struct SummaryKey {
    int n = 0;
    int m = 0;
    std::string grid_type;
    std::string domain;
    std::string algorithm;

    bool operator<(const SummaryKey& o) const {
        return std::make_tuple(n, m, gridTypeRank(grid_type), grid_type,
                               domainRank(domain), domain, algRank(algorithm), algorithm) <
               std::make_tuple(o.n, o.m, gridTypeRank(o.grid_type), o.grid_type,
                               domainRank(o.domain), o.domain, algRank(o.algorithm), o.algorithm);
    }
};

static bool writeSummaryCSV(const fs::path& filepath,
                            const std::vector<MeasurementRow>& rows) {
    std::map<SummaryKey, std::vector<const MeasurementRow*>> groups;
    for (const auto& r : rows) {
        SummaryKey key{r.n, r.m, r.grid_type, r.domain, r.algorithm};
        groups[key].push_back(&r);
    }

    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << "n,m,nm,grid_type,domain,algorithm,instances,found_count,fail_rate,"
           "time_median_of_medians_ms,time_mean_ms,aux_memory_mean_bytes,"
           "gap_mean,gap_median,gap_max\n";

    for (const auto& kv : groups) {
        const SummaryKey& k = kv.first;
        const auto& vec = kv.second;
        const std::size_t inst_count = vec.size();
        std::size_t found_count = 0;
        std::vector<double> medians;
        medians.reserve(inst_count);
        double time_mean_sum = 0.0;
        double mem_sum = 0.0;
        std::vector<double> gaps;
        long long gap_max = 0;

        for (const MeasurementRow* r : vec) {
            if (r->found) {
                ++found_count;
            }
            medians.push_back(r->time_median_ms);
            time_mean_sum += r->time_mean_ms;
            mem_sum += static_cast<double>(r->aux_memory_bytes);
            if (r->has_gap) {
                if (gaps.empty() || r->optimality_gap > gap_max) {
                    gap_max = r->optimality_gap;
                }
                gaps.push_back(static_cast<double>(r->optimality_gap));
            }
        }

        const double fail_rate =
            static_cast<double>(inst_count - found_count) / static_cast<double>(inst_count);
        const double med_of_meds = computeMedian(medians);
        const double time_mean = time_mean_sum / static_cast<double>(inst_count);
        const double mem_mean = mem_sum / static_cast<double>(inst_count);
        const long long nm = static_cast<long long>(k.n) * static_cast<long long>(k.m);

        out << k.n << ','
            << k.m << ','
            << nm << ','
            << k.grid_type << ','
            << k.domain << ','
            << k.algorithm << ','
            << inst_count << ','
            << found_count << ','
            << std::fixed << std::setprecision(6) << fail_rate << ','
            << med_of_meds << ','
            << time_mean << ','
            << std::setprecision(2) << mem_mean << ',';

        if (!gaps.empty()) {
            double gsum = 0.0;
            for (double g : gaps) {
                gsum += g;
            }
            const double gmean = gsum / static_cast<double>(gaps.size());
            const double gmed = computeMedian(gaps);
            out << std::setprecision(6) << gmean << ',' << gmed << ',' << gap_max;
        } else {
            out << ",,";
        }
        out << '\n';
    }

    return out.good();
}

// ============================================================================
// Auto-pruebas rigurosas (--self-test): Figura 1 + casos borde + memoria
// ============================================================================
static bool runSelfTest(bool verbose) {
    auto fail = [](const std::string& msg) -> bool {
        std::cerr << "[SELF-TEST FAIL] " << msg << '\n';
        return false;
    };

    auto checkValid = [&](int n, int m, int v, int p,
                          const std::vector<std::vector<int>>& t,
                          const std::vector<std::vector<long long>>& g,
                          const PathResult& r,
                          const std::string& label) -> bool {
        std::string err;
        if (!validatePath(n, m, v, p, t, g, r, err)) {
            return fail(label + ": " + err);
        }
        return true;
    };

    // Caso 1: Figura 1 del enunciado (4x5, V=2, P=5)
    {
        const int n = 4, m = 5, v = 2, p = 5;
        const std::vector<std::vector<int>> types = {
            {1, -1, 1, 1, 1},
            {1,  1, 0, 1, 1},
            {1,  1, 1, 1, 1},
            {0,  1, 1, 1, 1}
        };
        const std::vector<std::vector<long long>> values = {
            { 1,  0,  8,  8, 8},
            { 2,  3,  0, -2, 6},
            {-3,  4, -2, -3, 2},
            { 0, -4,  1,  2, 1}
        };

        const PathResult bf = solveBruteForce(n, m, v, p, types, values);
        const PathResult dp = solveDP(n, m, v, p, types, values);
        const PathResult gr = solveGreedy(n, m, v, p, types, values);

        if (!checkValid(n, m, v, p, types, values, bf, "Fig1 BF")) return false;
        if (!checkValid(n, m, v, p, types, values, dp, "Fig1 DP")) return false;
        if (!checkValid(n, m, v, p, types, values, gr, "Fig1 Greedy")) return false;

        if (!bf.found || bf.score != 29 || bf.path != "RRRRDDD" || bf.remaining_lives != 1) {
            return fail("Figura 1 BF esperado (29, RRRRDDD, 1)");
        }
        if (!dp.found || dp.score != 29 || dp.path != "RRRRDDD" || dp.remaining_lives != 1) {
            return fail("Figura 1 DP esperado (29, RRRRDDD, 1)");
        }
        if (!gr.found || gr.score != 12 || gr.path != "DRDRDRR" || gr.remaining_lives != 2) {
            return fail("Figura 1 Greedy esperado (12, DRDRDRR, 2)");
        }

        // Verificación del rastreador de memoria auxiliar sobre Figura 1
        std::size_t dp_mem = 0, gr_mem = 0, bf_mem = 0;
        {
            mem_tracker::ScopedMemTracker tr;
            PathResult tmp = solveDP(n, m, v, p, types, values);
            dp_mem = tr.peak_bytes();
            (void)tmp;
        }
        {
            mem_tracker::ScopedMemTracker tr;
            PathResult tmp = solveGreedy(n, m, v, p, types, values);
            gr_mem = tr.peak_bytes();
            (void)tmp;
        }
        {
            mem_tracker::ScopedMemTracker tr;
            PathResult tmp = solveBruteForce(n, m, v, p, types, values);
            bf_mem = tr.peak_bytes() + getLastBruteForceStackBytes();
            (void)tmp;
        }
        if (!(dp_mem > gr_mem && gr_mem > 0 && bf_mem > 0)) {
            return fail("ScopedMemTracker falló invariante dp_mem > gr_mem > 0 y bf_mem > 0");
        }
    }

    // Caso 2: Variante Figura 1 con V=1, P=5 (mina bloqueada -> OPT = Greedy = 12)
    {
        const int n = 4, m = 5, v = 1, p = 5;
        const std::vector<std::vector<int>> types = {
            {1, -1, 1, 1, 1},
            {1,  1, 0, 1, 1},
            {1,  1, 1, 1, 1},
            {0,  1, 1, 1, 1}
        };
        const std::vector<std::vector<long long>> values = {
            { 1,  0,  8,  8, 8},
            { 2,  3,  0, -2, 6},
            {-3,  4, -2, -3, 2},
            { 0, -4,  1,  2, 1}
        };

        const PathResult bf = solveBruteForce(n, m, v, p, types, values);
        const PathResult dp = solveDP(n, m, v, p, types, values);
        const PathResult gr = solveGreedy(n, m, v, p, types, values);

        if (!checkValid(n, m, v, p, types, values, bf, "Fig1-V1 BF")) return false;
        if (!checkValid(n, m, v, p, types, values, dp, "Fig1-V1 DP")) return false;
        if (!checkValid(n, m, v, p, types, values, gr, "Fig1-V1 Greedy")) return false;

        if (!bf.found || bf.score != 12 || bf.remaining_lives != 1) {
            return fail("Figura 1 V=1 BF esperado (12, lives=1)");
        }
        if (!dp.found || dp.score != 12 || dp.remaining_lives != 1) {
            return fail("Figura 1 V=1 DP esperado (12, lives=1)");
        }
        if (!gr.found || gr.score != 12 || gr.path != "DRDRDRR" || gr.remaining_lives != 1) {
            return fail("Figura 1 V=1 Greedy esperado (12, DRDRDRR, lives=1)");
        }
    }

    // Caso 3: Grillas 1x1 (positiva y negativa) y v <= 0
    {
        const std::vector<std::vector<int>> t1 = {{1}};
        const std::vector<std::vector<long long>> g_pos = {{7}};
        const std::vector<std::vector<long long>> g_neg = {{-9}};

        for (int sign = 0; sign < 2; ++sign) {
            const auto& g = (sign == 0) ? g_pos : g_neg;
            const long long expected = (sign == 0) ? 7 : -9;
            const PathResult bf = solveBruteForce(1, 1, 3, 5, t1, g);
            const PathResult dp = solveDP(1, 1, 3, 5, t1, g);
            const PathResult gr = solveGreedy(1, 1, 3, 5, t1, g);
            if (!checkValid(1, 1, 3, 5, t1, g, bf, "1x1 BF")) return false;
            if (!checkValid(1, 1, 3, 5, t1, g, dp, "1x1 DP")) return false;
            if (!checkValid(1, 1, 3, 5, t1, g, gr, "1x1 Greedy")) return false;
            if (!bf.found || bf.score != expected || !bf.path.empty() || bf.remaining_lives != 3 ||
                !dp.found || dp.score != expected || !dp.path.empty() || dp.remaining_lives != 3 ||
                !gr.found || gr.score != expected || !gr.path.empty() || gr.remaining_lives != 3) {
                return fail("Caso 1x1 falló");
            }
        }

        if (solveBruteForce(1, 1, 0, 5, t1, g_pos).found ||
            solveDP(1, 1, 0, 5, t1, g_pos).found ||
            solveGreedy(1, 1, 0, 5, t1, g_pos).found) {
            return fail("Caso v <= 0 debe retornar found=false");
        }
    }

    // Caso 4: Grilla completamente negativa (3x3)
    {
        const std::vector<std::vector<int>> t = {
            {1, 1, 1},
            {1, 1, 1},
            {1, 1, 1}
        };
        const std::vector<std::vector<long long>> g = {
            {-10, -20, -30},
            { -5, -10, -20},
            {-15,  -5, -10}
        };
        const PathResult bf = solveBruteForce(3, 3, 3, 10, t, g);
        const PathResult dp = solveDP(3, 3, 3, 10, t, g);
        const PathResult gr = solveGreedy(3, 3, 3, 10, t, g);
        if (!checkValid(3, 3, 3, 10, t, g, bf, "AllNeg BF")) return false;
        if (!checkValid(3, 3, 3, 10, t, g, dp, "AllNeg DP")) return false;
        if (!checkValid(3, 3, 3, 10, t, g, gr, "AllNeg Greedy")) return false;
        if (!bf.found || !dp.found || !gr.found || bf.score != dp.score || bf.score != -40) {
            return fail("Caso grilla totalmente negativa falló (esperado -40)");
        }
    }

    // Caso 5: Grillas sin solución (murallas y minas con V=1)
    {
        const std::vector<std::vector<int>> t_wall = {
            {1, 0, 1},
            {0, 1, 1},
            {1, 1, 1}
        };
        const std::vector<std::vector<long long>> g_wall = {
            {5, 0, 5},
            {0, 5, 5},
            {5, 5, 5}
        };
        if (solveBruteForce(3, 3, 3, 5, t_wall, g_wall).found ||
            solveDP(3, 3, 3, 5, t_wall, g_wall).found ||
            solveGreedy(3, 3, 3, 5, t_wall, g_wall).found) {
            return fail("Caso sin solución por murallas falló");
        }

        const std::vector<std::vector<int>> t_mine = {
            { 1, -1},
            {-1,  1}
        };
        const std::vector<std::vector<long long>> g_mine = {
            {1, 0},
            {0, 1}
        };
        if (solveBruteForce(2, 2, 1, 5, t_mine, g_mine).found ||
            solveDP(2, 2, 1, 5, t_mine, g_mine).found ||
            solveGreedy(2, 2, 1, 5, t_mine, g_mine).found) {
            return fail("Caso sin solución por minas (V=1) falló");
        }
    }

    // Caso 6: Desempate estricto de Greedy (R sobre D incluso cuando R es mina -P y D es normal -P)
    // y exclusión de minas cuando v == 1
    {
        const std::vector<std::vector<int>> t = {
            {1, -1},
            {1,  1}
        };
        const std::vector<std::vector<long long>> g = {
            { 0, 999999}, // valor dummy en mina DEBE ser ignorado
            {-5,      1}
        };
        // Con V=2, P=5: R es mina (-5) y D es normal (-5) -> empate exacto -> Greedy DEBE elegir R
        const PathResult gr_tie = solveGreedy(2, 2, 2, 5, t, g);
        if (!checkValid(2, 2, 2, 5, t, g, gr_tie, "Greedy tie R vs D")) return false;
        if (!gr_tie.found || gr_tie.path != "RD" || gr_tie.score != -4 || gr_tie.remaining_lives != 1) {
            return fail("Desempate estricto Greedy R-sobre-D (mina vs normal) falló");
        }

        // Con V=1, P=5: R es mina excluida por v==1 -> Greedy DEBE ir por D (normal -100)
        const std::vector<std::vector<long long>> g2 = {
            {   0, 999999},
            {-100,      1}
        };
        const PathResult gr_v1 = solveGreedy(2, 2, 1, 5, t, g2);
        const PathResult bf_v1 = solveBruteForce(2, 2, 1, 5, t, g2);
        const PathResult dp_v1 = solveDP(2, 2, 1, 5, t, g2);
        if (!checkValid(2, 2, 1, 5, t, g2, gr_v1, "Greedy V=1")) return false;
        if (!gr_v1.found || gr_v1.path != "DR" || gr_v1.score != -99 || gr_v1.remaining_lives != 1 ||
            bf_v1.score != -99 || dp_v1.score != -99) {
            return fail("Evitación de mina en V=1 o ignorar values en mina falló");
        }

        // Con V=2, P=5 y g2: mina R (-5) mejor que D (-100) -> BF, DP y Greedy ignoran 999999 y obtienen -4
        const PathResult bf_ign = solveBruteForce(2, 2, 2, 5, t, g2);
        const PathResult dp_ign = solveDP(2, 2, 2, 5, t, g2);
        const PathResult gr_ign = solveGreedy(2, 2, 2, 5, t, g2);
        if (!checkValid(2, 2, 2, 5, t, g2, bf_ign, "IgnoreMine BF")) return false;
        if (!checkValid(2, 2, 2, 5, t, g2, dp_ign, "IgnoreMine DP")) return false;
        if (!checkValid(2, 2, 2, 5, t, g2, gr_ign, "IgnoreMine Greedy")) return false;
        if (bf_ign.score != -4 || dp_ign.score != -4 || gr_ign.score != -4) {
            return fail("Algoritmos no ignoraron values[i][j] en celda mina");
        }
    }

    if (verbose) {
        std::cout << "[SELF-TEST PASS] Figura 1 (V=2 y V=1), 1x1, v<=0, todo-negativo, "
                     "sin solucion, desempate R>D, minas V=1, valores dummy en minas "
                     "y ScopedMemTracker verificados correctamente.\n";
    }
    return true;
}

// ============================================================================
// Modo archivo individual (--input)
// ============================================================================
static int runSingleInput(const fs::path& input_file,
                          const std::string& alg_opt,
                          const fs::path& output_dir,
                          bool force_bf) {
    GridData grid;
    std::string err;
    if (!loadGrid(input_file, grid, err)) {
        std::cerr << "[ERROR] " << err << '\n';
        return 1;
    }

    const bool want_bf = (alg_opt == "bruteforce" || alg_opt == "all");
    const bool want_dp = (alg_opt == "dp" || alg_opt == "all");
    const bool want_gr = (alg_opt == "greedy" || alg_opt == "all");

    if (!want_bf && !want_dp && !want_gr) {
        std::cerr << "[ERROR] Algoritmo desconocido: " << alg_opt
                  << " (use bruteforce|dp|greedy|all)\n";
        return 1;
    }

    bool run_bf = false;
    if (want_bf) {
        if (grid.n + grid.m <= 32 || force_bf) {
            run_bf = true;
        } else if (alg_opt == "bruteforce") {
            std::cerr << "[ERROR] Brute Force requiere N + M <= 32 (actual: "
                      << (grid.n + grid.m) << "). Use --force-bf para forzar.\n";
            return 1;
        }
    }

    if (!output_dir.empty()) {
        std::error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec) {
            std::cerr << "[ERROR] No se pudo crear directorio de salida: " << output_dir << '\n';
            return 1;
        }
    }

    const std::string stem = input_file.stem().string();
    BenchStats bf_st{}, dp_st{}, gr_st{};

    auto emitLine = [](const std::string& alg, const BenchStats& st) {
        std::cout << "ALG=" << alg
                  << " FOUND=" << (st.result.found ? 1 : 0)
                  << " SCORE=" << (st.result.found ? std::to_string(st.result.score) : "NA")
                  << " LIVES=" << (st.result.found ? std::to_string(st.result.remaining_lives) : "NA")
                  << " PATH=" << (st.result.found ? st.result.path : "SIN_SOLUCION")
                  << " TIME_MS=" << std::fixed << std::setprecision(6) << st.time_median_ms
                  << " AUX_MEM_BYTES=" << st.aux_memory_bytes << '\n';
    };

    if (run_bf) {
        bf_st = benchmarkAlgorithm("bruteforce", grid.n, grid.m, [&]() {
            return solveBruteForce(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
        });
        if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values, bf_st.result, err)) {
            std::cerr << "[ERROR] Validación BF falló: " << err << '\n';
            return 1;
        }
        emitLine("bruteforce", bf_st);
        if (!output_dir.empty()) {
            const fs::path out_p = output_dir / (stem + "_bruteforce_out.txt");
            if (!writeOutputFile(out_p, bf_st.result)) {
                std::cerr << "[ERROR] No se pudo escribir: " << out_p << '\n';
                return 1;
            }
        }
    }

    if (want_dp) {
        dp_st = benchmarkAlgorithm("dp", grid.n, grid.m, [&]() {
            return solveDP(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
        });
        if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values, dp_st.result, err)) {
            std::cerr << "[ERROR] Validación DP falló: " << err << '\n';
            return 1;
        }
        emitLine("dp", dp_st);
        if (!output_dir.empty()) {
            const fs::path out_p = output_dir / (stem + "_dp_out.txt");
            if (!writeOutputFile(out_p, dp_st.result)) {
                std::cerr << "[ERROR] No se pudo escribir: " << out_p << '\n';
                return 1;
            }
        }
    }

    if (run_bf && want_dp) {
        if (bf_st.result.found != dp_st.result.found ||
            (bf_st.result.found && bf_st.result.score != dp_st.result.score)) {
            std::cerr << "[ERROR] Discrepancia BF vs DP en " << input_file.string() << '\n';
            return 1;
        }
    }

    if (want_gr) {
        gr_st = benchmarkAlgorithm("greedy", grid.n, grid.m, [&]() {
            return solveGreedy(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
        });
        if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values, gr_st.result, err)) {
            std::cerr << "[ERROR] Validación Greedy falló: " << err << '\n';
            return 1;
        }
        emitLine("greedy", gr_st);
        if (!output_dir.empty()) {
            const fs::path out_p = output_dir / (stem + "_greedy_out.txt");
            if (!writeOutputFile(out_p, gr_st.result)) {
                std::cerr << "[ERROR] No se pudo escribir: " << out_p << '\n';
                return 1;
            }
        }
    }

    return 0;
}

// ============================================================================
// Modo lote completo (defecto y --input-dir / --output-dir / --measurements-dir)
// ============================================================================
static int runBatch(const fs::path& input_dir,
                    const fs::path& output_dir,
                    const fs::path& measurements_dir,
                    bool force_bf) {
    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
        std::cerr << "[ERROR] Directorio de entrada no existe: " << input_dir << '\n';
        return 1;
    }

    std::error_code ec;
    fs::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "[ERROR] No se pudo crear directorio de salida: " << output_dir << '\n';
        return 1;
    }
    fs::create_directories(measurements_dir, ec);
    if (ec) {
        std::cerr << "[ERROR] No se pudo crear directorio de mediciones: " << measurements_dir << '\n';
        return 1;
    }

    std::vector<InstanceMeta> instances;
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".txt") {
            continue;
        }
        InstanceMeta meta;
        if (!parseInstanceMeta(entry.path(), meta)) {
            std::cerr << "[ERROR] No se pudo interpretar metadatos de: " << entry.path() << '\n';
            return 1;
        }
        instances.push_back(std::move(meta));
    }

    if (instances.empty()) {
        std::cerr << "[ERROR] No se encontraron archivos .txt en: " << input_dir << '\n';
        return 1;
    }

    std::sort(instances.begin(), instances.end(),
              [](const InstanceMeta& a, const InstanceMeta& b) {
                  return std::make_tuple(a.n, a.m, gridTypeRank(a.grid_type), a.grid_type,
                                         domainRank(a.domain), a.domain, a.sample, a.instance) <
                         std::make_tuple(b.n, b.m, gridTypeRank(b.grid_type), b.grid_type,
                                         domainRank(b.domain), b.domain, b.sample, b.instance);
              });

    std::vector<MeasurementRow> rows;
    rows.reserve(instances.size() * 3);

    int bf_checked = 0;
    int bf_dp_matches = 0;
    int total_outputs = 0;

    const auto batch_start = std::chrono::steady_clock::now();

    for (std::size_t idx = 0; idx < instances.size(); ++idx) {
        const InstanceMeta& meta = instances[idx];
        GridData grid;
        std::string err;
        if (!loadGrid(meta.filepath, grid, err)) {
            std::cerr << "[ERROR] " << err << '\n';
            return 1;
        }

        if (meta.canonical && (meta.n != grid.n || meta.m != grid.m)) {
            std::cerr << "[ERROR] Dimensiones del nombre no coinciden con encabezado en: "
                      << meta.filepath << '\n';
            return 1;
        }

        const bool run_bf = (grid.n + grid.m <= 32) || force_bf;
        BenchStats bf_st{}, dp_st{}, gr_st{};

        if (run_bf) {
            bf_st = benchmarkAlgorithm("bruteforce", grid.n, grid.m, [&]() {
                return solveBruteForce(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
            });
            if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values,
                              bf_st.result, err)) {
                std::cerr << "[ERROR] Camino BF inválido en " << meta.instance << ": " << err << '\n';
                return 1;
            }
            const fs::path out_p = output_dir / (meta.instance + "_bruteforce_out.txt");
            if (!writeOutputFile(out_p, bf_st.result)) {
                std::cerr << "[ERROR] Error escribiendo: " << out_p << '\n';
                return 1;
            }
            ++total_outputs;
        }

        dp_st = benchmarkAlgorithm("dp", grid.n, grid.m, [&]() {
            return solveDP(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
        });
        if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values,
                          dp_st.result, err)) {
            std::cerr << "[ERROR] Camino DP inválido en " << meta.instance << ": " << err << '\n';
            return 1;
        }
        {
            const fs::path out_p = output_dir / (meta.instance + "_dp_out.txt");
            if (!writeOutputFile(out_p, dp_st.result)) {
                std::cerr << "[ERROR] Error escribiendo: " << out_p << '\n';
                return 1;
            }
            ++total_outputs;
        }

        if (run_bf) {
            ++bf_checked;
            if (bf_st.result.found != dp_st.result.found ||
                (bf_st.result.found && bf_st.result.score != dp_st.result.score)) {
                std::cerr << "[ERROR] Discrepancia BF vs DP en " << meta.instance
                          << " (BF found=" << bf_st.result.found << " score=" << bf_st.result.score
                          << ", DP found=" << dp_st.result.found << " score=" << dp_st.result.score
                          << ")\n";
                return 1;
            }
            ++bf_dp_matches;
        }

        gr_st = benchmarkAlgorithm("greedy", grid.n, grid.m, [&]() {
            return solveGreedy(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values);
        });
        if (!validatePath(grid.n, grid.m, grid.v, grid.p, grid.types, grid.values,
                          gr_st.result, err)) {
            std::cerr << "[ERROR] Camino Greedy inválido en " << meta.instance << ": " << err << '\n';
            return 1;
        }
        {
            const fs::path out_p = output_dir / (meta.instance + "_greedy_out.txt");
            if (!writeOutputFile(out_p, gr_st.result)) {
                std::cerr << "[ERROR] Error escribiendo: " << out_p << '\n';
                return 1;
            }
            ++total_outputs;
        }

        // Verificación de optimalidad: Greedy nunca puede superar al óptimo DP
        if (gr_st.result.found && dp_st.result.found && gr_st.result.score > dp_st.result.score) {
            std::cerr << "[ERROR] Greedy superó a DP en " << meta.instance
                      << " (Greedy=" << gr_st.result.score
                      << ", DP=" << dp_st.result.score << ")\n";
            return 1;
        }

        auto makeRow = [&](const std::string& alg, const BenchStats& st) -> MeasurementRow {
            MeasurementRow r;
            r.instance = meta.instance;
            r.n = grid.n;
            r.m = grid.m;
            r.nm = static_cast<long long>(grid.n) * static_cast<long long>(grid.m);
            r.v = grid.v;
            r.p = grid.p;
            r.grid_type = meta.grid_type;
            r.domain = meta.domain;
            r.sample = meta.sample;
            r.algorithm = alg;
            r.found = st.result.found;
            r.score = st.result.score;
            r.remaining_lives = st.result.remaining_lives;
            r.path_len = st.result.path.size();
            r.reps = st.reps;
            r.time_median_ms = st.time_median_ms;
            r.time_mean_ms = st.time_mean_ms;
            r.time_stddev_ms = st.time_stddev_ms;
            r.aux_memory_bytes = st.aux_memory_bytes;
            r.has_dp_score = dp_st.result.found;
            r.dp_score = dp_st.result.score;

            if (r.found) {
                if (alg == "bruteforce" || alg == "dp") {
                    r.has_gap = true;
                    r.optimality_gap = 0;
                } else if (dp_st.result.found) {
                    r.has_gap = true;
                    r.optimality_gap = dp_st.result.score - r.score;
                }
            }
            r.greedy_failed = (alg == "greedy" && !r.found) ? 1 : 0;
            return r;
        };

        if (run_bf) {
            rows.push_back(makeRow("bruteforce", bf_st));
        }
        rows.push_back(makeRow("dp", dp_st));
        rows.push_back(makeRow("greedy", gr_st));

        if ((idx + 1) % 24 == 0 || idx + 1 == instances.size()) {
            const double elapsed_s =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - batch_start).count();
            std::cout << "[PROGRESO] " << (idx + 1) << "/" << instances.size()
                      << " instancias completadas (última: " << meta.instance
                      << ", t=" << std::fixed << std::setprecision(2) << elapsed_s << " s)\n"
                      << std::flush;
        }
    }

    const fs::path meas_path = measurements_dir / "measurements.csv";
    if (!writeMeasurementsCSV(meas_path, rows)) {
        std::cerr << "[ERROR] No se pudo escribir: " << meas_path << '\n';
        return 1;
    }

    const fs::path sum_path = measurements_dir / "summary_by_config.csv";
    if (!writeSummaryCSV(sum_path, rows)) {
        std::cerr << "[ERROR] No se pudo escribir: " << sum_path << '\n';
        return 1;
    }

    const double total_s =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - batch_start).count();
    std::cout << "[RESUMEN] Instancias=" << instances.size()
              << " | Salidas=" << total_outputs
              << " | FilasCSV=" << rows.size()
              << " | Coincidencias BF==DP=" << bf_dp_matches << "/" << bf_checked
              << " | Tiempo total=" << std::fixed << std::setprecision(2) << total_s << " s\n";
    return 0;
}

static void printUsage(const char* prog) {
    std::cerr << "Uso:\n"
              << "  " << prog << "                      (self-test + lote completo sobre data/)\n"
              << "  " << prog << " --self-test          (verifica Figura 1 y casos borde)\n"
              << "  " << prog << " --input <archivo.txt> [--alg bruteforce|dp|greedy|all]"
                                  " [--output-dir <dir>] [--force-bf]\n"
              << "  " << prog << " --input-dir <dir> --output-dir <dir>"
                                  " --measurements-dir <dir> [--force-bf]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        if (!runSelfTest(true)) {
            return 1;
        }
        return runBatch("data/grid_input", "data/grid_output", "data/measurements", false);
    }

    bool do_self_test = false;
    bool force_bf = false;
    std::string input_file;
    std::string alg_opt = "all";
    std::string input_dir;
    std::string output_dir;
    std::string measurements_dir;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            do_self_test = true;
        } else if (arg == "--force-bf") {
            force_bf = true;
        } else if (arg == "--input" && i + 1 < argc) {
            input_file = argv[++i];
        } else if (arg == "--alg" && i + 1 < argc) {
            alg_opt = argv[++i];
        } else if (arg == "--input-dir" && i + 1 < argc) {
            input_dir = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--measurements-dir" && i + 1 < argc) {
            measurements_dir = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "[ERROR] Argumento desconocido o incompleto: " << arg << '\n';
            printUsage(argv[0]);
            return 1;
        }
    }

    if (do_self_test && input_file.empty() && input_dir.empty()) {
        return runSelfTest(true) ? 0 : 1;
    }

    if (!input_file.empty()) {
        if (!input_dir.empty() || !measurements_dir.empty()) {
            std::cerr << "[ERROR] No combine --input con --input-dir/--measurements-dir\n";
            return 1;
        }
        return runSingleInput(input_file, alg_opt, output_dir, force_bf);
    }

    if (!input_dir.empty() || !output_dir.empty() || !measurements_dir.empty()) {
        if (input_dir.empty() || output_dir.empty() || measurements_dir.empty()) {
            std::cerr << "[ERROR] El modo por directorios requiere --input-dir, "
                         "--output-dir y --measurements-dir\n";
            return 1;
        }
        if (do_self_test && !runSelfTest(true)) {
            return 1;
        }
        return runBatch(input_dir, output_dir, measurements_dir, force_bf);
    }

    printUsage(argv[0]);
    return 1;
}
