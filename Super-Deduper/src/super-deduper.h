#ifndef SUPERD_H
#define SUPERD_H
//  this is so we can implment hash function for dynamic_bitset
#define BOOST_DYNAMIC_BITSET_DONT_USE_FRIENDS

#include "utils.h"
#include "ioHandler.h"
#include <map>
#include <unordered_map>
#include <boost/dynamic_bitset.hpp>
#include <boost/functional/hash.hpp>

class dbhash {
public:
    std::size_t operator() (const boost::dynamic_bitset<>& bs) const {
        return boost::hash_value(bs.m_bits);
    }
};

typedef std::unordered_map <boost::dynamic_bitset<>, std::unique_ptr<ReadBase>, dbhash> BitMap;

template <class T, class Impl>
void load_map(InputReader<T, Impl> &reader, SuperDeduperCounters& counters, BitMap& read_map, std::shared_ptr<OutputWriter> pe, std::shared_ptr<OutputWriter> se, double avg_automatic_write, double discard_qual, size_t start, size_t length) {
    double tmpAvg;

    while(reader.has_next()) {
        auto i = reader.next();
        counters.input(*i);
        //check for existance, store or compare quality and replace:
        if (auto key=i->get_key(start, length)) {
            // find faster than count on some compilers, new key
            tmpAvg = i->avg_q_score();
            if ( tmpAvg < discard_qual ){ // averge qual must be less than discard_qual, ignored
                counters.increment_ignored();
            } else if(read_map.find(*key) == read_map.end()) { // first time the key is seen
                if ( tmpAvg >= avg_automatic_write ) { // if its greater than avg_automatic_write then write out
                    writer_helper(i.get(), pe, se, false);
                    counters.output(*i);
                    read_map[*key] = nullptr;
                } else {
                    read_map[*key] = std::move(i);
                }
            } else if (read_map[*key] == nullptr) { //key already seen and written out, PCR dup
                counters.increment_replace();
            } else if( tmpAvg > read_map[*key]->avg_q_score()){ // new read is 'better' than old, key not yet read out
                if (tmpAvg >= avg_automatic_write) { // read qualifies, write out
                    writer_helper(i.get(), pe, se, false);
                    counters.output(*i);
                    read_map[*key] = nullptr;
                } else if (tmpAvg > discard_qual) {
                    read_map[*key] = std::move(i);
                }
                counters.increment_replace();
            } else {
                counters.increment_replace(); // new read has been seen but is not better then last seen read with same key
            }
        } else {  // key had N, no key obtained
            counters.increment_ignored();
        }
    }
}

#endif
