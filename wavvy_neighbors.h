#ifndef __WAVVY_NEIGHBORS__
#define __WAVVY_NEIGHBORS__

#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
using namespace std;

struct wavvy_neighbor;
class wavvy_neighbor_list;

typedef int node_id;
typedef wavvy_neighbor_list nvect;


struct wavvy_neighbor {
public:
    wavvy_neighbor() : id (-1), contacted(0) {}
    wavvy_neighbor(node_id _id) : id(_id), contacted(0) {}

    void set_id(node_id _id) { id = _id; }
    void set_contacted(bool c) { contacted = c; }

    node_id get_id() const { return id; }
    bool get_contacted() const { return contacted; }

    bool operator<(const wavvy_neighbor& in) const {
        return (this->id < in.id);
    }

    bool operator==(const wavvy_neighbor& in) const {
        return (this->id == in.id);
    }

private:
    node_id id;
    bool contacted;

};

class wavvy_neighbor_list {

public:

    wavvy_neighbor_list(): current_neighbor(0) {}

    inline  void insert_node(const node_id& u);
    inline  void insert_node(const wavvy_neighbor& w);

    inline void delete_node(const node_id& u);

    inline wavvy_neighbor& find_node_by_id(const node_id& u);     // find node by id.
    inline wavvy_neighbor& find_node_by_index(const unsigned int& ind);    // find node by index.
    inline wavvy_neighbor& operator[](const unsigned int& ind);            // find node by index.
    inline wavvy_neighbor& get_random_node();
    inline wavvy_neighbor& next();
    inline wavvy_neighbor& next_not_contacted();

    inline void sort() {
        std::sort(neighbors.begin(), neighbors.end());
        std::reverse(neighbors.begin(), neighbors.end());
    }
    inline unsigned int size() const { return neighbors.size(); }

    inline bool is_in(const node_id& u) {
        for (vector<wavvy_neighbor>::iterator it = neighbors.begin();
             it != neighbors.end(); it++) {
            if (it->get_id() == u) return true;
        }

        return false;
    }


    inline void mark_contacted_by_node_id(const node_id& u);
    inline void mark_contacted_all();
    inline bool is_contacted_by_index(const unsigned int& index);
    inline bool is_contacted_by_node_id(const node_id& u);
    inline bool is_all_contacted() {
        for (vector<wavvy_neighbor>::iterator it = neighbors.begin(); it != neighbors.end();
             it ++) {
            if (! it->get_contacted()) return false;
        }
        return true;
    }


private:
    inline bool is_in_range(const unsigned int& index) { return (index < neighbors.size()); }


private:
    vector<wavvy_neighbor> neighbors;
    unsigned int current_neighbor;
};

void wavvy_neighbor_list::insert_node(const node_id& u) {
    neighbors.push_back(wavvy_neighbor(u));
}

void wavvy_neighbor_list::insert_node(const wavvy_neighbor &w) {
    neighbors.push_back(w);
}

void wavvy_neighbor_list::delete_node(const node_id& u) {

    vector<wavvy_neighbor>::iterator it = find(neighbors.begin(), neighbors.end(), u);
    if (it != neighbors.end()) { neighbors.erase(it); }
    else {
        fprintf(stderr, "error in %s .. trying to delete an inexisting node %d \n",
                __FUNCTION__, u);
        abort();
    }
}

wavvy_neighbor& wavvy_neighbor_list::find_node_by_id(const node_id& u) {

    vector<wavvy_neighbor>::iterator it = find(neighbors.begin(), neighbors.end(), u);
    if (it == neighbors.end()) {
        fprintf(stderr, "error in %s .. node %d does not exist in current neighbor list\n",
                __FUNCTION__, u);
        abort();
    }

    return *it;

}

wavvy_neighbor& wavvy_neighbor_list::find_node_by_index(const unsigned int& ind) {
    if (this->is_in_range(ind)) { return neighbors[ind]; }

    fprintf(stderr, "error in %s .. out of range %d \n",
            __FUNCTION__, ind);
    abort();
}

wavvy_neighbor& wavvy_neighbor_list::operator[](const unsigned int& ind) {
    return find_node_by_index(ind);
}

wavvy_neighbor& wavvy_neighbor_list::get_random_node() {
    if (! neighbors.size()) {
        fprintf(stderr, "error in %s .. neighbors list is empty .. \n", __FUNCTION__);
        abort();
    }

    return neighbors[rand() % neighbors.size()];
}

wavvy_neighbor& wavvy_neighbor_list::next() {
    if (! neighbors.size()) {
        fprintf(stderr, "error in %s .. neighbors list is empty .. \n", __FUNCTION__);
        abort();
    }

    wavvy_neighbor& c = neighbors[current_neighbor];
    current_neighbor = (current_neighbor + 1) % neighbors.size();
    return c;
}

// TODO test me please ..
wavvy_neighbor& wavvy_neighbor_list::next_not_contacted() {
    wavvy_neighbor& c = this->next();
    while (c.get_contacted()) { c = this->next(); }
    return c;

}

void wavvy_neighbor_list::mark_contacted_by_node_id(const node_id& u) {
    this->find_node_by_id(u).set_contacted(true);
}

void wavvy_neighbor_list::mark_contacted_all() {
    for (vector<wavvy_neighbor>::iterator it = neighbors.begin(); it != neighbors.end();
         it ++ )  it->set_contacted(false);
}

bool wavvy_neighbor_list::is_contacted_by_index(const unsigned int& index) {
    return (find_node_by_index(index).get_contacted());
}

bool wavvy_neighbor_list::is_contacted_by_node_id(const node_id& u) {
    return (find_node_by_id(u).get_contacted());
}


//TODO: this should be more general - and in a different class.(use templates)
//TODO: fprintf(stdout, ... ) should be replaced wit a C++ function.
/*
void ScatFormWavvy::move_element(node_id u, nvect* from, nvect* to) {

    nvect_iter it = find_neighbor(from, u);
    // pair<node_id,wavvy_neighbor> c;
    wavvy_neighbor c;

    if (it != from->end()) {
        c = *it;
        from->erase(it);
    } else {
        fprintf(stderr, "error: element %d does not exist in from \n", u);
        abort();
    }

    it = find_neighbor(u);
    if (it != from->end()) {
        fprintf(stderr, "error: element %d already exists in to\n", u);
        abort();
    } else {  to->push_back(c); sort(to->begin(), to->end()); }
}
*/

inline void move_node(node_id u, nvect* from, nvect* to) {
    wavvy_neighbor& c = from->find_node_by_id(u);
    from->delete_node(u);

    if (to->is_in(u)) {
        fprintf(stderr, "error .. neighbor %d already exist in (to) .. function %s \n",
                                u, __FUNCTION__);
        abort();
    }  else {

        // TODO: we can also .. insert_in_place.. but no need for now .
        to->insert_node(c);
        to->sort();
    }

}

#endif
