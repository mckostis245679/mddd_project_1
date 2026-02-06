#include <fstream>
#include <sstream>
#include <iostream>
#include "movie.h"
#include "kdtree/kdtree.h"
#include "lsh.h"
#include "range_tree.h"
#include "r_tree.h"
#include "quadtree.h"

using namespace std;


int main() {
    auto movies = readMoviesCSV("data_movies_clean.csv");

    //kdtree
     KDTree<2> tree;

    for (int i=0;i<10000;i++){
        tree.insert({movies[i].revenue,movies[i].runtime},&movies[i]);
    }

    // Print the KDTree structure
    cout << "KD Tree sstructure:" << endl;
   // tree.print();
    //revenu=3000,runtime=2
    //
    array<double, 2> searchPoint = {3000, 2};
    cout << "\nSearching for movie 3: "
        << (tree.search(searchPoint) ? "Found" : "Not found") << endl;

    auto result= tree.search(searchPoint);
    if(result) {
        cout << "Found movie: " << result->movie->title << endl;
    } else {
        cout << "Movie not found!" << endl;
    }

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
    vector<Movie*> results = tree.kNNSearch(2,searchPoint);
    for(int i=0;i<results.size();i++){
        cout << "Found movie: " << results[i]->title <<endl<< results[i]->revenue<<endl<< results[i]->runtime<< endl;
    }

    //LSH

//   // -----------------------
//     // LSH (new part)
//     // -----------------------
//     MinHashLSH lsh(120, 20, 2, 42);

//     // Add movies
//     for (auto& movie : movies) {
//         std::string text = movie.title; // optionally add genres/companies too
//         lsh.addDocument(&movie, text);
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




    //RANGE TREE
    MovieRangeTree<2, double> rangeTree;

    for (int i = 0; i < 10000; i++) {
        rangeTree.insert({movies[i].revenue, movies[i].runtime}, &movies[i]);
    }

    array<double, 2> low = {3000.0, 80.0};
    array<double, 2> high = {100000.0, 140.0};

    auto rangeTreeresults = rangeTree.rangeQuery(low, high);
    cout<<"RANGE TREE RESULTS:"<<endl;
    for (Movie* m : rangeTreeresults) {
        if (!m) continue;
        cout << m->title << " | " << m->revenue << " | " << m->runtime << "\n";
    }


    //R TREE
    MovieRTree<2, double, 8> rtree;

    // Insert movies as point-rectangles: (revenue, runtime)
    for (int i = 0; i < 10000 ; i++) {
        MovieRTree<2, double, 8>::Rect rect;
        rect.minPoint = {movies[i].revenue, movies[i].runtime};
        rect.maxPoint = {movies[i].revenue, movies[i].runtime};
        rtree.insert(rect, &movies[i]);
    }

    // Query box
    MovieRTree<2, double, 8>::Rect query;
    query.minPoint = {3000.0, 80.0};
    query.maxPoint = {100000.0, 140.0};

    auto rTreeresults = rtree.rangeQuery(query);
    for (Movie* m : rTreeresults) {
        if (!m) continue;
        cout << m->title << " | revenue=" << m->revenue << " | runtime=" << m->runtime << "\n";
    }


    // QUADTREE
    cout << "\n=== QUADTREE ===" << endl;
    
    // Create quadtree with boundary covering your data range
    // Adjust these bounds based on your actual data
    QuadTree<double> quadtree(QuadTree<double>::Boundary(0, 1000000000, 0, 300));
    
    // Insert movies
    for (int i = 0; i < 10000; i++) {
        quadtree.insert({movies[i].revenue, movies[i].runtime}, &movies[i]);
    }
    
    cout << "Inserted " << quadtree.size() << " movies into quadtree\n";
    
    // Point search
    searchPoint = {3000, 2};
    Movie* foundMovie = quadtree.search(searchPoint);
    if (foundMovie) {
        cout << "Found movie: " << foundMovie->title << endl;
    } else {
        cout << "Movie not found at exact point!\n";
    }
    
    // Range query
    array<double, 2> lower = {3000.0, 80.0};
    array<double, 2> upper = {100000.0, 140.0};
    auto quadResults = quadtree.rangeQuery(lower, upper);
    
    cout << "\nQuadtree Range Query Results (" << quadResults.size() << " movies):\n";
    for (int i = 0; i < min(5, (int)quadResults.size()); i++) {
        cout << quadResults[i]->title 
             << " | revenue=" << quadResults[i]->revenue 
             << " | runtime=" << quadResults[i]->runtime << "\n";
    }


    return 0;
}


