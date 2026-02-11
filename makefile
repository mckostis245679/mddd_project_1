# Compiler
CXX = g++

# Compiler flags - Compatible with older systems
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -static-libstdc++ -static-libgcc

# Target executables
TARGET = executable
BENCHMARK_TARGET = benchmark_runner
BENCH_SAMPLE_TARGET = benchmark_sample
SERVER_TARGET = search_server

# Directories
SRC_DIR = .
OBJ_DIR = build

# Source files for main executable (exclude benchmark runner)
MAIN_SRCS = main.cpp data.cpp simple_ui.cpp
MAIN_OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(MAIN_SRCS))

# Source files for benchmark executable
BENCHMARK_SRCS = run_benchmarks.cpp
BENCHMARK_OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(BENCHMARK_SRCS))

# Source files for sample benchmark executable
BENCH_SAMPLE_SRCS = run_benchmarks_sample.cpp
BENCH_SAMPLE_OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(BENCH_SAMPLE_SRCS))

# Source files for search server
SERVER_SRCS = search_server.cpp data.cpp
SERVER_OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRCS))

# Default rule
all: $(TARGET)

# Link main executable
$(TARGET): $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link benchmark executable
$(BENCHMARK_TARGET): $(BENCHMARK_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link sample benchmark executable
$(BENCH_SAMPLE_TARGET): $(BENCH_SAMPLE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link search server executable
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build benchmark target
benchmark: $(BENCHMARK_TARGET)

# Build search server target
server: $(SERVER_TARGET)

# Build sample benchmark target
bench: $(BENCH_SAMPLE_TARGET)

# Run sample benchmark
run-bench: $(BENCH_SAMPLE_TARGET)
	@echo "Running sample benchmark..."
	./$(BENCH_SAMPLE_TARGET)
	@echo ""
	@echo "Results saved! Run: python3 visualize_benchmarks.py"

# Run full benchmark
run-benchmark: $(BENCHMARK_TARGET)
	@echo "Running full benchmark (this may take several minutes)..."
	./$(BENCHMARK_TARGET)
	@echo ""
	@echo "Results saved! Run: python3 visualize_benchmarks.py"

# Visualize results
viz:
	@echo "Generating visualizations..."
	python3 visualize_benchmarks.py

# Compile source files into build/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(BENCHMARK_TARGET) $(BENCH_SAMPLE_TARGET) $(SERVER_TARGET)

# Rebuild everything
rebuild: clean all

# Check system info
info:
	@echo "=== System Information ==="
	@echo "Compiler: $(CXX)"
	@$(CXX) --version | head -n 1
	@echo "C++ Standard: C++17"
	@echo "GLIBC version:"
	@ldd --version | head -n 1
	@echo "=========================="

# Help
help:
	@echo "Available targets:"
	@echo "  make              - Build the main executable"
	@echo "  make server       - Build the search server (for Python GUI)"
	@echo "  make benchmark    - Build the full benchmark runner"
	@echo "  make bench        - Build the sample benchmark (quick test)"
	@echo "  make run-benchmark - Run the full benchmark"
	@echo "  make run-bench    - Run the sample benchmark"
	@echo "  make viz          - Generate visualizations from results"
	@echo "  make clean        - Remove build files"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make info         - Show system information"
	@echo "  make help         - Show this help"

.PHONY: all server benchmark bench run-benchmark run-bench viz clean rebuild info help
