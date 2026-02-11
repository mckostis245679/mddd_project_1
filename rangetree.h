#pragma once
#include <array>
#include <vector>
#include "movie.h"

using namespace std;

class RangeTree2D {
private:
    // 1D tree on y (associated structure)
    class YTree {
    private:
        struct YNode {
            double y;
            vector<Movie*> movies;
            YNode* left;
            YNode* right;

            YNode(double yVal, Movie* m) : y(yVal), movies{m}, left(nullptr), right(nullptr) {}
        };

        YNode* root;

        static void appendMovies(const vector<Movie*>& src, vector<Movie*>& out) {
            for (Movie* m : src) out.push_back(m);
        }

        YNode* insertRecursive(YNode* node, double y, Movie* movie) {
            if (node == nullptr) return new YNode(y, movie);

            if (y < node->y) {
                node->left = insertRecursive(node->left, y, movie);
            } else if (y > node->y) {
                node->right = insertRecursive(node->right, y, movie);
            } else {
                node->movies.push_back(movie); // same y
            }
            return node;
        }

        static const YNode* findSplitNode(const YNode* node, double low, double high) {
            const YNode* v = node;
            while (v != nullptr && (high < v->y || low > v->y)) {
                if (high < v->y) v = v->left;
                else v = v->right;
            }
            return v;
        }

        static void reportAll(const YNode* node, vector<Movie*>& out) {
            if (node == nullptr) return;
            reportAll(node->left, out);
            appendMovies(node->movies, out);
            reportAll(node->right, out);
        }

    public:
        YTree() : root(nullptr) {}

        void insert(double y, Movie* movie) {
            root = insertRecursive(root, y, movie);
        }

        void rangeSearch(double low, double high, vector<Movie*>& out) const {
            const YNode* split = findSplitNode(root, low, high);
            if (split == nullptr) return;

            if (low <= split->y && split->y <= high) {
                appendMovies(split->movies, out);
            }

            // Left path
            const YNode* v = split->left;
            while (v != nullptr) {
                if (low <= v->y) {
                    reportAll(v->right, out); // fully inside y-range
                    if (low <= v->y && v->y <= high) appendMovies(v->movies, out);
                    v = v->left;
                } else {
                    v = v->right;
                }
            }

            // Right path
            v = split->right;
            while (v != nullptr) {
                if (v->y <= high) {
                    reportAll(v->left, out); // fully inside y-range
                    if (low <= v->y && v->y <= high) appendMovies(v->movies, out);
                    v = v->right;
                } else {
                    v = v->left;
                }
            }
        }
    };

    struct XNode {
        array<double, 2> point; // [x, y]
        Movie* movie;
        XNode* left;
        XNode* right;
        YTree ytree; // y-tree for ALL points in this x-subtree

        XNode(const array<double, 2>& p, Movie* m)
            : point(p), movie(m), left(nullptr), right(nullptr), ytree() {
            ytree.insert(p[1], m);
        }
    };

    XNode* root;

    static bool comparePoints(const array<double, 2>& a, const array<double, 2>& b) {
        // order by x, then y
        return (a[0] < b[0]) || (a[0] == b[0] && a[1] < b[1]);
    }

    static bool inRect(const array<double, 2>& p,
                       const array<double, 2>& low,
                       const array<double, 2>& high) {
        return (low[0] <= p[0] && p[0] <= high[0] &&
                low[1] <= p[1] && p[1] <= high[1]);
    }

    XNode* insertRecursive(XNode* node, const array<double, 2>& point, Movie* movie) {
        if (node == nullptr) return new XNode(point, movie);

        // keep associated y-structure updated with every point in subtree
        node->ytree.insert(point[1], movie);

        if (comparePoints(point, node->point))
            node->left = insertRecursive(node->left, point, movie);
        else
            node->right = insertRecursive(node->right, point, movie);

        return node;
    }

    static XNode* searchRecursive(XNode* node, const array<double, 2>& point) {
        if (node == nullptr) return nullptr;
        if (node->point == point) return node;

        if (comparePoints(point, node->point))
            return searchRecursive(node->left, point);
        else
            return searchRecursive(node->right, point);
    }

    static const XNode* findSplitNodeX(const XNode* node, double xLow, double xHigh) {
        const XNode* v = node;
        while (v != nullptr && (xHigh < v->point[0] || xLow > v->point[0])) {
            if (xHigh < v->point[0]) v = v->left;
            else v = v->right;
        }
        return v;
    }

public:
    RangeTree2D() : root(nullptr) {}

    void insert(const array<double, 2>& point, Movie* movie) {
        root = insertRecursive(root, point, movie);
    }

    Movie* search(const array<double, 2>& point) {
        XNode* node = searchRecursive(root, point);
        if (node == nullptr) return nullptr;
        return node->movie;
    }

    vector<Movie*> rangeSearch(const array<double, 2>& lower,
                               const array<double, 2>& upper) const {
        vector<Movie*> out;
        if (root == nullptr) return out;
        if (lower[0] > upper[0] || lower[1] > upper[1]) return out;

        const XNode* split = findSplitNodeX(root, lower[0], upper[0]);
        if (split == nullptr) return out;

        // Split node point
        if (inRect(split->point, lower, upper)) {
            out.push_back(split->movie);
        }

        // Left path from split: add right-subtree associated structures
        const XNode* v = split->left;
        while (v != nullptr) {
            if (lower[0] <= v->point[0]) {
                if (v->right != nullptr) {
                    v->right->ytree.rangeSearch(lower[1], upper[1], out);
                }
                if (inRect(v->point, lower, upper)) out.push_back(v->movie);
                v = v->left;
            } else {
                v = v->right;
            }
        }

        // Right path from split: add left-subtree associated structures
        v = split->right;
        while (v != nullptr) {
            if (v->point[0] <= upper[0]) {
                if (v->left != nullptr) {
                    v->left->ytree.rangeSearch(lower[1], upper[1], out);
                }
                if (inRect(v->point, lower, upper)) out.push_back(v->movie);
                v = v->right;
            } else {
                v = v->left;
            }
        }

        return out;
    }
};
