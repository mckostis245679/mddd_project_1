// simple_ui.cpp
#include "simple_ui.h"   // declares: void runHybridSearchUI(std::vector<Movie>& movies);

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "movie.h"
#include "kdtree.h"      // KDTree<K>
#include "rtree.h"       // RTree<K>
#include "quadtree.h"    // QuadTree (2D)
#include "rangetree.h" // RangeTree2D (2D)
#include "lsh.h"         // MinHashLSH

// -------------------- UI Config --------------------
enum class TreeType { KD = 1, RTREE = 2, QUADTREE = 3, RANGETREE2D = 4 };

struct UIConfig {
    TreeType treeType = TreeType::KD;
    int k = 2; // used by KD/RTree (2..5), fixed to 2 for QuadTree/RangeTree2D

    std::vector<int> numericFieldIds; // exactly k fields
    std::vector<int> stringFieldIds;  // one or more

    std::vector<double> lower;
    std::vector<double> upper;

    std::string lshQuery;
    int N = 5;

    // Optional post-filter
    bool enforceUSGB_EN = false;
};

// -------------------- Helpers --------------------
static std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static std::vector<std::string> splitTokensAlphaNumUpper(const std::string& s) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            cur.push_back(static_cast<char>(std::toupper(uc)));
        } else {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

static std::vector<int> parseIntList(const std::string& line) {
    std::string t = line;
    for (char& c : t) {
        if (c == ',') c = ' ';
    }
    std::stringstream ss(t);
    std::vector<int> out;
    int x;
    while (ss >> x) out.push_back(x);
    return out;
}

static int readInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        try {
            int v = std::stoi(line);
            if (v >= minVal && v <= maxVal) return v;
        } catch (...) {}
        std::cout << "Invalid input. Try again.\n";
    }
}

static double readDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        try {
            return std::stod(line);
        } catch (...) {
            std::cout << "Invalid number. Try again.\n";
        }
    }
}

static bool readYesNo(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (y/n): ";
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty()) {
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
            if (c == 'y') return true;
            if (c == 'n') return false;
        }
        std::cout << "Please enter y or n.\n";
    }
}

// Parse year from release_date (expects YYYY... usually)
static double releaseYearAsDouble(const Movie& m) {
    const std::string& s = m.release_date;

    if (s.size() >= 4 &&
        std::isdigit(static_cast<unsigned char>(s[0])) &&
        std::isdigit(static_cast<unsigned char>(s[1])) &&
        std::isdigit(static_cast<unsigned char>(s[2])) &&
        std::isdigit(static_cast<unsigned char>(s[3]))) {
        return static_cast<double>(std::stoi(s.substr(0, 4)));
    }

    // fallback: first 4-digit sequence
    for (size_t i = 0; i + 3 < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i])) &&
            std::isdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isdigit(static_cast<unsigned char>(s[i + 2])) &&
            std::isdigit(static_cast<unsigned char>(s[i + 3]))) {
            return static_cast<double>(std::stoi(s.substr(i, 4)));
        }
    }

    return std::numeric_limits<double>::quiet_NaN();
}

// Numeric fields (double-only; release year derived as double)
static std::string numericFieldName(int id) {
    switch (id) {
        case 1: return "budget";
        case 2: return "revenue";
        case 3: return "runtime";
        case 4: return "popularity";
        case 5: return "vote_average";
        case 6: return "vote_count";
        case 7: return "release_year";
        default: return "unknown";
    }
}

static double numericFieldValue(const Movie& m, int id) {
    switch (id) {
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

static std::string stringFieldName(int id) {
    switch (id) {
        case 1: return "title";
        case 2: return "genre_names";
        case 3: return "production_company_names";
        case 4: return "original_language";
        case 5: return "origin_country";
        default: return "unknown";
    }
}

static std::string stringFieldValue(const Movie& m, int id) {
    switch (id) {
        case 1: return m.title;
        case 2: return m.genre_names;
        case 3: return m.production_company_names;
        case 4: return m.original_language;
        case 5: return m.origin_country;
        default: return "";
    }
}

static std::string buildLSHText(const Movie& m, const std::vector<int>& strFieldIds) {
    std::string out;
    for (int id : strFieldIds) {
        std::string part = stringFieldValue(m, id);
        if (!part.empty()) {
            if (!out.empty()) out += " ";
            out += part;
        }
    }
    return out;
}

static bool containsCountryCodeUSorGB(const std::string& originCountry) {
    const auto tokens = splitTokensAlphaNumUpper(originCountry);
    for (const auto& t : tokens) {
        if (t == "US" || t == "USA" || t == "GB" || t == "UK" ||
            t == "UNITEDSTATES" || t == "UNITEDKINGDOM") {
            return true;
        }
    }
    return false;
}

static bool isEnglishCode(const std::string& lang) {
    std::string u = toUpper(lang);
    return (u == "EN" || u == "ENGLISH");
}

static bool passUSGB_EN_Filter(const Movie& m) {
    return containsCountryCodeUSorGB(m.origin_country) && isEnglishCode(m.original_language);
}

static void runLSHIntersectionAndPrint(std::vector<Movie>& movies,
                                       const UIConfig& cfg,
                                       const std::vector<Movie*>& treeResults) {
    // Build LSH on selected string fields
    MinHashLSH lsh(120, 20, 2, 42);

    for (size_t i = 0; i < movies.size(); ++i) {
        std::string text = buildLSHText(movies[i], cfg.stringFieldIds);
        if (!text.empty()) lsh.addDocument(&movies[i], text);
    }
    lsh.buildIndex();

    // LSH query
    int topCandidates = std::max(cfg.N * 50, 200); // fetch enough before intersection
    auto lshResults = lsh.query(cfg.lshQuery, topCandidates, 0.0);

    // Intersection tree ∩ lsh (preserve LSH order)
    std::unordered_set<Movie*> allowed(treeResults.begin(), treeResults.end());

    std::vector<MinHashLSH::QueryResult> finalResults;
    finalResults.reserve(static_cast<size_t>(cfg.N));

    for (const auto& r : lshResults) {
        if (!r.movie) continue;
        if (!allowed.count(r.movie)) continue;
        if (cfg.enforceUSGB_EN && !passUSGB_EN_Filter(*r.movie)) continue;

        finalResults.push_back(r);
        if (static_cast<int>(finalResults.size()) >= cfg.N) break;
    }

    // Print
    std::cout << "\n================ RESULTS ================\n";
    std::cout << "Tree candidates: " << treeResults.size() << "\n";
    std::cout << "LSH candidates : " << lshResults.size() << "\n";
    std::cout << "Intersection top-" << cfg.N << ": " << finalResults.size() << "\n\n";

    if (finalResults.empty()) {
        std::cout << "No results found for this combination.\n";
        return;
    }

    std::cout << std::fixed << std::setprecision(4);
    for (size_t i = 0; i < finalResults.size(); ++i) {
        Movie* m = finalResults[i].movie;
        std::cout << (i + 1) << ") "
                  << m->title
                  << " | Jaccard=" << finalResults[i].jaccardScore
                  << " | year=" << releaseYearAsDouble(*m)
                  << " | pop=" << m->popularity
                  << " | vote_avg=" << m->vote_average
                  << " | runtime=" << m->runtime
                  << " | country=" << m->origin_country
                  << " | lang=" << m->original_language
                  << "\n";
    }
}

// -------------------- Core Engines --------------------
template <size_t K>
static void runSessionWithK(std::vector<Movie>& movies, const UIConfig& cfg) {
    // KD/RTree only
    if (cfg.numericFieldIds.size() != K) {
        std::cerr << "Internal error: numericFieldIds size != K\n";
        return;
    }

    std::vector<std::array<double, K>> points(movies.size());
    std::vector<bool> valid(movies.size(), false);

    for (size_t i = 0; i < movies.size(); ++i) {
        std::array<double, K> p{};
        bool ok = true;
        for (size_t d = 0; d < K; ++d) {
            const double v = numericFieldValue(movies[i], cfg.numericFieldIds[d]);
            if (!std::isfinite(v)) { ok = false; break; }
            p[d] = v;
        }
        if (ok) {
            points[i] = p;
            valid[i] = true;
        }
    }

    std::array<double, K> lowerA{}, upperA{};
    for (size_t d = 0; d < K; ++d) {
        lowerA[d] = std::min(cfg.lower[d], cfg.upper[d]);
        upperA[d] = std::max(cfg.lower[d], cfg.upper[d]);
    }

    std::vector<Movie*> treeResults;

    if (cfg.treeType == TreeType::KD) {
        KDTree<K> tree;
        for (size_t i = 0; i < movies.size(); ++i) {
            if (valid[i]) tree.insert(points[i], &movies[i]);
        }
        treeResults = tree.rangeSearch(lowerA, upperA);

    } else if (cfg.treeType == TreeType::RTREE) {
        RTree<K> tree;
        for (size_t i = 0; i < movies.size(); ++i) {
            if (!valid[i]) continue;
            typename RTree<K>::Rect rect;
            rect.minPoint = points[i];
            rect.maxPoint = points[i];
            tree.insert(rect, &movies[i]);
        }

        typename RTree<K>::Rect q;
        q.minPoint = lowerA;
        q.maxPoint = upperA;
        treeResults = tree.rangeQuery(q);

    } else {
        std::cerr << "Invalid tree type for runSessionWithK.\n";
        return;
    }

    runLSHIntersectionAndPrint(movies, cfg, treeResults);
}

static void runSession2D(std::vector<Movie>& movies, const UIConfig& cfg) {
    // QuadTree / RangeTree2D only (always 2D)
    if (cfg.numericFieldIds.size() != 2) {
        std::cerr << "Internal error: 2D session needs exactly 2 numeric fields.\n";
        return;
    }

    std::array<double, 2> lowerA = {
        std::min(cfg.lower[0], cfg.upper[0]),
        std::min(cfg.lower[1], cfg.upper[1])
    };
    std::array<double, 2> upperA = {
        std::max(cfg.lower[0], cfg.upper[0]),
        std::max(cfg.lower[1], cfg.upper[1])
    };

    std::vector<std::array<double, 2>> points(movies.size());
    std::vector<bool> valid(movies.size(), false);

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < movies.size(); ++i) {
        double x = numericFieldValue(movies[i], cfg.numericFieldIds[0]);
        double y = numericFieldValue(movies[i], cfg.numericFieldIds[1]);

        if (!std::isfinite(x) || !std::isfinite(y)) continue;

        points[i] = {x, y};
        valid[i] = true;

        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    if (!(std::isfinite(minX) && std::isfinite(maxX) &&
          std::isfinite(minY) && std::isfinite(maxY))) {
        std::cerr << "No valid points for selected 2D fields.\n";
        return;
    }

    std::vector<Movie*> treeResults;

    if (cfg.treeType == TreeType::QUADTREE) {
        // world bounds with tiny padding
        const double padX = std::max(1e-9, (maxX - minX) * 1e-6);
        const double padY = std::max(1e-9, (maxY - minY) * 1e-6);

        QuadTree::Boundary world(minX - padX, maxX + padX, minY - padY, maxY + padY);
        QuadTree qt(world);

        for (size_t i = 0; i < movies.size(); ++i) {
            if (valid[i]) qt.insert(points[i], &movies[i]);
        }

        treeResults = qt.rangeQuery(lowerA, upperA);

    } else if (cfg.treeType == TreeType::RANGETREE2D) {
        RangeTree2D rt;
        for (size_t i = 0; i < movies.size(); ++i) {
            if (valid[i]) rt.insert(points[i], &movies[i]);
        }

        treeResults = rt.rangeSearch(lowerA, upperA);

    } else {
        std::cerr << "Invalid tree type for 2D session.\n";
        return;
    }

    runLSHIntersectionAndPrint(movies, cfg, treeResults);
}

// -------------------- Public UI Entry --------------------
void runHybridSearchUI(std::vector<Movie>& movies) {
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }

    UIConfig cfg;

    const int treeChoice = readInt(
        "Choose tree [1=KDTree, 2=RTree, 3=QuadTree, 4=2DRangeTree]: ",
        1, 4
    );
    cfg.treeType = static_cast<TreeType>(treeChoice);

    // KD/RTree -> ask k in [2..5]
    // QuadTree/2DRangeTree -> fixed 2D
    if (cfg.treeType == TreeType::QUADTREE || cfg.treeType == TreeType::RANGETREE2D) {
        cfg.k = 2;
        std::cout << "Selected 2D tree. k is fixed to 2.\n";
    } else {
        cfg.k = readInt("Choose number of dimensions k (2..5): ", 2, 5);
    }

    std::cout << "\nAvailable numeric fields (double):\n";
    for (int id = 1; id <= 7; ++id) {
        std::cout << "  " << id << ") " << numericFieldName(id) << "\n";
    }

    while (true) {
        std::cout << "Select EXACTLY " << cfg.k
                  << " numeric fields (comma/space separated): ";
        std::string line;
        std::getline(std::cin, line);

        auto v = parseIntList(line);
        bool ok = (static_cast<int>(v.size()) == cfg.k);

        std::unordered_set<int> seen;
        for (int x : v) {
            if (x < 1 || x > 7) ok = false;
            if (seen.count(x)) ok = false;
            seen.insert(x);
        }

        if (ok) {
            cfg.numericFieldIds = std::move(v);
            break;
        }
        std::cout << "Invalid selection. Try again.\n";
    }

    cfg.lower.resize(static_cast<size_t>(cfg.k));
    cfg.upper.resize(static_cast<size_t>(cfg.k));

    std::cout << "\nEnter range bounds for selected numeric fields:\n";
    for (int i = 0; i < cfg.k; ++i) {
        const int fieldId = cfg.numericFieldIds[i];
        const std::string fname = numericFieldName(fieldId);

        cfg.lower[i] = readDouble("  lower " + fname + ": ");
        cfg.upper[i] = readDouble("  upper " + fname + ": ");
        if (cfg.lower[i] > cfg.upper[i]) std::swap(cfg.lower[i], cfg.upper[i]);
    }

    std::cout << "\nAvailable LSH string fields:\n";
    for (int id = 1; id <= 5; ++id) {
        std::cout << "  " << id << ") " << stringFieldName(id) << "\n";
    }

    while (true) {
        std::cout << "Select one or more string fields for LSH text: ";
        std::string line;
        std::getline(std::cin, line);

        auto v = parseIntList(line);
        bool ok = !v.empty();

        std::unordered_set<int> seen;
        for (int x : v) {
            if (x < 1 || x > 5) ok = false;
            if (seen.count(x)) ok = false;
            seen.insert(x);
        }

        if (ok) {
            cfg.stringFieldIds = std::move(v);
            break;
        }
        std::cout << "Invalid selection. Try again.\n";
    }

    std::cout << "Enter LSH query string: ";
    std::getline(std::cin, cfg.lshQuery);

    cfg.N = readInt("Enter N (top-N intersection results): ", 1, 1000);

    cfg.enforceUSGB_EN = readYesNo(
        "Apply extra categorical filter: origin-country in {US,GB} AND original-language == EN"
    );

    // Dispatch
    if (cfg.treeType == TreeType::QUADTREE || cfg.treeType == TreeType::RANGETREE2D) {
        runSession2D(movies, cfg);
    } else {
        switch (cfg.k) {
            case 2: runSessionWithK<2>(movies, cfg); break;
            case 3: runSessionWithK<3>(movies, cfg); break;
            case 4: runSessionWithK<4>(movies, cfg); break;
            case 5: runSessionWithK<5>(movies, cfg); break;
            default:
                std::cerr << "Unsupported k for KD/RTree.\n";
                return;
        }
    }

    std::cout << "\nDone.\n";
}
