# Compiler
CXX = g++

# Compiler flags - Compatible with older systems
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -static-libstdc++ -static-libgcc

# Target executable
TARGET = executable

# Directories
SRC_DIR = .
OBJ_DIR = build

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files into build/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

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
	@echo "  make          - Build the executable"
	@echo "  make clean    - Remove build files"
	@echo "  make rebuild  - Clean and rebuild"
	@echo "  make info     - Show system information"
	@echo "  make help     - Show this help"

.PHONY: all clean rebuild info help
