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
 *   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2).
 *
 * Uso:
 *   ./grid_pathfinding <ruta_archivo.txt> <algoritmo>
 *   donde <algoritmo> es uno de: bruteforce, dp, greedy.
 * ============================================================================
 */

#include "algorithms/path_result.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

static void trimTrailingCR(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

static bool parseLongLongStrict(const std::string& texto, long long& valor) {
    try {
        std::size_t procesados = 0;
        long long res = std::stoll(texto, &procesados);
        if (procesados != texto.size()) {
            return false;
        }
        valor = res;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <ruta_archivo.txt> <bruteforce|dp|greedy>\n";
        return 1;
    }

    std::string ruta_archivo = argv[1];
    std::string algoritmo = argv[2];

    if (algoritmo != "bruteforce" && algoritmo != "dp" && algoritmo != "greedy") {
        std::cerr << "Error: algoritmo invalido '" << algoritmo
                  << "'. Debe ser bruteforce, dp o greedy.\n";
        return 1;
    }

    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        std::cerr << "Error: no se pudo abrir el archivo '" << ruta_archivo << "'.\n";
        return 1;
    }

    int n = 0;
    int m = 0;
    int v = 0;
    int p = 0;
    if (!(archivo >> n >> m >> v >> p)) {
        std::cerr << "Error: encabezado invalido en '" << ruta_archivo << "'.\n";
        return 1;
    }

    // Restriccion para Fuerza Bruta: terminar silenciosamente con codigo 0 si N + M > 32
    if (algoritmo == "bruteforce" && n + m > 32) {
        return 0;
    }

    if (n <= 0 || m <= 0) {
        std::cerr << "Error: dimensiones invalidas (" << n << " x " << m << ") en '"
                  << ruta_archivo << "'.\n";
        return 1;
    }

    std::vector<std::vector<int>> types(n, std::vector<int>(m));
    std::vector<std::vector<long long>> values(n, std::vector<long long>(m));

    // Primero se lee la matriz de tipos de terreno (1=normal, 0=muralla, -1=mina)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (!(archivo >> types[i][j])) {
                std::cerr << "Error: fallo al leer la matriz de tipos en '"
                          << ruta_archivo << "'.\n";
                return 1;
            }
        }
    }

    // Luego se lee la matriz de valores cientificos
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (!(archivo >> values[i][j])) {
                std::cerr << "Error: fallo al leer la matriz de valores en '"
                          << ruta_archivo << "'.\n";
                return 1;
            }
        }
    }
    archivo.close();

    // Parsear metadatos desde el nombre del archivo: {n}_{m}_{tipo}_{dominio}_{muestra}.txt
    std::string instance = std::filesystem::path(ruta_archivo).stem().string();
    std::vector<std::string> partes;
    std::stringstream ss(instance);
    std::string token;
    while (std::getline(ss, token, '_')) {
        partes.push_back(token);
    }

    std::string tipo = (partes.size() >= 3) ? partes[2] : "desconocido";
    std::string dominio = (partes.size() >= 4) ? partes[3] : "desconocido";
    std::string muestra = (partes.size() >= 5) ? partes[4] : "a";

    // Medicion de memoria (ru_maxrss) y tiempo promedio en 3 repeticiones fijas sin warm-up
    struct rusage usage_ini{};
    struct rusage usage_fin{};
    getrusage(RUSAGE_SELF, &usage_ini);

    const int REPETICIONES = 3;
    double total_ms = 0.0;
    PathResult res = {false, 0, "", 0};

    for (int r = 0; r < REPETICIONES; r++) {
        auto inicio = std::chrono::steady_clock::now();
        if (algoritmo == "bruteforce") {
            res = solveBruteForce(n, m, v, p, types, values);
        } else if (algoritmo == "dp") {
            res = solveDP(n, m, v, p, types, values);
        } else {
            res = solveGreedy(n, m, v, p, types, values);
        }
        auto fin = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> duracion = fin - inicio;
        total_ms += duracion.count();
    }

    getrusage(RUSAGE_SELF, &usage_fin);

    double tiempo_ms = total_ms / REPETICIONES;
    // Nota: en Linux ru_maxrss se reporta en kilobytes (KB); en macOS viene en bytes.
    // Se asume Linux (KB).
    long mem_kb = usage_fin.ru_maxrss - usage_ini.ru_maxrss;

    // Validacion con dependencia de orden (asume que dp se ejecuto primero)
    std::string dp_out_path = "data/grid_output/" + instance + "_dp_out.txt";

    if (algoritmo == "bruteforce") {
        std::ifstream dp_file(dp_out_path);
        std::string linea;
        if (!dp_file.is_open() || !std::getline(dp_file, linea)) {
            std::cerr << "Error de validacion: no se pudo abrir la salida de DP en '"
                      << dp_out_path << "'.\n";
            return 1;
        }
        trimTrailingCR(linea);
        if (linea.empty()) {
            std::cerr << "Error de validacion: primera linea vacia en '"
                      << dp_out_path << "'.\n";
            return 1;
        }

        if (linea == "SIN SOLUCION") {
            if (res.found) {
                std::cerr << "Error de validacion: bruteforce encontro solucion ("
                          << res.score << ") pero DP reporto SIN SOLUCION en '"
                          << instance << "'.\n";
                return 1;
            }
        } else {
            long long dp_score = 0;
            if (!parseLongLongStrict(linea, dp_score)) {
                std::cerr << "Error de validacion: puntaje de DP invalido ('" << linea
                          << "') en '" << dp_out_path << "'.\n";
                return 1;
            }
            if (!res.found || res.score != dp_score) {
                std::cerr << "Error de validacion: puntaje de bruteforce difiere de DP en '"
                          << instance << "' (DP=" << dp_score << ", BF="
                          << (res.found ? std::to_string(res.score) : std::string("SIN SOLUCION"))
                          << ").\n";
                return 1;
            }
        }
    } else if (algoritmo == "greedy") {
        std::ifstream dp_file(dp_out_path);
        std::string linea;
        if (dp_file.is_open() && std::getline(dp_file, linea)) {
            trimTrailingCR(linea);
            if (!linea.empty() && linea != "SIN SOLUCION") {
                long long dp_score = 0;
                if (parseLongLongStrict(linea, dp_score)) {
                    if (res.found && res.score > dp_score) {
                        std::cerr << "Error de validacion: greedy (" << res.score
                                  << ") supero al puntaje optimo de DP (" << dp_score
                                  << ") en '" << instance << "'.\n";
                        return 1;
                    }
                }
            }
        }
    }

    // Escribir archivo de salida del algoritmo
    std::filesystem::create_directories("data/grid_output");
    std::filesystem::create_directories("data/measurements");

    std::string out_path = "data/grid_output/" + instance + "_" + algoritmo + "_out.txt";
    std::ofstream out_file(out_path);
    if (!out_file.is_open()) {
        std::cerr << "Error: no se pudo escribir el archivo de salida '" << out_path << "'.\n";
        return 1;
    }

    if (res.found) {
        out_file << res.score << "\n"
                 << res.path << "\n"
                 << res.remaining_lives << "\n";
    } else {
        out_file << "SIN SOLUCION\n";
    }
    out_file.close();

    // Agregar (append) fila al archivo CSV de mediciones
    std::ofstream csv_file("data/measurements/measurements.csv", std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error: no se pudo abrir 'data/measurements/measurements.csv' para escritura.\n";
        return 1;
    }

    csv_file << instance << ","
             << n << ","
             << m << ","
             << tipo << ","
             << dominio << ","
             << muestra << ","
             << algoritmo << ","
             << (res.found ? 1 : 0) << ",";

    if (res.found) {
        csv_file << res.score << "," << res.remaining_lives << ",";
    } else {
        csv_file << ",,";
    }

    csv_file << std::fixed << std::setprecision(6) << tiempo_ms << ","
             << mem_kb << "\n";
    csv_file.close();

    return 0;
}
