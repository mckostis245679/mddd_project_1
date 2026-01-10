#pragma once
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "kdnode.h"  // Node<K> definition

template <size_t K>
class Heap {
private:
    struct Neighbor {
        Node<K>* node;  // pointer to KD-tree node
        double dist;    // distance to the query point
    };

    std::vector<Neighbor> array;  

    // Helper function to maintain max-heap property
    void heapify(int i) {
        int largest = i;
        int left = 2*i + 1;
        int right = 2*i + 2;
        int n = array.size();

        if (left < n && array[left].dist > array[largest].dist)
            largest = left;
        if (right < n && array[right].dist > array[largest].dist)
            largest = right;

        if (largest != i) {
            std::swap(array[i], array[largest]);
            heapify(largest);
        }
    }

public:
    Heap() = default;
    
    // Insert a neighbor into the heap
    void insert(Node<K>* node, double dist) {
        Neighbor n;
        n.node = node;
        n.dist = dist;

        array.push_back(n);
        int i = array.size() - 1;

        // Bubble up to maintain max-heap property
        while (i != 0 && array[(i-1)/2].dist < array[i].dist) {
            std::swap(array[i], array[(i-1)/2]);
            i = (i-1)/2;
        }
    }

    // Extract the farthest neighbor (max)
    Neighbor extractMax() {
        if (array.empty()) {
            Neighbor n;
            n.node = nullptr;
            n.dist = NULL;   
            return n;
        }

        Neighbor root = array[0];
        array[0] = array.back();
        array.pop_back();
        if (!array.empty()) heapify(0);
        return root;
    }

    // Get the farthest neighbor without removing
    Neighbor getMax()  {
        if (array.empty()) {
            Neighbor n;
            n.node = nullptr;
            n.dist = NULL;   
            return n;
        }
        return array[0];
    }

    // Get current number of neighbors in heap
    int size()  { return array.size(); }
};
