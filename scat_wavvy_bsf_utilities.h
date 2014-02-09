#ifndef __SCAT_WAVVY_UTIL__
#define __SCAT_WAVVY_UTIL__

// utilities:

inline const char* bool2str(bool b) {
    return (b ? "true" : "false");
}


// We use this for candidate_table ..
//TODO test me please ..
template <class M> M max_map_value(M f, M l) {
    if (f == l) return l;
    M largest = f;
    while (++f!=l) {
        if (largest->second < f->second) { largest = f; }
    }
    return largest;
}


#define TYPE_DEF(org_type, type_name)                                       \
class type_name {                                                               \
public:                                                                         \
    type_name() { value = 0; }                                                  \
    type_name(const org_type& a) { value = a; }                                 \
    type_name& operator=(const type_name& a) { value = a.value; return (*this); }   \
    org_type operator*() const { return value; }                                          \
    bool operator==(const type_name& a) const { return (this->value == a.value);}              \
    bool operator<(const type_name& a) const { return (this->value < a.value); }          \
                                                                                    \
private:                                                                            \
    org_type value;                                                                 \
    template <typename NA> void operator=(const NA& a);                             \
    template <typename NA> type_name(const NA& a);                                   \
}


#endif
