#pragma once

/* 
 * This is an atomic lock-free hash map implementation.
 * Entries from the map CANNOT be deleted.
 * A hash map size must be provided statically.
 * Two hash functions are assumed - one for hashing the key, 
 * and a second-order hash function for hashing hash values in case of hash collisions.
 * When a valid bucket cannot be found get() will return a null pointer.
 * (By default 32 attempts to find a bucket are used.)
 * Hash collisions are not checked; you must check yourself for the unlikely event that two keys have the same hash.
 */

#include <atomic>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace lockfree {

namespace {

template <typename KEY, typename VALUE>
struct Element_ {
    size_t hash;
    VALUE val;

    Element_(const KEY& key, size_t hash_) : hash(hash_), val(key) {}
};

}

template <size_t SIZE, typename KEY, typename VALUE>
struct map {

    ~map() {
        for (size_t i = 0; i < SIZE; ++i) {
            Element* elt = hashmap[i].load(std::memory_order_relaxed);

            if (elt != nullptr) {
                delete elt;
            }
        }
    }

    VALUE* get(const KEY& key, auto&& hashfun1, auto&& hashfun2, size_t maxtries = 32) {

        size_t hash = hashfun1(key);
        size_t hash2 = hash;

        size_t tries = 0;
        size_t bucket;
        while (tries < maxtries) {
            bucket = hash2 % SIZE;
            Element* elt = hashmap[bucket].load(std::memory_order_relaxed);

            if (elt == nullptr) {
                Element* newelt = new Element(key, hash);

                if (hashmap[bucket].compare_exchange_weak(elt, newelt, std::memory_order_release, std::memory_order_relaxed)) {
                    return &newelt->val;

                } else if (elt->hash == hash) {
                    delete newelt;
                    return &elt->val;

                } else if (elt->hash != 0) {
                    delete newelt;
                    hash2 = hashfun2(hash2);
                }
                
            } else if (elt->hash == hash) {
                return &elt->val;

            } else if (elt->hash != 0) {
                hash2 = hashfun2(hash2);
            }
            ++tries;
        }

        return nullptr;
    }

    struct iterator {
        using container_type = map<SIZE, KEY, VALUE>;
        size_t bucket;
        typename container_type::Element* value;
        container_type& self;

        iterator(map<SIZE, KEY, VALUE>& s, size_t b) : bucket(b), value(nullptr), self(s) {
            increment();
        }

        VALUE& operator*() const {
            return value->val;
        }

        iterator& operator++() {
            if (bucket < SIZE) {
                ++bucket;
                increment();
            }
            return *this;
        }

        bool operator==(const iterator& a) const {
            return bucket == a.bucket;
        }

    private:
        void increment() {
            while (bucket < SIZE) {
                value = self.hashmap[bucket].load(std::memory_order_relaxed);
                if (value != nullptr) {
                    break;
                }
                ++bucket;
            }
        }
    };

    iterator begin() {
        return iterator(*this, 0);
    }

    iterator end() {
        return iterator(*this, SIZE);
    }

private:

    using Element = Element_<KEY, VALUE>;

    std::array<std::atomic<Element*>, SIZE> hashmap;
    
};

}
