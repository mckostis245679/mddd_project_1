#include <string>
#include <vector>

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
    int runtime;
    double popularity;
    double vote_average;
    int vote_count;
};


std::vector<Movie> readMoviesCSV(const std::string& filename);