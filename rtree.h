// rtree.h
#pragma once

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>
#include "movie.h"
#include <queue>
#include <cmath>

using namespace std;

template <size_t K, size_t MAX_CHILDREN = 8>
class RTree {
public:
    struct Rect {
        array<double, K> minPoint;
        array<double, K> maxPoint;
    };

    struct Node {
        bool isLeaf;
        Rect bounds;
        Movie* movie; // only for data leaf nodes
        Node* parent;
        vector<unique_ptr<Node>> children;

        explicit Node(bool leaf)
            : isLeaf(leaf), movie(nullptr), parent(nullptr) {
            for (size_t d = 0; d < K; ++d) {
                bounds.minPoint[d] = 0;
                bounds.maxPoint[d] = 0;
            }
        }
    };

    RTree() = default;

private:
    unique_ptr<Node> root;

    // Squared minimum distance from point to rectangle
    static double minDistancePointToRect(const array<double, K>& point,
                                         const Rect& rect) {
        double sum = 0;
        for (size_t d = 0; d < K; ++d) {
            double v = 0;
            if (point[d] < rect.minPoint[d]) {
                v = rect.minPoint[d] - point[d];
            } else if (point[d] > rect.maxPoint[d]) {
                v = point[d] - rect.maxPoint[d];
            } else {
                v = 0;
            }
            sum += v * v;
        }
        return sum;
    }

    // ---------- Geometry ----------
    static Rect combineRects(const Rect& a, const Rect& b) {
        Rect out;
        for (size_t d = 0; d < K; ++d) {
            out.minPoint[d] = min(a.minPoint[d], b.minPoint[d]);
            out.maxPoint[d] = max(a.maxPoint[d], b.maxPoint[d]);
        }
        return out;
    }

    static double enlargement(const Rect& current, const Rect& addRect) {
        auto measure = [](const Rect& r) {
            double m = 1.0;
            for (size_t d = 0; d < K; ++d) {
                m *= max(0.0, r.maxPoint[d] - r.minPoint[d]);
            }
            return m;
        };

        Rect combined = combineRects(current, addRect);
        return measure(combined) - measure(current);
    }

    static double centerDistanceL1(const Rect& a, const Rect& b) {
        double sum = 0;
        for (size_t d = 0; d < K; ++d) {
            double ca = (a.minPoint[d] + a.maxPoint[d]) / static_cast<double>(2);
            double cb = (b.minPoint[d] + b.maxPoint[d]) / static_cast<double>(2);
            sum += (ca > cb) ? (ca - cb) : (cb - ca);
        }
        return sum;
    }

    static bool intersects(const Rect& a, const Rect& b) {
        for (size_t d = 0; d < K; ++d) {
            if (a.maxPoint[d] < b.minPoint[d] || b.maxPoint[d] < a.minPoint[d]) {
                return false;
            }
        }
        return true;
    }

    static bool equalsRect(const Rect& a, const Rect& b) {
        for (size_t d = 0; d < K; ++d) {
            if (a.minPoint[d] != b.minPoint[d] || a.maxPoint[d] != b.maxPoint[d]) {
                return false;
            }
        }
        return true;
    }

    // ---------- Tree helpers ----------
    bool isDataLeaf(const Node* node) const {
        // data leaf = leaf node with movie set and no children
        return node && node->isLeaf && node->movie != nullptr && node->children.empty();
    }

    bool isLeafContainer(const Node* node) const {
        // leaf container = leaf node that holds data leaf children
        if (!node || !node->isLeaf) return false;
        if (node->children.empty()) return true; // empty leaf container
        return isDataLeaf(node->children[0].get());
    }

    void updateBounds(Node* node) {
        if (!node) return;

        if (isDataLeaf(node)) {
            // bounds already set
            return;
        }

        if (node->children.empty()) {
            for (size_t d = 0; d < K; ++d) {
                node->bounds.minPoint[d] = 0;
                node->bounds.maxPoint[d] = 0;
            }
            return;
        }

        Rect b = node->children[0]->bounds;
        for (size_t i = 1; i < node->children.size(); ++i) {
            b = combineRects(b, node->children[i]->bounds);
        }
        node->bounds = b;
    }

    Node* chooseLeafContainer(Node* node, const Rect& rect) {
        // If current is leaf container, done
        if (isLeafContainer(node)) return node;

        // Otherwise choose best child by minimum enlargement
        size_t bestIndex = 0;
        double bestEnl = numeric_limits<double>::max();
        double bestTie = numeric_limits<double>::max();

        for (size_t i = 0; i < node->children.size(); ++i) {
            const Rect& childRect = node->children[i]->bounds;
            double enl = enlargement(childRect, rect);
            double tie = centerDistanceL1(childRect, rect);

            if (enl < bestEnl || (enl == bestEnl && tie < bestTie)) {
                bestEnl = enl;
                bestTie = tie;
                bestIndex = i;
            }
        }

        return chooseLeafContainer(node->children[bestIndex].get(), rect);
    }

    void adjustAfterInsert(Node* node) {
        Node* cur = node;
        while (cur) {
            updateBounds(cur);
            if (cur->children.size() > MAX_CHILDREN) {
                cur = splitNode(cur);
            } else {
                cur = cur->parent;
            }
        }
    }

    Node* splitNode(Node* node) {
        Node* parent = node->parent;

        // Move children out
        vector<unique_ptr<Node>> movedChildren = move(node->children);
        node->children.clear();

        // Pick two seed children (farthest centers)
        size_t seedA = 0, seedB = 1;
        double bestDist = -1;
        for (size_t i = 0; i < movedChildren.size(); ++i) {
            for (size_t j = i + 1; j < movedChildren.size(); ++j) {
                double dist = centerDistanceL1(movedChildren[i]->bounds, movedChildren[j]->bounds);
                if (dist > bestDist) {
                    bestDist = dist;
                    seedA = i;
                    seedB = j;
                }
            }
        }

        auto leftNode = make_unique<Node>(node->isLeaf);
        auto rightNode = make_unique<Node>(node->isLeaf);

        vector<bool> used(movedChildren.size(), false);
        used[seedA] = true;
        used[seedB] = true;

        // Assign seeds
        movedChildren[seedA]->parent = leftNode.get();
        leftNode->children.push_back(move(movedChildren[seedA]));
        movedChildren[seedB]->parent = rightNode.get();
        rightNode->children.push_back(move(movedChildren[seedB]));

        updateBounds(leftNode.get());
        updateBounds(rightNode.get());

        // Distribute remaining
        for (size_t i = 0; i < movedChildren.size(); ++i) {
            if (used[i]) continue;

            Rect lb = leftNode->bounds;
            Rect rb = rightNode->bounds;
            double enlL = enlargement(lb, movedChildren[i]->bounds);
            double enlR = enlargement(rb, movedChildren[i]->bounds);

            if (enlL < enlR) {
                movedChildren[i]->parent = leftNode.get();
                leftNode->children.push_back(move(movedChildren[i]));
                updateBounds(leftNode.get());
            } else if (enlR < enlL) {
                movedChildren[i]->parent = rightNode.get();
                rightNode->children.push_back(move(movedChildren[i]));
                updateBounds(rightNode.get());
            } else {
                // tie-break without volume: keep groups more balanced
                if (leftNode->children.size() <= rightNode->children.size()) {
                    movedChildren[i]->parent = leftNode.get();
                    leftNode->children.push_back(move(movedChildren[i]));
                    updateBounds(leftNode.get());
                } else {
                    movedChildren[i]->parent = rightNode.get();
                    rightNode->children.push_back(move(movedChildren[i]));
                    updateBounds(rightNode.get());
                }
            }
        }

        if (!parent) {
            // node was root -> create new root
            auto newRoot = make_unique<Node>(false);
            leftNode->parent = newRoot.get();
            rightNode->parent = newRoot.get();
            newRoot->children.push_back(move(leftNode));
            newRoot->children.push_back(move(rightNode));
            updateBounds(newRoot.get());
            root = move(newRoot);
            return root.get();
        }

        // replace 'node' inside parent with left+right
        size_t idx = 0;
        bool found = false;
        for (size_t i = 0; i < parent->children.size(); ++i) {
            if (parent->children[i].get() == node) {
                idx = i;
                found = true;
                break;
            }
        }
        if (!found) return parent;

        // remove old node
        parent->children.erase(parent->children.begin() + static_cast<long>(idx));

        leftNode->parent = parent;
        rightNode->parent = parent;
        parent->children.push_back(move(leftNode));
        parent->children.push_back(move(rightNode));

        updateBounds(parent);
        return parent;
    }

    void rangeQueryRecursive(const Node* node, const Rect& query, vector<Movie*>& results) const {
        if (!node) return;

        if (!intersects(node->bounds, query)) return;

        if (isDataLeaf(node)) {
            if (intersects(node->bounds, query) && node->movie) {
                results.push_back(node->movie);
            }
            return;
        }

        for (const auto& child : node->children) {
            rangeQueryRecursive(child.get(), query, results);
        }
    }

    Movie* searchRecursive(const Node* node, const Rect& target) const {
        if (!node) return nullptr;

        if (!intersects(node->bounds, target)) return nullptr;

        if (isDataLeaf(node)) {
            if (node->movie && equalsRect(node->bounds, target)) {
                return node->movie;
            }
            return nullptr;
        }

        for (const auto& child : node->children) {
            Movie* found = searchRecursive(child.get(), target);
            if (found) return found;
        }

        return nullptr;
    }

    void kNearestRecursive(const Node* node,
                           const std::array<double, K>& target,
                           size_t k,
                           Heap& bestMovies) const {
        if (node == nullptr) {
            return;
        }

        // Current pruning threshold (worst distance among current best k)
        double worstBest;
        if (bestMovies.size() < k) {
            worstBest = numeric_limits<double>::max();
        } else {
            worstBest = bestMovies.getMax().dist;
        }

        // Prune whole subtree if even best possible point is too far
        double nodeMinDist = minDistancePointToRect(target, node->bounds);
        if (nodeMinDist > worstBest) {
            return;
        }

        // If this is a data leaf, evaluate the movie
        if (isDataLeaf(node)) {
            double d = minDistancePointToRect(target, node->bounds);

            if (bestMovies.size() < k) {
                bestMovies.insert(node->movie, d);
            } else {
                double currentWorst = bestMovies.getMax().dist;
                if (d < currentWorst) {
                    bestMovies.extractMax();
                    bestMovies.insert(node->movie, d);
                }
            }
            return;
        }

        // Internal node: collect children that are still promising
        vector<pair<double, const Node*>> children;

        for (int i = 0; i < node->children.size(); ++i) {
            const Node* child = node->children[i].get();
            if (child == nullptr) {
                continue;
            }

            double childMinDist = minDistancePointToRect(target, child->bounds);

            if (bestMovies.size() < k) {
                children.push_back({childMinDist, child});
            } else {
                double currentWorst = bestMovies.getMax().dist;
                if (childMinDist <= currentWorst) {
                    children.push_back({childMinDist, child});
                }
            }
        }

        // Visit nearest children first
        sort(children.begin(), children.end(),
             [](const auto& a, const auto& b) { return a.first < b.first; });

        for (int i = 0; i < children.size(); ++i) {
            double currentWorst;
            if (bestMovies.size() < k) {
                currentWorst = numeric_limits<double>::max();
            } else {
                currentWorst = bestMovies.getMax().dist;
            }

            if (children[i].first > currentWorst) {
                break;
            }

            kNearestRecursive(children[i].second, target, k, bestMovies);
        }
    }

public:
    void insert(const Rect& rect, Movie* movie) {
        if (!movie) return;

        if (!root) {
            // root is a leaf container
            root = make_unique<Node>(true);
            auto dataNode = make_unique<Node>(true);
            dataNode->bounds = rect;
            dataNode->movie = movie;
            dataNode->parent = root.get();
            root->children.push_back(move(dataNode));
            updateBounds(root.get());
            return;
        }

        Node* leafContainer = chooseLeafContainer(root.get(), rect);

        auto dataNode = make_unique<Node>(true);
        dataNode->bounds = rect;
        dataNode->movie = movie;
        dataNode->parent = leafContainer;
        leafContainer->children.push_back(move(dataNode));

        adjustAfterInsert(leafContainer);
    }

    Movie* search(const Rect& rect) const {
        return searchRecursive(root.get(), rect);
    }

    vector<Movie*> rangeQuery(const Rect& query) const {
        vector<Movie*> results;
        rangeQueryRecursive(root.get(), query, results);
        return results;
    }

    vector<Movie*> kNearest(const array<double, K>& target, int k) const {
        vector<Movie*> result;

        if (root == nullptr || k <= 0) {
            return result;
        }

        Heap bestMovies;
        kNearestRecursive(root.get(), target, static_cast<size_t>(k), bestMovies);

        // bestMovies is max-heap (farthest first), so reverse to get nearest first
        while (bestMovies.size() > 0) {
            result.push_back(bestMovies.extractMax().movie);
        }

        reverse(result.begin(), result.end());

        return result;
    }
};
