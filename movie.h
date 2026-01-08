#include <string>
#include <vector>
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