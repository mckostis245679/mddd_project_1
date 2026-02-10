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
int example();

int main() {
    //example();
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
    vector<Movie*> results = tree.kNNSearch(10,searchPoint);
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






#include <chrono>
#include <limits>
#include <unordered_set>

bool inYearRange(const string& date, int fromYear, int toYear) {
    // expects YYYY-MM-DD
    if (date.size() < 4) return false;
    int y = stoi(date.substr(0, 4));
    return y >= fromYear && y <= toYear;
}

bool containsToken(const string& text, const string& token) {
    // simple case-insensitive contains
    string a = text, b = token;
    for (char& c : a) c = (char)tolower((unsigned char)c);
    for (char& c : b) c = (char)tolower((unsigned char)c);
    return a.find(b) != string::npos;
}

bool isAllowedCountry(const string& originCountry) {
    // simple check for US or GB in string like "['US']" or "['US','GB']"
    return containsToken(originCountry, "us") || containsToken(originCountry, "gb");
}

bool isEnglish(const string& originalLanguage) {
    return containsToken(originalLanguage, "en");
}

// Project-style full filter after tree range
bool passProjectFilters(Movie* m) {
    if (!m) return false;

    // Example constraints from project statement:
    // release year 2000..2020, popularity 3..6, vote_average 3..5, runtime 30..60,
    // origin_country in {US,GB}, original_language=en
    if (!inYearRange(m->release_date, 2000, 2020)) return false;
    if (m->popularity < 3.0 || m->popularity > 6.0) return false;
    if (m->vote_average < 3.0 || m->vote_average > 5.0) return false;
    if (m->runtime < 30.0 || m->runtime > 60.0) return false;
    if (!isAllowedCountry(m->origin_country)) return false;
    if (!isEnglish(m->original_language)) return false;

    return true;
}

// Build text used by LSH (choose one textual attribute or combine)
string buildTextForLSH(Movie* m, int textMode) {
    // 1=production_company_names, 2=genre_names, 3=both
    if (!m) return "";
    if (textMode == 1) return m->production_company_names;
    if (textMode == 2) return m->genre_names;
    return m->production_company_names + " " + m->genre_names;
}

void printMovieShort(Movie* m) {
    if (!m) return;
    cout << m->title
         << " | year=" << (m->release_date.size() >= 4 ? m->release_date.substr(0,4) : "NA")
         << " | pop=" << m->popularity
         << " | vote=" << m->vote_average
         << " | runtime=" << m->runtime
         << " | revenue=" << m->revenue
         << "\n";
}

int example() {
    auto movies = readMoviesCSV("data_movies_clean.csv");
    if (movies.empty()) {
        cout << "No movies loaded.\n";
        return 0;
    }

    cout << "Loaded movies: " << movies.size() << "\n";

    // Build all indexes once
    KDTree<2> kdTree;
    MovieRangeTree<2, double> rangeTree;
    MovieRTree<2, double, 8> rTree;
    QuadTree<double> quadTree(QuadTree<double>::Boundary(0, 1000000000, 0, 300));

    for (int i = 0; i < 10000; i++) {
        array<double,2> p = {movies[i].revenue, movies[i].runtime};
        kdTree.insert(p, &movies[i]);
        rangeTree.insert(p, &movies[i]);

        MovieRTree<2, double, 8>::Rect rect;
        rect.minPoint = p;
        rect.maxPoint = p;
        rTree.insert(rect, &movies[i]);

        quadTree.insert(p, &movies[i]);
    }

    cout << "Indexes built.\n";

    while (true) {
        cout << "\n========== MENU ==========\n";
        cout << "1) KD-tree + LSH\n";
        cout << "2) Quad-tree + LSH\n";
        cout << "3) Range-tree + LSH\n";
        cout << "4) R-tree + LSH\n";
        cout << "5) Exit\n";
        cout << "Choose: ";

        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (choice == 5) break;
        if (choice < 1 || choice > 5) continue;

        // Numeric range for index query (you can edit defaults quickly)
        double minRevenue = 0, maxRevenue = 1e9;
        double minRuntime = 30, maxRuntime = 60;

        cout << "Revenue min max (e.g. 0 1000000000): ";
        cin >> minRevenue >> maxRevenue;
        cout << "Runtime min max (e.g. 30 60): ";
        cin >> minRuntime >> maxRuntime;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        vector<Movie*> candidates;

        auto t1 = chrono::high_resolution_clock::now();

        if (choice == 1) {
            array<double,2> low = {minRevenue, minRuntime};
            array<double,2> high = {maxRevenue, maxRuntime};
            candidates = kdTree.rangeSearch(low, high);
        } else if (choice == 2) {
            array<double,2> low = {minRevenue, minRuntime};
            array<double,2> high = {maxRevenue, maxRuntime};
            candidates = quadTree.rangeQuery(low, high);
        } else if (choice == 3) {
            array<double,2> low = {minRevenue, minRuntime};
            array<double,2> high = {maxRevenue, maxRuntime};
            candidates = rangeTree.rangeQuery(low, high);
        } else if (choice == 4) {
            MovieRTree<2, double, 8>::Rect q;
            q.minPoint = {minRevenue, minRuntime};
            q.maxPoint = {maxRevenue, maxRuntime};
            candidates = rTree.rangeQuery(q);
        }

        // Extra project filters (year/popularity/vote/country/language)
        vector<Movie*> filtered;
        filtered.reserve(candidates.size());
        for (Movie* m : candidates) {
            if (passProjectFilters(m)) filtered.push_back(m);
        }

        auto t2 = chrono::high_resolution_clock::now();
        auto filterMs = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();

        cout << "\nIndex candidates: " << candidates.size() << "\n";
        cout << "After project filters: " << filtered.size() << "\n";
        cout << "Filter time: " << filterMs << " ms\n";

        if (filtered.empty()) {
            cout << "No filtered movies. Try wider numeric ranges.\n";
            continue;
        }

        // LSH phase
        int textMode = 3;
        cout << "\nText attribute for LSH: 1=production_company_names, 2=genre_names, 3=both : ";
        cin >> textMode;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        int topN = 3;
        cout << "Top N similar results (e.g. 3): ";
        cin >> topN;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        string queryText;
        cout << "Enter similarity query text: ";
        getline(cin, queryText);

        MinHashLSH lsh(120, 20, 2, 42);
        for (Movie* m : filtered) {
            lsh.addDocument(m, buildTextForLSH(m, textMode));
        }

        auto t3 = chrono::high_resolution_clock::now();
        lsh.buildIndex();
        auto lshResults = lsh.query(queryText, topN, 0.0); // keep simple: no strict threshold
        auto t4 = chrono::high_resolution_clock::now();

        auto lshMs = chrono::duration_cast<chrono::milliseconds>(t4 - t3).count();

        cout << "\nTop-" << topN << " similar:\n";
        for (int i = 0; i < (int)lshResults.size(); i++) {
            Movie* m = lshResults[i].movie;
            if (!m) continue;
            cout << i + 1 << ") score=" << lshResults[i].jaccardScore << " | ";
            printMovieShort(m);
        }

        cout << "LSH build+query time: " << lshMs << " ms\n";
    }

    cout << "Done.\n";
    return 0;
}
