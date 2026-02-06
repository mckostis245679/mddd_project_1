// rtree.h
#pragma once

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>
#include "movie.h"

using namespace std;

template <size_t K, typename Number = double, size_t MAX_CHILDREN = 8>
class MovieRTree {
public:
    struct Rect {
        array<Number, K> minPoint;
        array<Number, K> maxPoint;
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

    MovieRTree() = default;

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

    vector<Movie*> rangeQuery(const Rect& query) const {
        vector<Movie*> results;
        rangeQueryRecursive(root.get(), query, results);
        return results;
    }

private:
    unique_ptr<Node> root;

private:
    // ---------- Geometry ----------
    static bool intersects(const Rect& a, const Rect& b) {
        for (size_t d = 0; d < K; ++d) {
            if (a.maxPoint[d] < b.minPoint[d] || b.maxPoint[d] < a.minPoint[d]) {
                return false;
            }
        }
        return true;
    }

    static Rect combineRects(const Rect& a, const Rect& b) {
        Rect out;
        for (size_t d = 0; d < K; ++d) {
            out.minPoint[d] = min(a.minPoint[d], b.minPoint[d]);
            out.maxPoint[d] = max(a.maxPoint[d], b.maxPoint[d]);
        }
        return out;
    }

    static Number volume(const Rect& r) {
        Number v = static_cast<Number>(1);
        for (size_t d = 0; d < K; ++d) {
            v *= max(static_cast<Number>(0), r.maxPoint[d] - r.minPoint[d]);
        }
        return v;
    }

    static Number enlargement(const Rect& current, const Rect& addRect) {
        Rect combined = combineRects(current, addRect);
        return volume(combined) - volume(current);
    }

    static Number centerDistanceL1(const Rect& a, const Rect& b) {
        Number sum = 0;
        for (size_t d = 0; d < K; ++d) {
            Number ca = (a.minPoint[d] + a.maxPoint[d]) / static_cast<Number>(2);
            Number cb = (b.minPoint[d] + b.maxPoint[d]) / static_cast<Number>(2);
            sum += (ca > cb) ? (ca - cb) : (cb - ca);
        }
        return sum;
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

    void updateBoundsUpward(Node* node) {
        Node* cur = node;
        while (cur) {
            updateBounds(cur);
            cur = cur->parent;
        }
    }

    Node* chooseLeafContainer(Node* node, const Rect& rect) {
        // If current is leaf container, done
        if (isLeafContainer(node)) return node;

        // Otherwise choose best child by minimum enlargement
        size_t bestIndex = 0;
        Number bestEnl = numeric_limits<Number>::max();
        Number bestVol = numeric_limits<Number>::max();

        for (size_t i = 0; i < node->children.size(); ++i) {
            const Rect& childRect = node->children[i]->bounds;
            Number enl = enlargement(childRect, rect);
            Number vol = volume(childRect);

            if (enl < bestEnl || (enl == bestEnl && vol < bestVol)) {
                bestEnl = enl;
                bestVol = vol;
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
        Number bestDist = -1;
        for (size_t i = 0; i < movedChildren.size(); ++i) {
            for (size_t j = i + 1; j < movedChildren.size(); ++j) {
                Number dist = centerDistanceL1(movedChildren[i]->bounds, movedChildren[j]->bounds);
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
            Number enlL = enlargement(lb, movedChildren[i]->bounds);
            Number enlR = enlargement(rb, movedChildren[i]->bounds);

            if (enlL < enlR) {
                movedChildren[i]->parent = leftNode.get();
                leftNode->children.push_back(move(movedChildren[i]));
                updateBounds(leftNode.get());
            } else if (enlR < enlL) {
                movedChildren[i]->parent = rightNode.get();
                rightNode->children.push_back(move(movedChildren[i]));
                updateBounds(rightNode.get());
            } else {
                if (volume(lb) <= volume(rb)) {
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
};
