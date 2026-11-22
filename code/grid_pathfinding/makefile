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
#
# Decisiones de diseño:
#   - Compilación modular con dependencias explícitas sobre algorithms/path_result.h
#     usando g++ -std=c++17 -O2 -Wall -Wextra (cero advertencias).
#   - Detección automática de sistema operativo y shell para compatibilidad directa
#     entre Linux (GNU Make) y Windows (mingw32-make en CMD o MSYS2/Git Bash).
#   - Targets estándar: all, run (lote completo), test (--self-test) y clean.
# ==============================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

SRCS := grid_pathfinding.cpp \
        algorithms/brute_force.cpp \
        algorithms/dynamic_programming.cpp \
        algorithms/greedy.cpp
OBJS := $(SRCS:.cpp=.o)
HDRS := algorithms/path_result.h

ifeq ($(OS),Windows_NT)
    TARGET := grid_pathfinding.exe
    ifneq (,$(findstring cmd,$(SHELL))$(findstring CMD,$(SHELL)))
        RUN_CMD   := $(TARGET)
        CLEAN_CMD := cmd /C "for %%f in ($(subst /,\,$(TARGET) $(OBJS))) do if exist %%f del /Q /F %%f"
    else
        RUN_CMD   := ./$(TARGET)
        CLEAN_CMD := rm -f $(TARGET) $(OBJS)
    endif
else
    TARGET    := grid_pathfinding
    RUN_CMD   := ./$(TARGET)
    CLEAN_CMD := rm -f $(TARGET) $(OBJS)
endif

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	$(RUN_CMD)

test: $(TARGET)
	$(RUN_CMD) --self-test

clean:
	$(CLEAN_CMD)
