
#include "lockfree-map.hh"

#include <thread>
#include <mutex>
#include <random>
#include <string>
#include <iostream>
#include <map>

uint64_t hash(const char* c, size_t n, uint64_t init = 0xcbf29ce484222325, uint64_t mul = 0x100000001b3) {

    uint64_t hash = init;

    while (n > 0) {
        hash ^= (uint64_t)(*c);
        hash *= (uint64_t)mul;
        ++c;
        --n;
    }

    return hash;
}

uint64_t hash_str(const std::string& s) {
    return hash(s.data(), s.size());
}

uint64_t hash_size_t(size_t v) {
    return hash((const char*)&v, sizeof(v));
}

struct counter_t {
    std::string key;
    std::atomic<int> counter;

    counter_t() {}
    counter_t(const std::string& key_) : key(key_), counter(0) {}
};

struct Test {
    lockfree::map<32, std::string, counter_t> lf_map;
    std::map<std::string, int> std_map;
    std::mutex mutex;

    void inc(const std::string& key, int howmuch = 1) {
        counter_t* v = lf_map.get(key, hash_str, hash_size_t);
        if (v == nullptr) {
            throw std::runtime_error("Could not find bucket.");
        }
        if (v->key != key) {
            throw std::runtime_error("Hash collision for key.");
        }
        v->counter += howmuch;
        std::lock_guard lock{mutex};
        std_map[key] += howmuch;
    }
};

std::mt19937_64& get_rand_generator(size_t seed = 0) {
    static thread_local std::mt19937_64 ret(seed);
    return ret;
}

int random(size_t seed, int low, int hi) {
    std::uniform_int_distribution<int> d(low, hi);
    return d(get_rand_generator(seed));
}

void go_thread(Test& test) {
    size_t seed = ::gettid();

    for (size_t i = 0; i < 100000; ++i) {
        test.inc(std::to_string(random(seed, 1, 15)));
    }
}

void go(Test& test) {
    std::vector<std::thread> threads;

    for (size_t i = 0; i < 100; ++i) {
        threads.emplace_back([&]() {
            try {
                go_thread(test);
            } catch (std::exception& e) {
                std::cout << "ERROR: " << e.what() << std::endl;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

void print_map(const auto& map) {
    std::cout << "===" << std::endl;
    for (const auto& [ key, val ] : map) {
        std::cout << " " << key << " : " << val << std::endl;
    }
    std::cout << "---" << std::endl;
}

void check(Test& test) {
    std::map<std::string, int> lf_map;
    std::map<std::string, int> std_map;

    int lf_total = 0;
    for (counter_t& c : test.lf_map) {
        lf_map[c.key] = c.counter.load();
        lf_total += lf_map[c.key];
    }

    int std_total = 0;
    for (const auto& [ key, val ] : test.std_map) {
        std_map[key] = val;
        std_total += std_map[key];
    }

    print_map(std_map);
    print_map(lf_map);

    std::cout << "STD total: " << std_total << " LF total: " << lf_total << std::endl;

    bool passed = (std_map == lf_map);

    if (passed) {
        std::cout << "PASSED" << std::endl;
    } else {
        std::cout << "FAILED" << std::endl;
    }
}

int main(int argc, char** argv) {

    try {
        Test test;
        go(test);
        check(test);
        
    } catch (std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
    }

    return 0;
}
