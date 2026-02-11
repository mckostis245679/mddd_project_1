#include <iostream>
#include "movie.h"
#include "comprehensive_benchmark.cpp"
#include "data.cpp"

int main() {
    std::cout << "Loading movie data...\n";
    auto movies = readMoviesCSV("data_movies_clean.csv");

    std::cout << "Loaded " << movies.size() << " movies\n";

    if (movies.empty()) {
        std::cerr << "Error: No movies loaded!\n";
        return 1;
    }

    run_comprehensive_benchmarks(movies);

    std::cout << "\nNow run: python3 visualize_benchmarks.py\n";
    std::cout << "to generate visualizations.\n";

    return 0;
}
