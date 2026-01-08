#include <fstream>
#include <sstream>
#include <vector>
#include "movie.h"

std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> result;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            result.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(field);
    return result;
}


std::vector<Movie> readMoviesCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file");
    }

    std::vector<Movie> movies;
    std::string line;

    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        auto cols = parseCSVLine(line);
        if (cols.size() != 14) continue; // safety check

        Movie m;
        m.id = std::stoi(cols[0]);
        m.title = cols[1];
        m.adult = (cols[2] == "TRUE");
        m.original_language = cols[3];
        m.origin_country = cols[4];
        m.release_date = cols[5];
        m.genre_names = cols[6];
        m.production_company_names = cols[7];
        m.budget = std::stod(cols[8]);
        m.revenue = std::stod(cols[9]);
        m.runtime = std::stod(cols[10]);
        m.popularity = std::stod(cols[11]);
        m.vote_average = std::stod(cols[12]);
        m.vote_count = std::stod(cols[13]);

        movies.push_back(std::move(m));
    }

    return movies;
}