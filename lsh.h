// lsh.h
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "movie.h"

using namespace std;

class MinHashLSH {
public:
    struct QueryResult {
        double jaccardScore;
        Movie* movie;
    };

    //class builder
    MinHashLSH(int numberOfHashes = 120, int numberOfBands = 20, int shingleSize = 2, uint64_t seed = 42)
        : numHashes(numberOfHashes),
          numBands(numberOfBands),
          wordsPerShingle(shingleSize),
          rng(seed) {
        if (numHashes <= 0 || numBands <= 0 || wordsPerShingle <= 0) {
            throw runtime_error("All parameters must be > 0.");
        }
        if (numHashes % numBands != 0) {
            throw runtime_error("numberOfHashes must be divisible by numberOfBands.");
        }

        rowsPerBand = numHashes / numBands;
        initializeHashFunctions();
    }

    // Add a movie document with text used for similarity (e.g. title + genres)
    void addDocument(Movie* moviePtr, const string& text) {
        if (!moviePtr) return;

        Document doc;
        doc.movie = moviePtr;
        doc.shingles = createShingles(text, wordsPerShingle);
        documents.push_back(move(doc));
    }

    void buildIndex() {
        signatures.clear();
        signatures.reserve(documents.size());
        buckets.clear();

        // MinHash signature for each document
        for (const auto& doc : documents) {
            signatures.push_back(createMinHashSignature(doc.shingles));
        }

        // LSH buckets by band
        for (size_t docIndex = 0; docIndex < signatures.size(); ++docIndex) {
            for (int band = 0; band < numBands; ++band) {
                uint64_t bandHashValue = hashBand(signatures[docIndex], band);
                string key = makeBucketKey(band, bandHashValue);
                buckets[key].push_back(static_cast<int>(docIndex));
            }
        }
    }

    vector<QueryResult> query(const string& queryText, int topK = 5, double minJaccard = 0.0) const {
        unordered_set<string> queryShingles = createShingles(queryText, wordsPerShingle);
        vector<uint64_t> querySignature = createMinHashSignature(queryShingles);

        // 1) Candidate generation
        unordered_set<int> candidateIndexes;
        for (int band = 0; band < numBands; ++band) {
            uint64_t bandHashValue = hashBand(querySignature, band);
            string key = makeBucketKey(band, bandHashValue);

            auto bucketIt = buckets.find(key);
            if (bucketIt != buckets.end()) {
                for (int idx : bucketIt->second) {
                    candidateIndexes.insert(idx);
                }
            }
        }

        // 2) Exact Jaccard scoring
        vector<QueryResult> results;
        results.reserve(candidateIndexes.size());

        for (int idx : candidateIndexes) {
            double score = computeJaccard(queryShingles, documents[idx].shingles);
            if (score >= minJaccard) {
                QueryResult qr;
                qr.jaccardScore = score;
                qr.movie = documents[idx].movie;
                results.push_back(qr);
            }
        }

        // 3) Sort by score descending
        sort(results.begin(), results.end(),
             [](const QueryResult& a, const QueryResult& b) {
                 if (a.jaccardScore != b.jaccardScore) {
                     return a.jaccardScore > b.jaccardScore;
                 }
                 if (!a.movie || !b.movie) return a.movie != nullptr;
                 return a.movie->id < b.movie->id;
             });

        // 4) Keep topK
        if (topK >= 0 && static_cast<int>(results.size()) > topK) {
            results.resize(topK);
        }

        return results;
    }

private:
    struct Document {
        Movie* movie = nullptr;
        unordered_set<string> shingles;
    };

    // Parameters
    int numHashes;
    int numBands;
    int rowsPerBand;
    int wordsPerShingle;

    // Universal hash params for MinHash
    vector<uint64_t> hashA;
    vector<uint64_t> hashB;
    mt19937_64 rng;

    // Data
    vector<Document> documents;
    vector<vector<uint64_t>> signatures;

    // bucket key -> document indexes
    unordered_map<string, vector<int>> buckets;

    static constexpr uint64_t PRIME = 4294967311ULL; // > 2^32

private:
    // ---------- Text utilities ----------
    static string normalizeText(const string& text) {
        string out;
        out.reserve(text.size());

        for (char c : text) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (isalnum(uc) || isspace(uc)) {
                out.push_back(static_cast<char>(tolower(uc)));
            } else {
                out.push_back(' ');
            }
        }
        return out;
    }

    static vector<string> splitWords(const string& text) {
        vector<string> words;
        istringstream iss(text);
        string word;
        while (iss >> word) {
            words.push_back(word);
        }
        return words;
    }

    static unordered_set<string> createShingles(const string& text, int shingleSize) {
        unordered_set<string> shingles;
        string clean = normalizeText(text);
        vector<string> words = splitWords(clean);

        if (words.empty()) return shingles;

        if (static_cast<int>(words.size()) < shingleSize) {
            shingles.insert(words[0]);
            return shingles;
        }

        for (size_t i = 0; i + shingleSize <= words.size(); ++i) {
            string shingle = words[i];
            for (int j = 1; j < shingleSize; ++j) {
                shingle += "_" + words[i + j];
            }
            shingles.insert(shingle);
        }

        return shingles;
    }

    // ---------- MinHash ----------
    void initializeHashFunctions() {
        hashA.resize(numHashes);
        hashB.resize(numHashes);

        uniform_int_distribution<uint64_t> distA(1, PRIME - 1);
        uniform_int_distribution<uint64_t> distB(0, PRIME - 1);

        for (int i = 0; i < numHashes; ++i) {
            hashA[i] = distA(rng);
            hashB[i] = distB(rng);
        }
    }

    static uint64_t hashStringToUint64(const string& s) {
        return static_cast<uint64_t>(hash<string>{}(s));
    }

    vector<uint64_t> createMinHashSignature(const unordered_set<string>& shingles) const {
        vector<uint64_t> signature(numHashes, UINT64_MAX);
        if (shingles.empty()) return signature;

        for (const auto& token : shingles) {
            uint64_t x = hashStringToUint64(token) % PRIME;

            for (int i = 0; i < numHashes; ++i) {
                uint64_t value = ((hashA[i] * x) % PRIME + hashB[i]) % PRIME;
                if (value < signature[i]) {
                    signature[i] = value;
                }
            }
        }

        return signature;
    }

    // ---------- LSH ----------
    uint64_t hashBand(const vector<uint64_t>& signature, int bandIndex) const {
        int start = bandIndex * rowsPerBand;
        int end = start + rowsPerBand;

        uint64_t combined = 1469598103934665603ULL; // FNV-like seed
        for (int i = start; i < end; ++i) {
            combined ^= signature[i] + 0x9e3779b97f4a7c15ULL + (combined << 6) + (combined >> 2);
        }
        return combined;
    }

    static string makeBucketKey(int bandIndex, uint64_t bandHashValue) {
        return to_string(bandIndex) + "#" + to_string(bandHashValue);
    }

    // ---------- Similarity ----------
    static double computeJaccard(const unordered_set<string>& a,
                                 const unordered_set<string>& b) {
        if (a.empty() && b.empty()) return 1.0;

        size_t intersectionCount = 0;
        if (a.size() < b.size()) {
            for (const auto& item : a) {
                if (b.count(item)) ++intersectionCount;
            }
        } else {
            for (const auto& item : b) {
                if (a.count(item)) ++intersectionCount;
            }
        }

        size_t unionCount = a.size() + b.size() - intersectionCount;
        if (unionCount == 0) return 0.0;
        return static_cast<double>(intersectionCount) / static_cast<double>(unionCount);
    }
};
