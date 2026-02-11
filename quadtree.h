#pragma once

#include <vector>
#include <array>
#include <memory>
#include <cmath>
#include <algorithm> // sort, min
#include "movie.h"
#include "knn-heap.h"

using namespace std;

class QuadTree {
public:
    struct Point {
        array<double, 2> coords;
        Movie* movie;
        
        Point(array<double, 2> c, Movie* m) : coords(c), movie(m) {}
    };
    
    struct Boundary {
        double minX, maxX, minY, maxY;
        
        Boundary(double minX = 0, double maxX = 0, double minY = 0, double maxY = 0)
            : minX(minX), maxX(maxX), minY(minY), maxY(maxY) {}
        
        bool contains(const array<double, 2>& point) const {
            return point[0] >= minX && point[0] <= maxX &&
                   point[1] >= minY && point[1] <= maxY;
        }
        
        bool intersects(const Boundary& other) const {
            return !(other.minX > maxX || other.maxX < minX ||
                     other.minY > maxY || other.maxY < minY);
        }

        double minDistToBoundary(const array<double, 2>& target) const {
            double dx = 0.0;
            if (target[0] < minX) dx = minX - target[0];
            else if (target[0] > maxX) dx = target[0] - maxX;

            double dy = 0.0;
            if (target[1] < minY) dy = minY - target[1];
            else if (target[1] > maxY) dy = target[1] - maxY;

            return sqrt(dx * dx + dy * dy);
        }
    };

private:
    static const int CAPACITY = 4;  // Max points per node before subdividing

    Boundary boundary;
    vector<Point> points;
    bool divided;
    
    unique_ptr<QuadTree> northeast;
    unique_ptr<QuadTree> northwest;
    unique_ptr<QuadTree> southeast;
    unique_ptr<QuadTree> southwest;
    
    void subdivide() {
        double midX = (boundary.minX + boundary.maxX) / 2.0;
        double midY = (boundary.minY + boundary.maxY) / 2.0;
        
        northeast = make_unique<QuadTree>(
            Boundary(midX, boundary.maxX, midY, boundary.maxY));
        northwest = make_unique<QuadTree>(
            Boundary(boundary.minX, midX, midY, boundary.maxY));
        southeast = make_unique<QuadTree>(
            Boundary(midX, boundary.maxX, boundary.minY, midY));
        southwest = make_unique<QuadTree>(
            Boundary(boundary.minX, midX, boundary.minY, midY));
        
        divided = true;
    }

    double distance(const array<double, 2>& a, const array<double, 2>& b) const {
        double dx = a[0] - b[0];
        double dy = a[1] - b[1];
        return sqrt(dx * dx + dy * dy);
    }



    void kNNRec(const array<double, 2>& target, int k, Heap& heap) const {
        // 1) Prune this node if it cannot contain a better candidate
        if (heap.size() == k) {
            double bestWorstDistance = heap.getMax().dist;  // current kth nearest distance
            double MinDistance = boundary.minDistToBoundary(target);
            if (MinDistance > bestWorstDistance) return;
        }

        // 2) Check points in this node
        for (const auto& p : points) {
            double dist = distance(p.coords, target);

            if (heap.size() < k) {
                heap.insert(p.movie, dist);
            } else if (dist < heap.getMax().dist) {
                heap.extractMax();        // remove current farthest among k best
                heap.insert(p.movie, dist);  // add better one
            }
        }

        if (!divided) return;

        // 3) Visit children in order of increasing min possible distance
        vector<pair<double, const QuadTree*>> children;

        children.push_back({northeast->boundary.minDistToBoundary(target), northeast.get()});
        children.push_back({northwest->boundary.minDistToBoundary(target), northwest.get()});
        children.push_back({southeast->boundary.minDistToBoundary(target), southeast.get()});
        children.push_back({southwest->boundary.minDistToBoundary(target), southwest.get()});

        sort(children.begin(), children.end(),
             [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& child : children) {
            if (heap.size() < k || child.first <= heap.getMax().dist) {
                child.second->kNNRec(target, k, heap);
            }
        }
    }


public:
    QuadTree(Boundary b = Boundary()) 
        : boundary(b), divided(false) {}
    
    // Insert a point
    bool insert(array<double, 2> coords, Movie* movie) {
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
    vector<Movie*> rangeQuery(const Boundary& range) const {
        vector<Movie*> found;
        
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
            
            //insert
            found.insert(found.end(), ne.begin(), ne.end());
            found.insert(found.end(), nw.begin(), nw.end());
            found.insert(found.end(), se.begin(), se.end());
            found.insert(found.end(), sw.begin(), sw.end());
        }
        
        return found;
    }
    
    // Range query using lower and upper bounds
    vector<Movie*> rangeQuery(const array<double, 2>& lower, 
                              const array<double, 2>& upper) const {
        Boundary range(lower[0], upper[0], lower[1], upper[1]);
        return rangeQuery(range);
    }
    
    vector<Movie*> kNNSearch(int k, const array<double, 2>& target) const {
        vector<Movie*> results;
        if (k <= 0) return results;

        Heap heap;  // or KNNHeap heap(k); if your constructor requires k

        kNNRec(target, k, heap);

        while (heap.size() > 0) {
            results.push_back(heap.extractMax().movie);
        }
        
        reverse(results.begin(), results.end()); // nearest -> farthest
        return results;
    }
    
    // Point search - exact match
    Movie* search(const array<double, 2>& target) const {
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
};
