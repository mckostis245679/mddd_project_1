#ifndef QUADTREE_H
#define QUADTREE_H

#include <vector>
#include <array>
#include <memory>
#include <limits>
#include <cmath>
#include "movie.h"

template<typename T = double>
class QuadTree {
public:
    struct Point {
        std::array<T, 2> coords;
        Movie* movie;
        
        Point(std::array<T, 2> c, Movie* m) : coords(c), movie(m) {}
    };
    
    struct Boundary {
        T minX, maxX, minY, maxY;
        
        Boundary(T minX = 0, T maxX = 0, T minY = 0, T maxY = 0)
            : minX(minX), maxX(maxX), minY(minY), maxY(maxY) {}
        
        bool contains(const std::array<T, 2>& point) const {
            return point[0] >= minX && point[0] <= maxX &&
                   point[1] >= minY && point[1] <= maxY;
        }
        
        bool intersects(const Boundary& other) const {
            return !(other.minX > maxX || other.maxX < minX ||
                    other.minY > maxY || other.maxY < minY);
        }
    };

private:
    static const int CAPACITY = 4;  // Max points per node before subdividing
    
    Boundary boundary;
    std::vector<Point> points;
    bool divided;
    
    std::unique_ptr<QuadTree> northeast;
    std::unique_ptr<QuadTree> northwest;
    std::unique_ptr<QuadTree> southeast;
    std::unique_ptr<QuadTree> southwest;
    
    void subdivide() {
        T midX = (boundary.minX + boundary.maxX) / 2;
        T midY = (boundary.minY + boundary.maxY) / 2;
        
        northeast = std::make_unique<QuadTree>(
            Boundary(midX, boundary.maxX, midY, boundary.maxY));
        northwest = std::make_unique<QuadTree>(
            Boundary(boundary.minX, midX, midY, boundary.maxY));
        southeast = std::make_unique<QuadTree>(
            Boundary(midX, boundary.maxX, boundary.minY, midY));
        southwest = std::make_unique<QuadTree>(
            Boundary(boundary.minX, midX, boundary.minY, midY));
        
        divided = true;
    }

public:
    QuadTree(Boundary b = Boundary()) 
        : boundary(b), divided(false) {}
    
    // Insert a point
    bool insert(std::array<T, 2> coords, Movie* movie) {
        if (!boundary.contains(coords)) {
            return false;
        }
        
        if (points.size() < CAPACITY) {
            points.emplace_back(coords, movie);
            return true;
        }
        
        if (!divided) {
            subdivide();
        }
        
        return northeast->insert(coords, movie) ||
               northwest->insert(coords, movie) ||
               southeast->insert(coords, movie) ||
               southwest->insert(coords, movie);
    }
    
    // Range query - find all points within a boundary
    std::vector<Movie*> rangeQuery(const Boundary& range) const {
        std::vector<Movie*> found;
        
        if (!boundary.intersects(range)) {
            return found;
        }
        
        for (const auto& p : points) {
            if (range.contains(p.coords)) {
                found.push_back(p.movie);
            }
        }
        
        if (divided) {
            auto ne = northeast->rangeQuery(range);
            auto nw = northwest->rangeQuery(range);
            auto se = southeast->rangeQuery(range);
            auto sw = southwest->rangeQuery(range);
            
            found.insert(found.end(), ne.begin(), ne.end());
            found.insert(found.end(), nw.begin(), nw.end());
            found.insert(found.end(), se.begin(), se.end());
            found.insert(found.end(), sw.begin(), sw.end());
        }
        
        return found;
    }
    
    // Range query using lower and upper bounds (like your other structures)
    std::vector<Movie*> rangeQuery(const std::array<T, 2>& lower, 
                                   const std::array<T, 2>& upper) const {
        Boundary range(lower[0], upper[0], lower[1], upper[1]);
        return rangeQuery(range);
    }
    
    // Find nearest neighbor
    Movie* nearestNeighbor(const std::array<T, 2>& target) const {
        Movie* best = nullptr;
        T bestDist = std::numeric_limits<T>::max();
        nearestNeighborHelper(target, best, bestDist);
        return best;
    }
    
    // K-nearest neighbors
    std::vector<Movie*> kNNSearch(int k, const std::array<T, 2>& target) const {
        std::vector<std::pair<T, Movie*>> candidates;
        kNNHelper(target, candidates);
        
        // Sort by distance
        std::sort(candidates.begin(), candidates.end(),
                 [](const auto& a, const auto& b) { return a.first < b.first; });
        
        std::vector<Movie*> result;
        for (int i = 0; i < std::min(k, (int)candidates.size()); i++) {
            result.push_back(candidates[i].second);
        }
        return result;
    }
    
    // Point search - exact match
    Movie* search(const std::array<T, 2>& target) const {
        if (!boundary.contains(target)) {
            return nullptr;
        }
        
        for (const auto& p : points) {
            if (p.coords[0] == target[0] && p.coords[1] == target[1]) {
                return p.movie;
            }
        }
        
        if (divided) {
            Movie* found = northeast->search(target);
            if (found) return found;
            found = northwest->search(target);
            if (found) return found;
            found = southeast->search(target);
            if (found) return found;
            found = southwest->search(target);
            if (found) return found;
        }
        
        return nullptr;
    }
    
    // Get total number of points
    int size() const {
        int count = points.size();
        if (divided) {
            count += northeast->size() + northwest->size() +
                    southeast->size() + southwest->size();
        }
        return count;
    }

private:
    T distance(const std::array<T, 2>& a, const std::array<T, 2>& b) const {
        T dx = a[0] - b[0];
        T dy = a[1] - b[1];
        return std::sqrt(dx * dx + dy * dy);
    }
    
    void nearestNeighborHelper(const std::array<T, 2>& target, 
                               Movie*& best, T& bestDist) const {
        for (const auto& p : points) {
            T dist = distance(target, p.coords);
            if (dist < bestDist) {
                bestDist = dist;
                best = p.movie;
            }
        }
        
        if (divided) {
            northeast->nearestNeighborHelper(target, best, bestDist);
            northwest->nearestNeighborHelper(target, best, bestDist);
            southeast->nearestNeighborHelper(target, best, bestDist);
            southwest->nearestNeighborHelper(target, best, bestDist);
        }
    }
    
    void kNNHelper(const std::array<T, 2>& target,
                   std::vector<std::pair<T, Movie*>>& candidates) const {
        for (const auto& p : points) {
            T dist = distance(target, p.coords);
            candidates.emplace_back(dist, p.movie);
        }
        
        if (divided) {
            northeast->kNNHelper(target, candidates);
            northwest->kNNHelper(target, candidates);
            southeast->kNNHelper(target, candidates);
            southwest->kNNHelper(target, candidates);
        }
    }
};

#endif // QUADTREE_H
