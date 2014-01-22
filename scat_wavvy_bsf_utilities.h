#ifndef __SCAT_WAVVY_UTIL__
#define __SCAT_WAVVY_UTIL__

// utilities:

inline const char* bool2str(bool b) {
    return (b ? "true" : "false");
}


// We use this for candidate_table ..
//TODO test me please ..
template <class M> M max_map_value(M first, M last) {
    if (first == last) return last;
    M largest = first;
    while (++first!=last) {
        if (largest->second < first->second) { largest = first; }
    }
    return largest;
}

#endif
