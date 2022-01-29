

This is an atomic lock-free hash map implementation.

  * Entries from the map CANNOT be deleted.
  * A hash map size must be provided statically.
  * Two hash functions are assumed - one for hashing the key, and a second-order hash function for hashing hash values in case of hash collisions.
  * When a valid bucket cannot be found `get()` will return a null pointer. (By default 32 attempts to find a bucket are used; this should be enough.)
  * Hash collisions are not checked; you must check yourself for the unlikely event that two keys have the same hash.
  
Usage:

```
#include <lockfree-hash.hh>

lockfree::map<32, std::string, counter_t> my_map;

counter_t* c = my_map.get("hello", 
                          [](const std::string& key) { return hash(key); },
                          [](size_t prev_hash) { return hash(prev_hash); });
                          
// c will be nullptr in case of hash map overflow.

for (counter_t& c : my_map) {
  // 
}
```

Note: the constructor of your value type (e.g. `counter_t` in the example) *must* accept the key as the argument:

```
struct counter_t {
    std::atomic<int> value;
    counter_t(const std::string& key) : value(0) {}
};
```
