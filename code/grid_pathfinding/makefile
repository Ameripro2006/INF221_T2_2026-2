# ==============================================================================
# Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2)
# Estudiante: Americo Ignacio Acuña Rodriguez | Rol USM: 202473018-5 | Campus: Casa Central
# Archivo: code/grid_pathfinding/makefile
#
# Referencias bibliográficas consultadas:
#   - Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2022).
#     Introduction to Algorithms (4th ed.). MIT Press.
#   - Erickson, J. (2019). Algorithms. University of Illinois.
#   - Enunciado Tarea 2 — INF-221: Algoritmos y Complejidad (2026-2).
# ==============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2

TARGET = grid_pathfinding
SRCS   = grid_pathfinding.cpp \
         algorithms/brute_force.cpp \
         algorithms/dynamic_programming.cpp \
         algorithms/greedy.cpp
OBJS   = $(SRCS:.cpp=.o)
HDRS   = algorithms/path_result.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	bash run_all.sh

clean:
	rm -f $(TARGET) $(OBJS)
