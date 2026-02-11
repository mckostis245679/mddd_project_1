#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "quadtree.h"
#include "movie.h"
#include "kdtree.h"
#include "rangetree.h"
#include "rtree.h"
#include "lsh.h"

struct BenchmarkResult {
    std::string structure_name;
    int k;
    std::string query_type;
    long long build_time_us;
    double mean_query_us;
    int num_queries;
};

std::vector<BenchmarkResult> results;

// Helper to add noise to a movie for query generation
inline void addSmallNoise(Movie& m, double p = 0.30, double relSigma = 0.50) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::bernoulli_distribution apply(p);

    auto perturb = [&](double& v, double lo, double hi, double absFloorSigma) {
        if (!std::isfinite(v)) return;
        if (!apply(rng)) return;

        double sigma = std::max(std::abs(v) * relSigma, absFloorSigma);
        std::normal_distribution<double> noise(0.0, sigma);
        v = std::clamp(v + noise(rng), lo, hi);
    };

    perturb(m.budget,       0.0, std::numeric_limits<double>::max(), 1000.0);
    perturb(m.revenue,      0.0, std::numeric_limits<double>::max(), 1000.0);
    perturb(m.runtime,      0.0, std::numeric_limits<double>::max(), 1.0);
    perturb(m.popularity,   0.0, std::numeric_limits<double>::max(), 0.5);
    perturb(m.vote_count,   0.0, std::numeric_limits<double>::max(), 1.0);
    perturb(m.vote_average, 0.0, 10.0,                               0.2);
}

// Extract K-dimensional point from movie
template <size_t K>
std::array<double, K> extractPoint(const Movie& m) {
    std::array<double, K> pt;
    if constexpr (K >= 1) pt[0] = m.revenue;
    if constexpr (K >= 2) pt[1] = m.runtime;
    if constexpr (K >= 3) pt[2] = m.budget;
    if constexpr (K >= 4) pt[3] = m.popularity;
    if constexpr (K >= 5) pt[4] = m.vote_average;
    return pt;
}

// Collect valid indices for K dimensions
template <size_t K>
std::vector<size_t> getValidIndices(const std::vector<Movie>& movies) {
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    for (size_t i = 0; i < movies.size(); ++i) {
        auto pt = extractPoint<K>(movies[i]);
        bool valid = true;
        for (size_t d = 0; d < K; ++d) {
            if (!std::isfinite(pt[d])) {
                valid = false;
                break;
            }
        }
        if (valid) validIdx.push_back(i);
    }

    return validIdx;
}

// Get bounds for K dimensions
template <size_t K>
void getBounds(const std::vector<Movie>& movies, const std::vector<size_t>& validIdx,
               std::array<double, K>& minBounds, std::array<double, K>& maxBounds) {
    for (size_t d = 0; d < K; ++d) {
        minBounds[d] = std::numeric_limits<double>::infinity();
        maxBounds[d] = -std::numeric_limits<double>::infinity();
    }

    for (size_t idx : validIdx) {
        auto pt = extractPoint<K>(movies[idx]);
        for (size_t d = 0; d < K; ++d) {
            minBounds[d] = std::min(minBounds[d], pt[d]);
            maxBounds[d] = std::max(maxBounds[d], pt[d]);
        }
    }
}

// ============================================================================
// KDTree Benchmark for K dimensions
// ============================================================================
template <size_t K>
void benchmark_KDTree(const std::vector<Movie>& movies, int num_queries = 1000, int k = 15) {
    auto validIdx = getValidIndices<K>(movies);
    if (validIdx.empty()) {
        std::cerr << "KDTree<" << K << ">: No valid points\n";
        return;
    }

    std::array<double, K> minBounds, maxBounds;
    getBounds<K>(movies, validIdx, minBounds, maxBounds);

    // Build tree
    KDTree<K> kd;
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        kd.insert(extractPoint<K>(movies[idx]), const_cast<Movie*>(&movies[idx]));
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();
    long long build_us = std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    // Create span and half-ranges for range queries
    std::array<std::uniform_real_distribution<double>, K> halfDists;
    for (size_t d = 0; d < K; ++d) {
        double span = std::max(maxBounds[d] - minBounds[d], 1.0);
        halfDists[d] = std::uniform_real_distribution<double>(0.001 * span, 0.01 * span);
    }

    std::vector<double> knn_times, range_times, search_times;
    knn_times.reserve(num_queries);
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0;

    // kNN benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& base = movies[validIdx[pick(rng)]];
        Movie noisy = base;
        addSmallNoise(noisy, 0.30, 0.50);
        auto target = extractPoint<K>(noisy);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = kd.kNNSearch(k, target);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times.push_back(us);
        sink += nn.size();
    }

    // Range benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        auto center = extractPoint<K>(c);

        std::array<double, K> lower, upper;
        for (size_t d = 0; d < K; ++d) {
            double h = halfDists[d](rng);
            lower[d] = std::clamp(center[d] - h, minBounds[d], maxBounds[d]);
            upper[d] = std::clamp(center[d] + h, minBounds[d], maxBounds[d]);
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = kd.rangeSearch(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += res.size();
    }

    // Search benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& m = movies[validIdx[pick(rng)]];
        auto point = extractPoint<K>(m);

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = kd.search(point);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times.push_back(us);
        sink += (found ? 1 : 0);
    }

    // Compute means
    double knn_mean = std::accumulate(knn_times.begin(), knn_times.end(), 0.0) / knn_times.size();
    double range_mean = std::accumulate(range_times.begin(), range_times.end(), 0.0) / range_times.size();
    double search_mean = std::accumulate(search_times.begin(), search_times.end(), 0.0) / search_times.size();

    results.push_back({"KDTree", (int)K, "knn", build_us, knn_mean, num_queries});
    results.push_back({"KDTree", (int)K, "range", build_us, range_mean, num_queries});
    results.push_back({"KDTree", (int)K, "search", build_us, search_mean, num_queries});

    std::cout << "KDTree<" << K << "> build: " << build_us << " us, kNN: " << knn_mean
              << " us, range: " << range_mean << " us, search: " << search_mean << " us\n";
}

// ============================================================================
// RTree Benchmark for K dimensions
// ============================================================================
template <size_t K>
void benchmark_RTree(const std::vector<Movie>& movies, int num_queries = 1000, int k = 15) {
    using RT = RTree<K>;
    auto validIdx = getValidIndices<K>(movies);
    if (validIdx.empty()) {
        std::cerr << "RTree<" << K << ">: No valid points\n";
        return;
    }

    std::array<double, K> minBounds, maxBounds;
    getBounds<K>(movies, validIdx, minBounds, maxBounds);

    // Build tree
    RT tree;
    auto makePointRect = [](const std::array<double, K>& pt) {
        typename RT::Rect r;
        r.minPoint = pt;
        r.maxPoint = pt;
        return r;
    };

    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        tree.insert(makePointRect(extractPoint<K>(movies[idx])), const_cast<Movie*>(&movies[idx]));
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();
    long long build_us = std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    std::array<std::uniform_real_distribution<double>, K> halfDists;
    for (size_t d = 0; d < K; ++d) {
        double span = std::max(maxBounds[d] - minBounds[d], 1.0);
        halfDists[d] = std::uniform_real_distribution<double>(0.001 * span, 0.01 * span);
    }

    std::vector<double> knn_times, range_times, search_times;
    knn_times.reserve(num_queries);
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0;

    // kNN benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& base = movies[validIdx[pick(rng)]];
        Movie noisy = base;
        addSmallNoise(noisy, 0.30, 0.50);
        auto target = extractPoint<K>(noisy);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = tree.kNearest(target, k);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times.push_back(us);
        sink += nn.size();
    }

    // Range benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        auto center = extractPoint<K>(c);

        typename RT::Rect q;
        for (size_t d = 0; d < K; ++d) {
            double h = halfDists[d](rng);
            q.minPoint[d] = std::clamp(center[d] - h, minBounds[d], maxBounds[d]);
            q.maxPoint[d] = std::clamp(center[d] + h, minBounds[d], maxBounds[d]);
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = tree.rangeQuery(q);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += res.size();
    }

    // Search benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& m = movies[validIdx[pick(rng)]];
        auto targetRect = makePointRect(extractPoint<K>(m));

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = tree.search(targetRect);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times.push_back(us);
        sink += (found ? 1 : 0);
    }

    double knn_mean = std::accumulate(knn_times.begin(), knn_times.end(), 0.0) / knn_times.size();
    double range_mean = std::accumulate(range_times.begin(), range_times.end(), 0.0) / range_times.size();
    double search_mean = std::accumulate(search_times.begin(), search_times.end(), 0.0) / search_times.size();

    results.push_back({"RTree", (int)K, "knn", build_us, knn_mean, num_queries});
    results.push_back({"RTree", (int)K, "range", build_us, range_mean, num_queries});
    results.push_back({"RTree", (int)K, "search", build_us, search_mean, num_queries});

    std::cout << "RTree<" << K << "> build: " << build_us << " us, kNN: " << knn_mean
              << " us, range: " << range_mean << " us, search: " << search_mean << " us\n";
}

// ============================================================================
// QuadTree Benchmark (k=2 only)
// ============================================================================
void benchmark_QuadTree(const std::vector<Movie>& movies, int num_queries = 1000, int k = 15) {
    auto validIdx = getValidIndices<2>(movies);
    if (validIdx.empty()) {
        std::cerr << "QuadTree: No valid points\n";
        return;
    }

    std::array<double, 2> minBounds, maxBounds;
    getBounds<2>(movies, validIdx, minBounds, maxBounds);

    const double padX = 1.0;
    const double padY = 1e-6;
    QuadTree::Boundary world(minBounds[0] - padX, maxBounds[0] + padX,
                             minBounds[1] - padY, maxBounds[1] + padY);
    QuadTree qt(world);

    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        qt.insert({movies[idx].revenue, movies[idx].runtime}, const_cast<Movie*>(&movies[idx]));
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();
    long long build_us = std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxBounds[0] - minBounds[0], 1.0);
    const double spanY = std::max(maxBounds[1] - minBounds[1], 1.0);
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    std::vector<double> knn_times, range_times, search_times;
    knn_times.reserve(num_queries);
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0;

    // kNN
    for (int i = 0; i < num_queries; ++i) {
        const Movie& base = movies[validIdx[pick(rng)]];
        Movie noisy = base;
        addSmallNoise(noisy, 0.30, 0.50);
        std::array<double, 2> target = {noisy.revenue, noisy.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = qt.kNNSearch(k, target);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times.push_back(us);
        sink += nn.size();
    }

    // Range
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        double cx = c.revenue, cy = c.runtime;
        double hx = halfXDist(rng), hy = halfYDist(rng);

        std::array<double, 2> lower = {
            std::clamp(cx - hx, minBounds[0] - padX, maxBounds[0] + padX),
            std::clamp(cy - hy, minBounds[1] - padY, maxBounds[1] + padY)
        };
        std::array<double, 2> upper = {
            std::clamp(cx + hx, minBounds[0] - padX, maxBounds[0] + padX),
            std::clamp(cy + hy, minBounds[1] - padY, maxBounds[1] + padY)
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = qt.rangeQuery(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += res.size();
    }

    // Search
    for (int i = 0; i < num_queries; ++i) {
        const Movie& m = movies[validIdx[pick(rng)]];
        std::array<double, 2> target = {m.revenue, m.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = qt.search(target);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times.push_back(us);
        sink += (found ? 1 : 0);
    }

    double knn_mean = std::accumulate(knn_times.begin(), knn_times.end(), 0.0) / knn_times.size();
    double range_mean = std::accumulate(range_times.begin(), range_times.end(), 0.0) / range_times.size();
    double search_mean = std::accumulate(search_times.begin(), search_times.end(), 0.0) / search_times.size();

    results.push_back({"QuadTree", 2, "knn", build_us, knn_mean, num_queries});
    results.push_back({"QuadTree", 2, "range", build_us, range_mean, num_queries});
    results.push_back({"QuadTree", 2, "search", build_us, search_mean, num_queries});

    std::cout << "QuadTree build: " << build_us << " us, kNN: " << knn_mean
              << " us, range: " << range_mean << " us, search: " << search_mean << " us\n";
}

// ============================================================================
// RangeTree2D Benchmark (k=2 only, no kNN)
// ============================================================================
void benchmark_RangeTree2D(const std::vector<Movie>& movies, int num_queries = 1000) {
    auto validIdx = getValidIndices<2>(movies);
    if (validIdx.empty()) {
        std::cerr << "RangeTree2D: No valid points\n";
        return;
    }

    std::array<double, 2> minBounds, maxBounds;
    getBounds<2>(movies, validIdx, minBounds, maxBounds);

    RangeTree2D rt;
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        rt.insert({movies[idx].revenue, movies[idx].runtime}, const_cast<Movie*>(&movies[idx]));
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();
    long long build_us = std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxBounds[0] - minBounds[0], 1.0);
    const double spanY = std::max(maxBounds[1] - minBounds[1], 1.0);
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    std::vector<double> range_times, search_times;
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0;

    // Range
    for (int q = 0; q < num_queries; ++q) {
        const Movie& c = movies[validIdx[pick(rng)]];
        double cx = c.revenue, cy = c.runtime;
        double hx = halfXDist(rng), hy = halfYDist(rng);

        std::array<double, 2> lower = {
            std::clamp(cx - hx, minBounds[0], maxBounds[0]),
            std::clamp(cy - hy, minBounds[1], maxBounds[1])
        };
        std::array<double, 2> upper = {
            std::clamp(cx + hx, minBounds[0], maxBounds[0]),
            std::clamp(cy + hy, minBounds[1], maxBounds[1])
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = rt.rangeSearch(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += res.size();
    }

    // Search
    for (int q = 0; q < num_queries; ++q) {
        const Movie& m = movies[validIdx[pick(rng)]];
        std::array<double, 2> p = {m.revenue, m.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = rt.search(p);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times.push_back(us);
        sink += (found ? 1 : 0);
    }

    double range_mean = std::accumulate(range_times.begin(), range_times.end(), 0.0) / range_times.size();
    double search_mean = std::accumulate(search_times.begin(), search_times.end(), 0.0) / search_times.size();

    results.push_back({"RangeTree2D", 2, "range", build_us, range_mean, num_queries});
    results.push_back({"RangeTree2D", 2, "search", build_us, search_mean, num_queries});

    std::cout << "RangeTree2D build: " << build_us << " us, range: " << range_mean
              << " us, search: " << search_mean << " us\n";
}

// ============================================================================
// LSH Benchmark
// ============================================================================
static std::string makeLSHText(const Movie& m) {
    std::string t = m.title;
    if (!m.genre_names.empty()) t += " " + m.genre_names;
    if (!m.production_company_names.empty()) t += " " + m.production_company_names;
    if (!m.original_language.empty()) t += " " + m.original_language;
    if (!m.origin_country.empty()) t += " " + m.origin_country;
    return t;
}

void benchmark_LSH(std::vector<Movie>& movies, int num_queries = 1000, int topK = 5) {
    std::vector<std::string> texts(movies.size());
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    for (size_t i = 0; i < movies.size(); ++i) {
        texts[i] = makeLSHText(movies[i]);
        if (!texts[i].empty()) validIdx.push_back(i);
    }

    if (validIdx.empty()) {
        std::cerr << "LSH: No valid texts\n";
        return;
    }

    MinHashLSH lsh(120, 20, 2, 42);

    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        lsh.addDocument(&movies[idx], texts[idx]);
    }
    lsh.buildIndex();
    auto build_t1 = std::chrono::high_resolution_clock::now();
    long long build_us = std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    std::vector<double> query_times;
    query_times.reserve(num_queries);

    volatile size_t sink = 0;

    for (int i = 0; i < num_queries; ++i) {
        const std::string& qtext = texts[validIdx[pick(rng)]];

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = lsh.query(qtext, topK, 0.0);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        query_times.push_back(us);
        sink += res.size();
    }

    double query_mean = std::accumulate(query_times.begin(), query_times.end(), 0.0) / query_times.size();

    results.push_back({"LSH", 0, "query", build_us, query_mean, num_queries});

    std::cout << "LSH build: " << build_us << " us, query: " << query_mean << " us\n";
}

// ============================================================================
// Main benchmark runner
// ============================================================================
void run_comprehensive_benchmarks(std::vector<Movie>& movies) {
    std::cout << "\n=== Starting Comprehensive Benchmarks ===\n\n";

    const int num_queries = 1000;
    const int k = 15;

    // Test k=2
    std::cout << "\n--- K=2 Benchmarks ---\n";
    benchmark_KDTree<2>(movies, num_queries, k);
    benchmark_RTree<2>(movies, num_queries, k);
    benchmark_QuadTree(movies, num_queries, k);
    benchmark_RangeTree2D(movies, num_queries);

    // Test k=3
    std::cout << "\n--- K=3 Benchmarks ---\n";
    benchmark_KDTree<3>(movies, num_queries, k);
    benchmark_RTree<3>(movies, num_queries, k);

    // Test k=4
    std::cout << "\n--- K=4 Benchmarks ---\n";
    benchmark_KDTree<4>(movies, num_queries, k);
    benchmark_RTree<4>(movies, num_queries, k);

    // Test k=5
    std::cout << "\n--- K=5 Benchmarks ---\n";
    benchmark_KDTree<5>(movies, num_queries, k);
    benchmark_RTree<5>(movies, num_queries, k);

    // LSH (text-based, not dimension-specific)
    std::cout << "\n--- LSH Benchmark ---\n";
    benchmark_LSH(movies, num_queries, 5);

    // Save results to CSV
    std::ofstream out("comprehensive_benchmark.csv");
    out << "structure,k,query_type,build_time_us,mean_query_us,num_queries\n";
    for (const auto& r : results) {
        out << r.structure_name << "," << r.k << "," << r.query_type << ","
            << r.build_time_us << "," << r.mean_query_us << "," << r.num_queries << "\n";
    }
    out.close();

    std::cout << "\n=== Benchmark complete! Results saved to comprehensive_benchmark.csv ===\n";
}
