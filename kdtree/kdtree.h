// C++ Program to Implement KD Tree
#include <iostream>
#include <array>
#include <cmath>
#include "../movie.h"
#include "kdnode.h"
#include "knn-heap.h"

using namespace std;

// Template class for KDTree with K dimensions
template <size_t K>
class KDTree {
private:
    using Node = Node<K>;  
    
    Heap<K> heap;
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

        
    void kNNSearchRecursive(int k,Node* node, const array<double, K>& point, int depth)  {
        // Base case: If node is null, the point is not found
        if (node == nullptr) {
            return ;
        } 
        int cd = depth % K;
        double dist_node_point = dist(node->point, point);

        if (heap.size() < k) {
            heap.insert(node,dist_node_point);  // fill heap until we have k elements
        } else if (dist_node_point < heap.getMax().dist) {
            heap.extractMax();      // remove farthest
            heap.insert(node,dist_node_point);  // insert closer neighbor
        }

        // Compare point with current node and decide to go left or right
        if (point[cd] < node->point[cd]){
            kNNSearchRecursive(k,node->left, point, depth + 1);
            if (heap.size() < k || abs(point[cd] - node->point[cd]) < heap.getMax().dist)
                kNNSearchRecursive(k,node->right, point, depth + 1);
        }
        else{
            kNNSearchRecursive(k,node->right, point, depth + 1);
            if (heap.size() < k || abs(point[cd] - node->point[cd]) < heap.getMax().dist)
                kNNSearchRecursive(k,node->left, point, depth + 1);
        }
    }

        Node* nNSearchRecursive(Node* node, const array<double, K>& point, int depth) const {
        // Base case: If node is null, the point is not found
        if (node == nullptr) return nullptr;

        
        if (node->point == point) return node;

        int cd = depth % K;
        Node* best;
        Node* candidate= nullptr;
        // Compare point with current node and decide to go left or right
        if (point[cd] < node->point[cd]){
            best=nNSearchRecursive(node->left, point, depth + 1);
            if (best==nullptr || dist(point, node->point) < dist(point, best->point))
                best = node;
            if (dist(point,best->point) > abs(point[cd]-node->point[cd]))
                candidate=nNSearchRecursive(node->right, point, depth + 1);
 
        }
        else{
            best=nNSearchRecursive(node->right, point, depth + 1);
            if (best==nullptr || dist(point, node->point) < dist(point, best->point))
                best = node;
            if (dist(point,best->point) > abs(point[cd]-node->point[cd])){
                candidate=nNSearchRecursive(node->left, point, depth + 1);
            }
        }
        
        if (candidate != nullptr && dist(point, candidate->point) < dist(point,best->point)) {
            best = candidate;
        }
        return best;
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

    static double dist(const array<double, K>& point_a,const array<double, K>& point_b)
    {
        double sum = 0;
        for (int i = 0; i < K; i++) {
            double diff = point_a[i] - point_b[i];
            sum += diff * diff;
        }
        return sqrt(sum);
    }

public:
    KDTree() : root(nullptr) {}

    void insert(const array<double, K>& point,Movie * movie) {
        root = insertRecursive(root, point, 0,movie);
    }
    Node* search(const array<double, K>& point)  {
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

    Node* nNSearch(const array<double, K>& point){
        return nNSearchRecursive(root, point, 0);
    }

    vector<Movie*> kNNSearch(int k,const array<double, K>& point){
        kNNSearchRecursive(k,root, point, 0);
        vector<Movie*> results;
        for (int i=0;i<k;i++){
            Node* node=heap.extractMax().node;
            if (node==nullptr) break;
            results.push_back(node->movie);
        }

        return results;
    }
};

