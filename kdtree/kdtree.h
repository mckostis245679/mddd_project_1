#include <iostream>
#include <array>
#include <cmath>
using namespace std;

template <size_t K>
class KDTree {
private:
    // Node structure representing each point in the KDTree
    struct Node {
        // Point in K dimensions
        array<double, K> point; 
        // Pointer to left child
        Node* left;          
        // Pointer to right child
        Node* right;            

        // Constructor to initialize a Node
        Node(const array<double, K>& pt) : point(pt), left(nullptr), right(nullptr) {}
    };

    Node* root; // Root of the KDTree
};