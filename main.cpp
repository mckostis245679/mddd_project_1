#include <fstream>
#include <sstream>
#include <iostream>
#include "movie.h"
#include "kdtree.h"
#include "lsh.h"
#include "rangetree.h"
#include "rtree.h"
#include "quadtree.h"
#include "benchmark.cpp"
#include "simple_ui.h"


using namespace std;

int main() {
    //example();
    auto movies = readMoviesCSV("data_movies_clean.csv");
     runHybridSearchUI(movies);

//   // -----------------------
//     // LSH (new part)
//     // -----------------------
//     MinHashLSH lsh(120, 20, 2, 42);

//     // Add movies
//     for (auto& movie : movies) {
//         lsh.addDocument(&movie, movie.title);
//     }

//     lsh.buildIndex();
//     cout<<"lsh built";

//     while (true) {
//         string queryText;
//         cout << "\nEnter query (or 'exit'): ";
//         getline(cin, queryText);

//         if (queryText == "exit") break;
//         if (queryText.empty()) continue;

//         auto lshResults = lsh.query(queryText, 5, 0.02);

//         if (lshResults.empty()) {
//             cout << "No similar movies found.\n";
//             continue;
//         }

//         for (int i = 0; i < (int)lshResults.size(); i++) {
//             Movie* movie = lshResults[i].movie;
//             if (!movie) continue;

//             cout << i + 1 << ") "
//                 << movie->title
//                 << " | score: " << lshResults[i].jaccardScore
//                 << " | revenue: " << movie->revenue
//                 << " | runtime: " << movie->runtime
//                 << "\n";
//         }
//     }   





    //QuadTree_execution_time(movies);
    //KDTree_execution_time(movies);
    //RangeTree2D_execution_time(movies);
    //RTree_execution_time(movies);
    //LSH_execution_time(movies);
    return 0;
}



