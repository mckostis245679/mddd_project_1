#pragma once
#include <array>
using namespace std;

template <size_t K>
struct Node {
    array<double, K> point;
    Node* left;
    Node* right;
    Movie* movie;  
    
    Node(const array<double, K>& pt, Movie* movie)
        : point(pt), left(nullptr), right(nullptr), movie(movie) {}
};
