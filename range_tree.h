// range_tree.h
#pragma once

#include <array>
#include <memory>
#include <vector>
#include "movie.h"
#include <queue>
#include <cmath>

using namespace std;

template <size_t K, typename Number = double>
class MovieRangeTree {
public:
    struct Node {
        array<Number, K> point;
        Movie* movie;
        size_t splitDimension;
        unique_ptr<Node> left;
        unique_ptr<Node> right;

        Node(const array<Number, K>& p, Movie* m, size_t splitDim)
            : point(p), movie(m), splitDimension(splitDim) {}
    };

    // Insert one movie point (similar style to KDTree insert)
    void insert(const array<Number, K>& point, Movie* movie) {
        root = insertRecursive(std::move(root), point, movie, 0);
    }

    vector<Movie*> rangeQuery(const array<Number, K>& low,
                              const array<Number, K>& high) const {
        vector<Movie*> results;
        rangeQueryRecursive(root.get(), low, high, results);
        return results;
    }
    
    vector<Movie*> kNearest(const array<Number, K>& queryPoint, int k) const {
        vector<Movie*> result;
        if (!root || k <= 0) return result;

        // max-heap by distance (largest on top), so we keep best k
        priority_queue<pair<Number, Movie*>> best;

        kNearestRecursive(root.get(), queryPoint, k, best);

        // extract in ascending distance
        vector<pair<Number, Movie*>> temp;
        while (!best.empty()) {
            temp.push_back(best.top());
            best.pop();
        }
        reverse(temp.begin(), temp.end());

        for (auto& item : temp) {
            if (item.second) result.push_back(item.second);
        }
        return result;
    }


private:

    static Number squaredDistancePoint(const array<Number, K>& a,
                                    const array<Number, K>& b) {
        Number sum = 0;
        for (size_t d = 0; d < K; ++d) {
            Number diff = a[d] - b[d];
            sum += diff * diff;
        }
        return sum;
    }

    void kNearestRecursive(const Node* node,
                        const array<Number, K>& queryPoint,
                        int k,
                        priority_queue<pair<Number, Movie*>>& best) const {
        if (!node) return;

        Number dist = squaredDistancePoint(node->point, queryPoint);

        if ((int)best.size() < k) {
            best.push({dist, node->movie});
        } else if (dist < best.top().first) {
            best.pop();
            best.push({dist, node->movie});
        }

        size_t dim = node->splitDimension;
        Number delta = queryPoint[dim] - node->point[dim];

        const Node* nearChild = (delta < 0) ? node->left.get() : node->right.get();
        const Node* farChild  = (delta < 0) ? node->right.get() : node->left.get();

        // Visit near side first
        kNearestRecursive(nearChild, queryPoint, k, best);

        // Decide if far side can still contain better points
        Number axisDist2 = delta * delta;
        Number worstBest = ((int)best.size() < k) ? numeric_limits<Number>::max() : best.top().first;

        if (axisDist2 <= worstBest) {
            kNearestRecursive(farChild, queryPoint, k, best);
        }
    }

    unique_ptr<Node> root;

    static bool pointInsideRange(const array<Number, K>& point,
                                 const array<Number, K>& low,
                                 const array<Number, K>& high) {
        for (size_t d = 0; d < K; ++d) {
            if (point[d] < low[d] || point[d] > high[d]) return false;
        }
        return true;
    }

    unique_ptr<Node> insertRecursive(unique_ptr<Node> node,
                                     const array<Number, K>& point,
                                     Movie* movie,
                                     size_t depth) {
        if (!node) {
            size_t splitDim = depth % K;
            return make_unique<Node>(point, movie, splitDim);
        }

        size_t dim = node->splitDimension;
        if (point[dim] < node->point[dim]) {
            node->left = insertRecursive(std::move(node->left), point, movie, depth + 1);
        } else {
            node->right = insertRecursive(std::move(node->right), point, movie, depth + 1);
        }
        return node;
    }

    void rangeQueryRecursive(const Node* node,
                             const array<Number, K>& low,
                             const array<Number, K>& high,
                             vector<Movie*>& results) const {
        if (!node) return;

        if (pointInsideRange(node->point, low, high)) {
            results.push_back(node->movie);
        }

        size_t dim = node->splitDimension;

        if (low[dim] <= node->point[dim]) {
            rangeQueryRecursive(node->left.get(), low, high, results);
        }
        if (node->point[dim] <= high[dim]) {
            rangeQueryRecursive(node->right.get(), low, high, results);
        }
    }
};
