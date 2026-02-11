#!/bin/bash
# Build and run script for movie search project

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

success() {
    echo -e "${GREEN}✓${NC} $1"
}

error() {
    echo -e "${RED}✗${NC} $1"
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Build benchmark if needed
build_bench() {
    if [ ! -f "benchmark_sample" ]; then
        info "Building benchmark executable..."
        make bench
        success "Benchmark built"
    else
        info "Benchmark already built (run 'make clean' to rebuild)"
    fi
}

# Build server if needed
build_server() {
    if [ ! -f "search_server" ]; then
        info "Building search server..."
        make server
        success "Search server built"
    else
        info "Search server already built (run 'make clean' to rebuild)"
    fi
}

# Check Python dependencies
check_python_deps() {
    python3 -c "import tkinter" 2>/dev/null || {
        error "tkinter not found. Install with: sudo apt-get install python3-tk"
        exit 1
    }

    if [ "$1" == "viz" ]; then
        python3 -c "import matplotlib" 2>/dev/null || {
            error "matplotlib not found. Install with: pip3 install matplotlib"
            exit 1
        }
    fi
}

# Command handlers
cmd_bench() {
    info "Running benchmark workflow..."
    echo ""

    build_bench

    info "Running benchmarks (this may take a few minutes)..."
    make run-bench
    success "Benchmarks complete"
    echo ""

    check_python_deps viz

    info "Generating visualizations..."
    make viz
    success "Visualizations generated"
    echo ""

    success "All done! Check the benchmark_plots/ folder for results"
}

cmd_ui() {
    info "Starting GUI..."
    echo ""

    build_server
    check_python_deps

    info "Launching Python GUI..."
    info "The server will start automatically and load 946K movies"
    info "First startup takes ~10 seconds, then searches are instant!"
    echo ""

    python3 movie_search_gui.py
}

cmd_help() {
    echo "Movie Search Build & Run Script"
    echo ""
    echo "Usage: ./run <command>"
    echo ""
    echo "Commands:"
    echo "  bench    - Build and run benchmarks, then show visualizations"
    echo "  ui       - Build and launch the Python GUI with fast search server"
    echo "  clean    - Clean all build files"
    echo "  help     - Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./run bench    # Run benchmarks and generate plots"
    echo "  ./run ui       # Launch the GUI"
    echo ""
}

cmd_clean() {
    info "Cleaning build files..."
    make clean
    rm -rf benchmark_plots/
    success "Clean complete"
}

# Main script
case "$1" in
    bench)
        cmd_bench
        ;;
    ui)
        cmd_ui
        ;;
    clean)
        cmd_clean
        ;;
    help|--help|-h|"")
        cmd_help
        ;;
    *)
        error "Unknown command: $1"
        echo ""
        cmd_help
        exit 1
        ;;
esac
