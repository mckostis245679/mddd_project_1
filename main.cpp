#include <fstream>
#include <sstream>
#include <iostream>
#include "movie.h"
#include "kdtree/kdtree.h"
using namespace std;


int main() {
    auto movies = readMoviesCSV("data_movies_clean.csv");

    std::cout << "Loaded: " << movies.size() << " movies\n";
    std::cout << movies[2].title << "\n";
    std::cout << movies[5].genre_names << "\n";


     KDTree<2> tree;

    for (int i=0;i<10;i++){
        tree.insert({movies[i].revenue,movies[i].runtime},&movies[i]);
    }

    // Print the KDTree structure
    cout << "KD Tree structure:" << endl;
    tree.print();

    array<double, 2> searchPoint = {movies[2].revenue, movies[2].runtime};
    cout << "\nSearching for movie 3: "
        << (tree.search(searchPoint) ? "Found" : "Not found") << endl;

    auto result= tree.search(searchPoint);
    if(result) {
        cout << "Found movie: " << result->movie->title << endl;
    } else {
        cout << "Movie not found!" << endl;
    }
    return 0;
}