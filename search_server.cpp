// search_server.cpp - Persistent search server for GUI
// Builds data structures once, then answers queries via stdin/stdout

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <limits>
#include <unordered_set>
#include <memory>

#include "movie.h"
#include "kdtree.h"
#include "rtree.h"
#include "quadtree.h"
#include "rangetree.h"
#include "lsh.h"

// Helper to extract release year
double releaseYearAsDouble(const Movie& m) {
    if (m.release_date.size() >= 4) {
        try {
            return std::stod(m.release_date.substr(0, 4));
        } catch (...) {}
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// Field accessors
double numericFieldValue(const Movie& m, int fieldId) {
    switch(fieldId) {
        case 1: return m.budget;
        case 2: return m.revenue;
        case 3: return m.runtime;
        case 4: return m.popularity;
        case 5: return m.vote_average;
        case 6: return m.vote_count;
        case 7: return releaseYearAsDouble(m);
        default: return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string buildLSHText(const Movie& m, const std::vector<int>& stringFieldIds) {
    std::string text;
    for (int id : stringFieldIds) {
        switch(id) {
            case 1: text += m.title + " "; break;
            case 2: text += m.genre_names + " "; break;
            case 3: text += m.production_company_names + " "; break;
            case 4: text += m.original_language + " "; break;
            case 5: text += m.origin_country + " "; break;
        }
    }
    return text;
}

// Base class for search engines
class SearchEngine {
public:
    virtual ~SearchEngine() = default;
    virtual std::vector<Movie*> rangeSearch(const std::vector<double>& lower, const std::vector<double>& upper) = 0;
};

// KDTree wrapper
template<size_t K>
class KDTreeEngine : public SearchEngine {
    KDTree<K> tree;
public:
    KDTreeEngine(const std::vector<Movie>& movies, const std::vector<int>& fieldIds) {
        for (size_t i = 0; i < movies.size(); ++i) {
            std::array<double, K> p{};
            bool valid = true;
            for (size_t d = 0; d < K; ++d) {
                double v = numericFieldValue(movies[i], fieldIds[d]);
                if (!std::isfinite(v)) { valid = false; break; }
                p[d] = v;
            }
            if (valid) tree.insert(p, const_cast<Movie*>(&movies[i]));
        }
    }

    std::vector<Movie*> rangeSearch(const std::vector<double>& lower, const std::vector<double>& upper) override {
        std::array<double, K> lowerA{}, upperA{};
        for (size_t d = 0; d < K; ++d) {
            lowerA[d] = lower[d];
            upperA[d] = upper[d];
        }
        return tree.rangeSearch(lowerA, upperA);
    }
};

// RTree wrapper
template<size_t K>
class RTreeEngine : public SearchEngine {
    RTree<K> tree;
public:
    RTreeEngine(const std::vector<Movie>& movies, const std::vector<int>& fieldIds) {
        for (size_t i = 0; i < movies.size(); ++i) {
            std::array<double, K> p{};
            bool valid = true;
            for (size_t d = 0; d < K; ++d) {
                double v = numericFieldValue(movies[i], fieldIds[d]);
                if (!std::isfinite(v)) { valid = false; break; }
                p[d] = v;
            }
            if (valid) {
                typename RTree<K>::Rect rect;
                rect.minPoint = p;
                rect.maxPoint = p;
                tree.insert(rect, const_cast<Movie*>(&movies[i]));
            }
        }
    }

    std::vector<Movie*> rangeSearch(const std::vector<double>& lower, const std::vector<double>& upper) override {
        std::array<double, K> lowerA{}, upperA{};
        for (size_t d = 0; d < K; ++d) {
            lowerA[d] = lower[d];
            upperA[d] = upper[d];
        }
        typename RTree<K>::Rect q;
        q.minPoint = lowerA;
        q.maxPoint = upperA;
        return tree.rangeQuery(q);
    }
};

// QuadTree wrapper
class QuadTreeEngine : public SearchEngine {
    std::unique_ptr<QuadTree> tree;
public:
    QuadTreeEngine(const std::vector<Movie>& movies, const std::vector<int>& fieldIds) {
        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();

        std::vector<std::pair<std::array<double,2>, Movie*>> valid_points;

        for (size_t i = 0; i < movies.size(); ++i) {
            double x = numericFieldValue(movies[i], fieldIds[0]);
            double y = numericFieldValue(movies[i], fieldIds[1]);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;

            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
            valid_points.push_back({{x, y}, const_cast<Movie*>(&movies[i])});
        }

        const double padX = std::max(1e-9, (maxX - minX) * 1e-6);
        const double padY = std::max(1e-9, (maxY - minY) * 1e-6);
        QuadTree::Boundary world(minX - padX, maxX + padX, minY - padY, maxY + padY);
        tree = std::make_unique<QuadTree>(world);

        for (auto& [pt, mov] : valid_points) {
            tree->insert(pt, mov);
        }
    }

    std::vector<Movie*> rangeSearch(const std::vector<double>& lower, const std::vector<double>& upper) override {
        std::array<double, 2> lowerA = {lower[0], lower[1]};
        std::array<double, 2> upperA = {upper[0], upper[1]};
        return tree->rangeQuery(lowerA, upperA);
    }
};

// RangeTree2D wrapper
class RangeTree2DEngine : public SearchEngine {
    RangeTree2D tree;
public:
    RangeTree2DEngine(const std::vector<Movie>& movies, const std::vector<int>& fieldIds) {
        for (size_t i = 0; i < movies.size(); ++i) {
            double x = numericFieldValue(movies[i], fieldIds[0]);
            double y = numericFieldValue(movies[i], fieldIds[1]);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            tree.insert({x, y}, const_cast<Movie*>(&movies[i]));
        }
    }

    std::vector<Movie*> rangeSearch(const std::vector<double>& lower, const std::vector<double>& upper) override {
        std::array<double, 2> lowerA = {lower[0], lower[1]};
        std::array<double, 2> upperA = {upper[0], upper[1]};
        return tree.rangeSearch(lowerA, upperA);
    }
};

int main() {
    // Disable buffering for immediate communication
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    std::cerr << "SERVER: Loading movies...\n";
    auto movies = readMoviesCSV("data_movies_clean.csv");
    std::cerr << "SERVER: Loaded " << movies.size() << " movies\n";

    if (movies.empty()) {
        std::cerr << "ERROR: No movies loaded\n";
        return 1;
    }

    std::cerr << "SERVER: Ready for commands\n";
    std::cout << "READY\n";  // Signal to Python that we're ready

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "QUIT") {
            std::cerr << "SERVER: Shutting down\n";
            break;
        }

        if (line != "SEARCH") {
            std::cerr << "SERVER: Unknown command: " << line << "\n";
            continue;
        }

        try {
            // Read configuration
            int treeType, k;
            std::getline(std::cin, line); treeType = std::stoi(line);

            if (treeType == 1 || treeType == 2) {
                std::getline(std::cin, line); k = std::stoi(line);
            } else {
                k = 2;  // 2D trees
            }

            // Parse numeric field IDs
            std::getline(std::cin, line);
            std::vector<int> numericFieldIds;
            std::stringstream ss(line);
            std::string token;
            while (std::getline(ss, token, ',')) {
                numericFieldIds.push_back(std::stoi(token));
            }

            // Parse ranges
            std::vector<double> lower(k), upper(k);
            for (int i = 0; i < k; ++i) {
                std::getline(std::cin, line); lower[i] = std::stod(line);
                std::getline(std::cin, line); upper[i] = std::stod(line);
            }

            // Parse string field IDs
            std::getline(std::cin, line);
            std::vector<int> stringFieldIds;
            std::stringstream ss2(line);
            while (std::getline(ss2, token, ',')) {
                stringFieldIds.push_back(std::stoi(token));
            }

            // LSH query
            std::string lshQuery;
            std::getline(std::cin, lshQuery);

            // Top-N
            int N;
            std::getline(std::cin, line); N = std::stoi(line);

            // Search mode
            char mode;
            std::getline(std::cin, line);
            mode = line.empty() ? 'h' : line[0];

            std::cerr << "SERVER: Building tree type " << treeType << " with k=" << k << "\n";

            // Build tree
            std::unique_ptr<SearchEngine> engine;
            if (treeType == 1) {  // KD-Tree
                if (k == 2) engine = std::make_unique<KDTreeEngine<2>>(movies, numericFieldIds);
                else if (k == 3) engine = std::make_unique<KDTreeEngine<3>>(movies, numericFieldIds);
                else if (k == 4) engine = std::make_unique<KDTreeEngine<4>>(movies, numericFieldIds);
                else if (k == 5) engine = std::make_unique<KDTreeEngine<5>>(movies, numericFieldIds);
            } else if (treeType == 2) {  // R-Tree
                if (k == 2) engine = std::make_unique<RTreeEngine<2>>(movies, numericFieldIds);
                else if (k == 3) engine = std::make_unique<RTreeEngine<3>>(movies, numericFieldIds);
                else if (k == 4) engine = std::make_unique<RTreeEngine<4>>(movies, numericFieldIds);
                else if (k == 5) engine = std::make_unique<RTreeEngine<5>>(movies, numericFieldIds);
            } else if (treeType == 3) {  // QuadTree
                engine = std::make_unique<QuadTreeEngine>(movies, numericFieldIds);
            } else if (treeType == 4) {  // RangeTree2D
                engine = std::make_unique<RangeTree2DEngine>(movies, numericFieldIds);
            }

            std::cerr << "SERVER: Building LSH index\n";

            // Build LSH
            MinHashLSH lsh(120, 20, 2, 42);
            for (size_t i = 0; i < movies.size(); ++i) {
                std::string text = buildLSHText(movies[i], stringFieldIds);
                if (!text.empty()) lsh.addDocument(&movies[i], text);
            }
            lsh.buildIndex();

            std::cerr << "SERVER: Performing search\n";

            // Perform search
            std::vector<Movie*> treeResults;
            if (mode != 'l') {  // Not LSH-only
                treeResults = engine->rangeSearch(lower, upper);
            }

            // LSH query
            int topCandidates = std::max(N * 50, 200);
            auto lshResults = lsh.query(lshQuery, topCandidates, 0.0);

            // Combine results
            std::vector<MinHashLSH::QueryResult> finalResults;
            finalResults.reserve(N);

            if (mode == 'r') {  // Range only
                for (Movie* m : treeResults) {
                    if (!m) continue;
                    MinHashLSH::QueryResult r;
                    r.movie = m;
                    r.jaccardScore = 0.0;
                    finalResults.push_back(r);
                    if (static_cast<int>(finalResults.size()) >= N) break;
                }
            } else if (mode == 'l') {  // LSH only
                for (const auto& r : lshResults) {
                    if (!r.movie) continue;
                    finalResults.push_back(r);
                    if (static_cast<int>(finalResults.size()) >= N) break;
                }
            } else {  // Hybrid
                std::unordered_set<Movie*> allowed(treeResults.begin(), treeResults.end());
                for (const auto& r : lshResults) {
                    if (!r.movie || !allowed.count(r.movie)) continue;
                    finalResults.push_back(r);
                    if (static_cast<int>(finalResults.size()) >= N) break;
                }
            }

            // Output results
            std::cout << "RESULTS\n";
            std::cout << "Tree candidates: " << treeResults.size() << "\n";
            std::cout << "LSH candidates: " << lshResults.size() << "\n";
            std::cout << "Intersection top-" << N << ": " << finalResults.size() << "\n";

            for (size_t i = 0; i < finalResults.size(); ++i) {
                Movie* m = finalResults[i].movie;
                std::cout << (i + 1) << ") " << m->title
                          << " | Jaccard=" << finalResults[i].jaccardScore
                          << " | year=" << releaseYearAsDouble(*m)
                          << " | pop=" << m->popularity
                          << " | vote_avg=" << m->vote_average
                          << " | runtime=" << m->runtime
                          << " | country=" << m->origin_country
                          << " | lang=" << m->original_language
                          << "\n";
            }

            std::cout << "END\n";
            std::cerr << "SERVER: Search complete\n";

        } catch (const std::exception& e) {
            std::cerr << "SERVER ERROR: " << e.what() << "\n";
            std::cout << "ERROR\n" << e.what() << "\nEND\n";
        }
    }

    return 0;
}
