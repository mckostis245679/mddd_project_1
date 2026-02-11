#include <string>
#include <vector>
#include <limits>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#pragma once
struct Movie {
    int id;
    std::string title;
    bool adult;
    std::string original_language;
    std::string origin_country;
    std::string release_date;
    std::string genre_names;
    std::string production_company_names;
    double budget;
    double revenue;
    double runtime;
    double popularity;
    double vote_average;
    double vote_count;
};


std::vector<Movie> readMoviesCSV(const std::string& filename);


struct MovieNumericMinMax {
    int id_min, id_max;

    double budget_min, budget_max;
    double revenue_min, revenue_max;
    double runtime_min, runtime_max;
    double popularity_min, popularity_max;
    double vote_average_min, vote_average_max;
    double vote_count_min, vote_count_max;
};

inline MovieNumericMinMax findMovieNumericMinMax(const std::vector<Movie>& movies) {
    if (movies.empty()) {
        throw std::invalid_argument("findMovieNumericMinMax: movies vector is empty.");
    }

    MovieNumericMinMax mm;

    mm.id_min = std::numeric_limits<int>::max();
    mm.id_max = std::numeric_limits<int>::min();

    mm.budget_min = std::numeric_limits<double>::infinity();
    mm.budget_max = -std::numeric_limits<double>::infinity();

    mm.revenue_min = std::numeric_limits<double>::infinity();
    mm.revenue_max = -std::numeric_limits<double>::infinity();

    mm.runtime_min = std::numeric_limits<double>::infinity();
    mm.runtime_max = -std::numeric_limits<double>::infinity();

    mm.popularity_min = std::numeric_limits<double>::infinity();
    mm.popularity_max = -std::numeric_limits<double>::infinity();

    mm.vote_average_min = std::numeric_limits<double>::infinity();
    mm.vote_average_max = -std::numeric_limits<double>::infinity();

    mm.vote_count_min = std::numeric_limits<double>::infinity();
    mm.vote_count_max = -std::numeric_limits<double>::infinity();

    for (const auto& m : movies) {
        mm.id_min = std::min(mm.id_min, m.id);
        mm.id_max = std::max(mm.id_max, m.id);

        mm.budget_min = std::min(mm.budget_min, m.budget);
        mm.budget_max = std::max(mm.budget_max, m.budget);

        mm.revenue_min = std::min(mm.revenue_min, m.revenue);
        mm.revenue_max = std::max(mm.revenue_max, m.revenue);

        mm.runtime_min = std::min(mm.runtime_min, m.runtime);
        mm.runtime_max = std::max(mm.runtime_max, m.runtime);

        mm.popularity_min = std::min(mm.popularity_min, m.popularity);
        mm.popularity_max = std::max(mm.popularity_max, m.popularity);

        mm.vote_average_min = std::min(mm.vote_average_min, m.vote_average);
        mm.vote_average_max = std::max(mm.vote_average_max, m.vote_average);

        mm.vote_count_min = std::min(mm.vote_count_min, m.vote_count);
        mm.vote_count_max = std::max(mm.vote_count_max, m.vote_count);
    }

    return mm;
}