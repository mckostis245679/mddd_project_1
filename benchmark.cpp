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


struct BenchStats {
    double min_us = 0.0;
    double max_us = 0.0;
    double mean_us = 0.0;
};


static BenchStats computeStats(const std::vector<double>& t) {
    BenchStats s;
    if (t.empty()) return s;

    auto [mn, mx] = std::minmax_element(t.begin(), t.end());
    s.min_us = *mn;
    s.max_us = *mx;
    s.mean_us = std::accumulate(t.begin(), t.end(), 0.0) / static_cast<double>(t.size());
    return s;
}

// 30% chance per field by default
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


inline void QuadTree_execution_time(const std::vector<Movie>& movies) {
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }

    // 1) Collect finite points + compute bounds
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < movies.size(); ++i) {
        const double x = movies[i].revenue;
        const double y = movies[i].runtime;
        if (!std::isfinite(x) || !std::isfinite(y)) continue;

        validIdx.push_back(i);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    if (validIdx.empty()) {
        std::cerr << "No finite (revenue, runtime) points.\n";
        return;
    }

    // Small pad so boundary contains edge points robustly
    const double padX = 1.0;
    const double padY = 1e-6;
    QuadTree::Boundary world(minX - padX, maxX + padX, minY - padY, maxY + padY);
    QuadTree qt(world);

    // 2) Build tree + time
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        qt.insert({movies[idx].revenue, movies[idx].runtime},
                  const_cast<Movie*>(&movies[idx]));
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();

    const long long build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    // 3) Benchmark settings
    const int num_queries = 1000;
    const int k = 15;

    std::mt19937_64 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxX - minX, 1.0);
    const double spanY = std::max(maxY - minY, 1.0);
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    std::vector<double> knn_times, range_times, search_times;
    knn_times.reserve(num_queries);
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0; // prevent aggressive optimization removal

    // 4) kNN benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& base = movies[validIdx[pick(rng)]];
        Movie noisy = base;
        addSmallNoise(noisy, 0.30, 0.50); // 30% chance per field

        std::array<double, 2> target = {noisy.revenue, noisy.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = qt.kNNSearch(k, target);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times.push_back(us);
        sink += nn.size();
    }

    // 5) Range benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        double cx = c.revenue;
        double cy = c.runtime;

        double hx = halfXDist(rng);
        double hy = halfYDist(rng);

        std::array<double, 2> lower = {
            std::clamp(cx - hx, minX - padX, maxX + padX),
            std::clamp(cy - hy, minY - padY, maxY + padY)
        };
        std::array<double, 2> upper = {
            std::clamp(cx + hx, minX - padX, maxX + padX),
            std::clamp(cy + hy, minY - padY, maxY + padY)
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = qt.rangeQuery(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += res.size();
    }

    // 6) Exact search benchmark
    // NOTE: search() checks exact equality on doubles, so use original stored coordinates.
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

    // 7) Stats
    BenchStats knn = computeStats(knn_times);
    BenchStats range = computeStats(range_times);
    BenchStats search = computeStats(search_times);

    // 8) Console
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Build time: " << build_us << " us\n";
    std::cout << "kNN   -> mean: " << knn.mean_us   << " us, min: " << knn.min_us   << ", max: " << knn.max_us   << "\n";
    std::cout << "Range -> mean: " << range.mean_us << " us, min: " << range.min_us << ", max: " << range.max_us << "\n";
    std::cout << "Search-> mean: " << search.mean_us<< " us, min: " << search.min_us<< ", max: " << search.max_us<< "\n";
    std::cout << "Sink(ignore): " << sink << "\n";

    // 9) CSV output
    std::ofstream out("quadtree_benchmark.csv");
    if (!out) {
        std::cerr << "Failed to open quadtree_benchmark.csv for writing.\n";
        return;
    }

    out << "query_type,num_queries,build_time_us,min_query_us,max_query_us,mean_query_us\n";
    out << "knn,"    << num_queries << "," << build_us << "," << knn.min_us    << "," << knn.max_us    << "," << knn.mean_us    << "\n";
    out << "range,"  << num_queries << "," << build_us << "," << range.min_us  << "," << range.max_us  << "," << range.mean_us  << "\n";
    out << "search," << num_queries << "," << build_us << "," << search.min_us << "," << search.max_us << "," << search.mean_us << "\n";

    std::cout << "Saved: quadtree_benchmark.csv\n";
}


inline void KDTree_execution_time(std::vector<Movie>& movies) {
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }

    // Collect valid points + bounds
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < movies.size(); ++i) {
        double x = movies[i].revenue;
        double y = movies[i].runtime;
        if (!std::isfinite(x) || !std::isfinite(y)) continue;

        validIdx.push_back(i);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    if (validIdx.empty()) {
        std::cerr << "No finite (revenue, runtime) points.\n";
        return;
    }

    KDTree<2> kd;

    // Build timing
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        kd.insert({movies[idx].revenue, movies[idx].runtime}, &movies[idx]);
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();

    const long long build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    const int num_queries = 1000;
    const int k = 15;

    std::mt19937_64 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxX - minX, 1.0);
    const double spanY = std::max(maxY - minY, 1.0);
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    std::vector<double> knn_times, range_times, search_times;
    knn_times.reserve(num_queries);
    range_times.reserve(num_queries);
    search_times.reserve(num_queries);

    volatile size_t sink = 0; // prevent optimization removing query calls

    // 1) kNN benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& base = movies[validIdx[pick(rng)]];
        Movie noisy = base;
        addSmallNoise(noisy, 0.30, 0.50); // 30% chance per field

        std::array<double, 2> target = {noisy.revenue, noisy.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = kd.kNNSearch(k, target);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times.push_back(us);
        sink += nn.size();
    }

    // 2) rangeSearch benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        double cx = c.revenue;
        double cy = c.runtime;

        double hx = halfXDist(rng);
        double hy = halfYDist(rng);

        std::array<double, 2> lower = {
            std::clamp(cx - hx, minX, maxX),
            std::clamp(cy - hy, minY, maxY)
        };
        std::array<double, 2> upper = {
            std::clamp(cx + hx, minX, maxX),
            std::clamp(cy + hy, minY, maxY)
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        auto results = kd.rangeSearch(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times.push_back(us);
        sink += results.size();
    }

    // 3) exact search benchmark (exact coordinates, no noise)
    for (int i = 0; i < num_queries; ++i) {
        const Movie& m = movies[validIdx[pick(rng)]];
        std::array<double, 2> point = {m.revenue, m.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = kd.search(point);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times.push_back(us);
        sink += (found ? 1 : 0);
    }

    BenchStats knn   = computeStats(knn_times);
    BenchStats range = computeStats(range_times);
    BenchStats srch  = computeStats(search_times);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "KDTree build time: " << build_us << " us\n";
    std::cout << "kNN   -> mean: " << knn.mean_us   << " us, min: " << knn.min_us   << ", max: " << knn.max_us   << "\n";
    std::cout << "Range -> mean: " << range.mean_us << " us, min: " << range.min_us << ", max: " << range.max_us << "\n";
    std::cout << "Search-> mean: " << srch.mean_us  << " us, min: " << srch.min_us  << ", max: " << srch.max_us  << "\n";
    std::cout << "Sink(ignore): " << sink << "\n";

    std::ofstream out("kdtree_benchmark.csv");
    if (!out) {
        std::cerr << "Failed to open kdtree_benchmark.csv for writing.\n";
        return;
    }

    out << "query_type,num_queries,build_time_us,min_query_us,max_query_us,mean_query_us\n";
    out << "knn,"    << num_queries << "," << build_us << "," << knn.min_us   << "," << knn.max_us   << "," << knn.mean_us   << "\n";
    out << "range,"  << num_queries << "," << build_us << "," << range.min_us << "," << range.max_us << "," << range.mean_us << "\n";
    out << "search," << num_queries << "," << build_us << "," << srch.min_us  << "," << srch.max_us  << "," << srch.mean_us  << "\n";

    std::cout << "Saved: kdtree_benchmark.csv\n";
}



inline void RangeTree2D_execution_time(std::vector<Movie>& movies, int num_queries = 1000) {
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }

    // 1) Collect valid points + bounds
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < movies.size(); ++i) {
        const double x = movies[i].revenue;
        const double y = movies[i].runtime;
        if (!std::isfinite(x) || !std::isfinite(y)) continue;

        validIdx.push_back(i);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    if (validIdx.empty()) {
        std::cerr << "No finite (revenue, runtime) points found.\n";
        return;
    }

    // 2) Build tree + timing
    RangeTree2D rt;
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        rt.insert({movies[idx].revenue, movies[idx].runtime}, &movies[idx]);
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();

    const long long build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    // 3) Query setup
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxX - minX, 1.0);
    const double spanY = std::max(maxY - minY, 1.0);
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    std::vector<double> search_times_us;
    std::vector<double> range_times_us;
    search_times_us.reserve(num_queries);
    range_times_us.reserve(num_queries);

    volatile size_t sink = 0; // keep calls from being optimized away

    // 4) Exact search benchmark
    for (int q = 0; q < num_queries; ++q) {
        const Movie& m = movies[validIdx[pick(rng)]];
        std::array<double, 2> p = {m.revenue, m.runtime};

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = rt.search(p);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times_us.push_back(us);
        sink += (found ? 1 : 0);
    }

    // 5) Range search benchmark
    for (int q = 0; q < num_queries; ++q) {
        const Movie& center = movies[validIdx[pick(rng)]];
        const double cx = center.revenue;
        const double cy = center.runtime;

        const double hx = halfXDist(rng);
        const double hy = halfYDist(rng);

        std::array<double, 2> lower = {
            std::clamp(cx - hx, minX, maxX),
            std::clamp(cy - hy, minY, maxY)
        };
        std::array<double, 2> upper = {
            std::clamp(cx + hx, minX, maxX),
            std::clamp(cy + hy, minY, maxY)
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        std::vector<Movie*> res = rt.rangeSearch(lower, upper);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times_us.push_back(us);
        sink += res.size();
    }

    // 6) Stats
    auto computeStats = [](const std::vector<double>& t) {
        struct S { double mn, mx, mean; };
        if (t.empty()) return S{0.0, 0.0, 0.0};
        auto [mnIt, mxIt] = std::minmax_element(t.begin(), t.end());
        double mean = std::accumulate(t.begin(), t.end(), 0.0) / static_cast<double>(t.size());
        return S{*mnIt, *mxIt, mean};
    };

    auto sSearch = computeStats(search_times_us);
    auto sRange  = computeStats(range_times_us);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "RangeTree2D build time: " << build_us << " us\n";
    std::cout << "Search -> mean: " << sSearch.mean << " us, min: " << sSearch.mn << ", max: " << sSearch.mx << "\n";
    std::cout << "Range  -> mean: " << sRange.mean  << " us, min: " << sRange.mn  << ", max: " << sRange.mx  << "\n";
    std::cout << "Sink(ignore): " << sink << "\n";

    // 7) Save summary CSV (time only)
    std::ofstream out("rangetree2d_benchmark.csv");
    if (!out) {
        std::cerr << "Failed to open rangetree2d_benchmark.csv for writing.\n";
        return;
    }

    out << "query_type,num_queries,build_time_us,min_query_us,max_query_us,mean_query_us\n";
    out << "search," << num_queries << "," << build_us << ","
        << sSearch.mn << "," << sSearch.mx << "," << sSearch.mean << "\n";
    out << "range,"  << num_queries << "," << build_us << ","
        << sRange.mn << "," << sRange.mx << "," << sRange.mean << "\n";

    out.close();
    std::cout << "Saved: rangetree2d_benchmark.csv\n";
}



inline void RTree_execution_time(std::vector<Movie>& movies, int num_queries = 1000, int k = 15) {
    using RT = RTree<2>;
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }
    if (num_queries <= 0) {
        std::cerr << "num_queries must be > 0.\n";
        return;
    }
    if (k <= 0) {
        std::cerr << "k must be > 0.\n";
        return;
    }

    // 1) Collect valid points + bounds
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < movies.size(); ++i) {
        const double x = movies[i].revenue;
        const double y = movies[i].runtime;
        if (!std::isfinite(x) || !std::isfinite(y)) continue;

        validIdx.push_back(i);
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    if (validIdx.empty()) {
        std::cerr << "No finite (revenue, runtime) points found.\n";
        return;
    }

    // 2) Build tree + time
    RT tree;

    auto makePointRect = [](double x, double y) {
        typename RT::Rect r;
        r.minPoint = {x, y};
        r.maxPoint = {x, y};
        return r;
    };

    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        tree.insert(makePointRect(movies[idx].revenue, movies[idx].runtime), &movies[idx]);
    }
    auto build_t1 = std::chrono::high_resolution_clock::now();

    const long long build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    // 3) Query setup
    std::mt19937_64 rng(42); // reproducible
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    const double spanX = std::max(maxX - minX, 1.0);
    const double spanY = std::max(maxY - minY, 1.0);

    // range window half-sizes: 0.1% to 1% of span
    std::uniform_real_distribution<double> halfXDist(0.001 * spanX, 0.01 * spanX);
    std::uniform_real_distribution<double> halfYDist(0.001 * spanY, 0.01 * spanY);

    // knn targets sampled across full bounds
    std::uniform_real_distribution<double> qx(minX, maxX);
    std::uniform_real_distribution<double> qy(minY, maxY);

    std::vector<double> search_times_us, range_times_us, knn_times_us;
    search_times_us.reserve(num_queries);
    range_times_us.reserve(num_queries);
    knn_times_us.reserve(num_queries);

    volatile size_t sink = 0; // prevents optimizer removing work

    // 4) Exact search benchmark (point existing in tree)
    for (int i = 0; i < num_queries; ++i) {
        const Movie& m = movies[validIdx[pick(rng)]];
        auto targetRect = makePointRect(m.revenue, m.runtime);

        auto t0 = std::chrono::high_resolution_clock::now();
        Movie* found = tree.search(targetRect);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        search_times_us.push_back(us);
        sink += (found ? 1 : 0);
    }

    // 5) Range benchmark
    for (int i = 0; i < num_queries; ++i) {
        const Movie& c = movies[validIdx[pick(rng)]];
        const double cx = c.revenue;
        const double cy = c.runtime;

        const double hx = halfXDist(rng);
        const double hy = halfYDist(rng);

        typename RT::Rect q;
        q.minPoint = {
            std::clamp(cx - hx, minX, maxX),
            std::clamp(cy - hy, minY, maxY)
        };
        q.maxPoint = {
            std::clamp(cx + hx, minX, maxX),
            std::clamp(cy + hy, minY, maxY)
        };

        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = tree.rangeQuery(q);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        range_times_us.push_back(us);
        sink += res.size();
    }

    // 6) kNN benchmark
    for (int i = 0; i < num_queries; ++i) {
        std::array<double, 2> target = {qx(rng), qy(rng)};

        auto t0 = std::chrono::high_resolution_clock::now();
        auto nn = tree.kNearest(target, k);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        knn_times_us.push_back(us);
        sink += nn.size();
    }

    // 7) Stats
    BenchStats s_search = computeStats(search_times_us);
    BenchStats s_range  = computeStats(range_times_us);
    BenchStats s_knn    = computeStats(knn_times_us);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "RTree build time: " << build_us << " us\n";
    std::cout << "Search -> mean: " << s_search.mean_us
              << " us, min: " << s_search.min_us
              << ", max: " << s_search.max_us << "\n";
    std::cout << "Range  -> mean: " << s_range.mean_us
              << " us, min: " << s_range.min_us
              << ", max: " << s_range.max_us << "\n";
    std::cout << "kNN    -> mean: " << s_knn.mean_us
              << " us, min: " << s_knn.min_us
              << ", max: " << s_knn.max_us << "\n";
    std::cout << "Sink(ignore): " << sink << "\n";

    // 8) CSV (time only)
    std::ofstream out("rtree_benchmark.csv");
    if (!out) {
        std::cerr << "Failed to open rtree_benchmark.csv for writing.\n";
        return;
    }

    out << "query_type,num_queries,k,build_time_us,min_query_us,max_query_us,mean_query_us\n";
    out << "search," << num_queries << ",0," << build_us << ","
        << s_search.min_us << "," << s_search.max_us << "," << s_search.mean_us << "\n";
    out << "range,"  << num_queries << ",0," << build_us << ","
        << s_range.min_us  << "," << s_range.max_us  << "," << s_range.mean_us  << "\n";
    out << "knn,"    << num_queries << "," << k << "," << build_us << ","
        << s_knn.min_us    << "," << s_knn.max_us    << "," << s_knn.mean_us    << "\n";

    out.close();
    std::cout << "Saved: rtree_benchmark.csv\n";
}




static std::string makeLSHText(const Movie& m) {
    // Keep consistent text representation for indexing/querying
    std::string t = m.title;
    if (!m.genre_names.empty()) t += " " + m.genre_names;
    if (!m.production_company_names.empty()) t += " " + m.production_company_names;
    if (!m.original_language.empty()) t += " " + m.original_language;
    if (!m.origin_country.empty()) t += " " + m.origin_country;
    return t;
}

inline void LSH_execution_time(std::vector<Movie>& movies,
                        int num_queries = 1000,
                        int topK = 5,
                        double minJaccard = 0.0,
                        int numHashes = 120,
                        int numBands = 20,
                        int shingleSize = 2,
                        uint64_t seed = 42) {
    if (movies.empty()) {
        std::cerr << "No movies loaded.\n";
        return;
    }
    if (num_queries <= 0) {
        std::cerr << "num_queries must be > 0.\n";
        return;
    }

    // Build document texts + valid indices
    std::vector<std::string> texts(movies.size());
    std::vector<size_t> validIdx;
    validIdx.reserve(movies.size());

    for (size_t i = 0; i < movies.size(); ++i) {
        texts[i] = makeLSHText(movies[i]);
        if (!texts[i].empty()) validIdx.push_back(i);
    }

    if (validIdx.empty()) {
        std::cerr << "No non-empty texts for LSH.\n";
        return;
    }

    MinHashLSH lsh(numHashes, numBands, shingleSize, seed);

    // 1) Build timing (add docs + build index)
    auto build_t0 = std::chrono::high_resolution_clock::now();
    for (size_t idx : validIdx) {
        lsh.addDocument(&movies[idx], texts[idx]);
    }
    lsh.buildIndex();
    auto build_t1 = std::chrono::high_resolution_clock::now();

    const long long build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(build_t1 - build_t0).count();

    // 2) Query timing
    std::mt19937_64 rng(seed + 1);
    std::uniform_int_distribution<size_t> pick(0, validIdx.size() - 1);

    std::vector<double> query_times_us;
    query_times_us.reserve(num_queries);

    volatile size_t sink = 0; // prevent optimization removing queries

    for (int i = 0; i < num_queries; ++i) {
        const std::string& qtext = texts[validIdx[pick(rng)]];

        auto t0 = std::chrono::high_resolution_clock::now();
        auto results = lsh.query(qtext, topK, minJaccard);
        auto t1 = std::chrono::high_resolution_clock::now();

        double us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
        query_times_us.push_back(us);

        sink += results.size();
    }

    // 3) Stats
    auto [mnIt, mxIt] = std::minmax_element(query_times_us.begin(), query_times_us.end());
    const double min_us = (mnIt != query_times_us.end()) ? *mnIt : 0.0;
    const double max_us = (mxIt != query_times_us.end()) ? *mxIt : 0.0;
    const double mean_us =
        query_times_us.empty()
            ? 0.0
            : std::accumulate(query_times_us.begin(), query_times_us.end(), 0.0) /
                  static_cast<double>(query_times_us.size());

    std::cout << "LSH build time: " << build_us << " us\n";
    std::cout << "Query mean: " << mean_us
              << " us, min: " << min_us
              << ", max: " << max_us << " us\n";
    std::cout << "Sink(ignore): " << sink << "\n";

    // 4) CSV in same tree-style format
    std::ofstream out("lsh_benchmark.csv");
    if (!out) {
        std::cerr << "Failed to open lsh_benchmark.csv for writing.\n";
        return;
    }

    out << "query_type,num_queries,build_time_us,min_query_us,max_query_us,mean_query_us\n";
    out << "query," << num_queries << ","
        << build_us << ","
        << min_us << ","
        << max_us << ","
        << mean_us << "\n";

    std::cout << "Saved: lsh_benchmark.csv\n";
}
