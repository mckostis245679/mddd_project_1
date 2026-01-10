#include <fstream>
#include <sstream>
#include <iostream>
#include "movie.h"
#include "kdtree/kdtree.h"
using namespace std;


int main() {
    auto movies = readMoviesCSV("data_movies_clean.csv");

     KDTree<2> tree;

    for (int i=0;i<10;i++){
        tree.insert({movies[i].revenue,movies[i].runtime},&movies[i]);
    }

    // Print the KDTree structure
    cout << "KD Tree sstructure:" << endl;
    tree.print();

    array<double, 2> searchPoint = {movies[2].revenue, movies[2].runtime};
    // cout << "\nSearching for movie 3: "
    //     << (tree.search(searchPoint) ? "Found" : "Not found") << endl;

    // auto result= tree.search(searchPoint);
    // if(result) {
    //     cout << "Found movie: " << result->movie->title << endl;
    // } else {
    //     cout << "Movie not found!" << endl;
    // }

    // array<double, 2> lower = {3000.0, 1.0};
    // array<double, 2> upper = {4000.0, 4.0};
    // vector<Movie*> results = tree.rangeSearch(lower, upper);
    // for(int i=0;i<results.size();i++){
    //     cout << "Found movie: " << results[i]->title << endl;
    // }
    //  result=tree.nNSearch(searchPoint);
    // if(result) {
    //     cout << "Found movie: " << result->movie->title << endl;
    // } else {
    //     cout << "Movie not found!" << endl;
    // }
    vector<Movie*> results = tree.kNNSearch(4,searchPoint);
    for(int i=0;i<results.size();i++){
        cout << "Found movie: " << results[i]->title <<results[i]->revenue<< results[i]->runtime<< endl;
    }
    return 0;
}