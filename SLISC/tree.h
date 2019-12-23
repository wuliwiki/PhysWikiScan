// tree utility
#pragma once
#include "search.h"

namespace slisc {

struct Node
{
    vecLong last; // last nodes
    vecLong next; // next nodes
};

// links[2n] -> links[2n+1]
void tree_gen(vector<Node> &tree, vecLong_I links)
{
    Long Nlink = links.size();
    tree.resize(Nlink);
    for (Long i = 0; i < Nlink; i += 2) {
        Long ind1 = links[i], ind2 = links[i + 1];
        tree[ind1].next.push_back(ind2);
        tree[ind2].last.push_back(ind1);
    }
}

// iterative implementation of tree_all_dep()
// return -1-ind if too many levels (probably circular dependency), tree[ind] is the deepest level
Long tree_all_dep_imp(vecLong_O deps, const vector<Node> &tree, Long_I ind)
{
    static Long Niter = 0;
    // cout << "tree debug: ind = " << ind << ", level = " << Niter << endl;
    if (Niter > 300) {
        return -1-ind;
    }
    for (Long i = 0; i < size(tree[ind].last); ++i) {
        Long ind0 = tree[ind].last[i];
        deps.push_back(ind0);
        ++Niter;
        Long ret = tree_all_dep_imp(deps, tree, ind0);
        if (ret < 0)
            return ret;
        --Niter;
    }
    return 0;
}

// find all upstream nodes of a tree, and the distances
// return -1-ind if too many levels (probably circular dependency), tree[ind] is the deepest level
Long tree_all_dep(vecLong_O deps, const vector<Node> &tree, Long_I ind)
{
    deps.clear();
    Long ret = tree_all_dep_imp(deps, tree, ind);
    if (ret < 0)
        return ret;
    while (true) {
        Long ret = find_repeat(deps);
        if (ret < 0) {
            return size(deps);
        }
        deps.erase(deps.begin() + ret);
    }
}

// find redundant i.e. if A->B->...C, then A->C is redundent
// return -1-ind if too many levels (probably circular dependency), tree[ind] is the deepest level
Long tree_redundant(vecLong_O links, const vector<Node> &tree)
{
    vecLong deps;
    for (Long i = 0; i < Long(tree.size()); ++i) {
        deps.clear();
        Long ret = tree_all_dep_imp(deps, tree, i);
        if (ret < 0)
            return ret;
        for (Long j = 0; j < size(tree[i].last); ++j) {
            Long ind = search(tree[i].last[j], deps);
            if (ind >= 0) {
                deps.erase(deps.begin() + ind);
            }
        }
        for (Long j = 0; j < size(tree[i].last); ++j) {
            Long ind = search(tree[i].last[j], deps);
            if (ind >= 0) {
                links.push_back(deps[ind]);
                links.push_back(i);
            }
        }
    }
    return size(links) / 2;
}

} // namespace slisc
