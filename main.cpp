#include <fstream>
#include <sstream>
#include <iostream>
#include "movie.h"

using namespace std;


int main() {
    auto movies = readMoviesCSV("data_movies_clean.csv");

    std::cout << "Loaded: " << movies.size() << " movies\n";
    std::cout << movies[2].title << "\n";
    std::cout << movies[5].genre_names << "\n";
}