// C++ Program to Implement KD Tree
#include <iostream>
#include <array>
#include <cmath>
#include "../movie.h"
using namespace std;

// Template class for KDTree with K dimensions
template <size_t K>
class KDTree {
private:
    // Node structure representing each point in the KDTree
    struct Node {
        array<double, K> point;
        Node* left;
        Node* right;
        Movie* movie;

        Node(const array<double, K>& pt, Movie* m) : point(pt), left(nullptr), right(nullptr), movie(m) {}
    };


    Node* root;

    Node* insertRecursive(Node* node, const array<double, K>& point, int depth, Movie* movie) {
        if (node == nullptr) return new Node(point, movie);

        int cd = depth % K;
        if (point[cd] < node->point[cd])
            node->left = insertRecursive(node->left, point, depth + 1, movie);
        else
            node->right = insertRecursive(node->right, point, depth + 1, movie);

        return node;
    }

    // Recursive function to search for a point in the KDTree
    Node* searchRecursive(Node* node, const array<double, K>& point, int depth) const {
        // Base case: If node is null, the point is not found
        if (node == nullptr) return nullptr;

        // If the current node matches the point, return true
        if (node->point == point) return node;

        // Calculate current dimension (cd)
        int cd = depth % K;

        // Compare point with current node and decide to go left or right
        if (point[cd] < node->point[cd])
            return searchRecursive(node->left, point, depth + 1);
        else
            return searchRecursive(node->right, point, depth + 1);
    }


    void rangeSearchRecursive(vector<Movie*>&  movies,Node* node, const array<double, K>& lower,const array<double, K>& upper, int depth) const {

        if (node == nullptr) return ;
       bool include=true;
        for (int i=0;i<K;i++){
            if (node->point[i] < lower[i] || node->point[i] > upper[i]){
                include=false;
                break;
            }
        }
        if (include) movies.push_back(node->movie);
        
        int cd = depth % K;

        if (lower[cd] <= node->point[cd]) {
            rangeSearchRecursive(movies, node->left, lower, upper, depth + 1);
        }

        if (upper[cd] >= node->point[cd]) {
            rangeSearchRecursive(movies, node->right, lower, upper, depth + 1);
        }
    }

    // Recursive function to print the KDTree
    void printRecursive(Node* node, int depth) const {
        // Base case: If node is null, return
        if (node == nullptr) return;

        // Print current node with indentation based on depth
        for (int i = 0; i < depth; i++) cout << "  ";
        cout << "(";
        for (size_t i = 0; i < K; i++) {
            cout << node->point[i];
            if (i < K - 1) cout << ", ";
        }
        cout << ")" << endl;

        // Recursively print left and right children
        printRecursive(node->left, depth + 1);
        printRecursive(node->right, depth + 1);
    }

public:
    KDTree() : root(nullptr) {}

    void insert(const array<double, K>& point,Movie * movie) {
        root = insertRecursive(root, point, 0,movie);
    }
    Node* search(const array<double, K>& point) const {
        return searchRecursive(root, point, 0);
    }

    void print() {
        printRecursive(root, 0);
    }

    vector<Movie*> rangeSearch(const array<double, K>& lower,const array<double, K>& upper)  {
        vector<Movie*> results;
        rangeSearchRecursive(results, root, lower, upper, 0);
        return results;
    }
};

