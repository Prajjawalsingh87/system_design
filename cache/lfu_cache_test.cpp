#include "lfu_cache.hpp"

#include <map>
#include <string>

#include "../testing/mini_test.hpp"

TEST(get_and_put_basics) {
    LFUCache<std::string, int> cache(2);
    CHECK(!cache.get("a").has_value());
    cache.put("a", 1);
    CHECK_EQ(*cache.get("a"), 1);
    CHECK_EQ(cache.frequency("a"), 2u);  // put + get
    CHECK_EQ(cache.frequency("missing"), 0u);
}

TEST(evicts_least_frequently_used) {
    LFUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.get(1);
    cache.get(1);
    auto evicted = cache.put(3, 30);  // 2 has the lowest frequency
    REQUIRE(evicted.has_value());
    CHECK_EQ(*evicted, 2);
    CHECK(cache.contains(1));
    CHECK(cache.contains(3));
}

TEST(ties_are_broken_by_recency) {
    LFUCache<int, int> cache(3);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.put(3, 3);  // all at frequency 1; 1 is the least recent
    CHECK_EQ(*cache.put(4, 4), 1);
    cache.get(2);
    cache.get(3);
    cache.get(4);  // all at frequency 2; 2 is the least recent
    CHECK_EQ(*cache.put(5, 5), 2);
}

TEST(new_key_resets_min_frequency) {
    LFUCache<int, int> cache(2);
    cache.put(1, 1);
    for (int i = 0; i < 5; ++i) cache.get(1);
    cache.put(2, 2);
    for (int i = 0; i < 5; ++i) cache.get(2);
    cache.put(3, 3);  // evicts 1 (freq 6, older than 2 at freq 6)
    CHECK(!cache.contains(1));
    CHECK_EQ(*cache.put(4, 4), 3);  // the new key 3 (freq 1) is the next victim
}

TEST(update_counts_as_use) {
    LFUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.put(1, 100);  // freq(1) = 2
    CHECK_EQ(*cache.put(3, 3), 2);
    CHECK_EQ(*cache.get(1), 100);
}

TEST(erase_keeps_eviction_correct) {
    LFUCache<int, int> cache(3);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.get(2);
    cache.put(3, 3);
    cache.get(3);   // freq: 1->1, 2->2, 3->2
    CHECK(cache.erase(1));  // removes the only key at min frequency
    CHECK(!cache.erase(1));
    cache.put(4, 4);  // size back to 3, 4 at freq 1
    CHECK_EQ(*cache.put(5, 5), 4);
    CHECK(cache.erase(5));
    CHECK(cache.erase(2));
    CHECK(cache.erase(3));
    CHECK_EQ(cache.size(), 0u);
    cache.put(6, 6);
    CHECK_EQ(*cache.get(6), 6);
}

TEST(capacity_one_and_zero) {
    LFUCache<int, int> one(1);
    one.put(1, 1);
    one.get(1);
    CHECK_EQ(*one.put(2, 2), 1);
    using IntCache = LFUCache<int, int>;
    CHECK_THROWS(IntCache(0), std::invalid_argument);
}

TEST(matches_reference_model_on_long_sequence) {
    // O(n) reference: evict min (freq, last_used).
    const std::size_t capacity = 4;
    LFUCache<int, int> cache(capacity);
    struct Ref { int value; std::size_t freq; int last_used; };
    std::map<int, Ref> model;
    unsigned seed = 99;
    auto rnd = [&seed]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) % 10); };

    for (int t = 0; t < 5000; ++t) {
        const int key = rnd();
        auto it = model.find(key);
        if (t % 2 == 0) {
            auto got = cache.get(key);
            REQUIRE_EQ(got.has_value(), it != model.end());
            if (got) {
                REQUIRE_EQ(*got, it->second.value);
                ++it->second.freq;
                it->second.last_used = t;
            }
        } else {
            auto evicted = cache.put(key, t);
            std::optional<int> expected;
            if (it != model.end()) {
                it->second = {t, it->second.freq + 1, t};
            } else {
                if (model.size() == capacity) {
                    auto victim = model.begin();
                    for (auto m = model.begin(); m != model.end(); ++m) {
                        if (m->second.freq < victim->second.freq ||
                            (m->second.freq == victim->second.freq && m->second.last_used < victim->second.last_used))
                            victim = m;
                    }
                    expected = victim->first;
                    model.erase(victim);
                }
                model[key] = {t, 1, t};
            }
            REQUIRE_EQ(evicted.has_value(), expected.has_value());
            if (evicted) REQUIRE_EQ(*evicted, *expected);
        }
        if (t % 97 == 0 && !model.empty()) {  // occasionally erase too
            const int k = model.begin()->first;
            cache.erase(k);
            model.erase(k);
        }
        REQUIRE_EQ(cache.size(), model.size());
    }
}

MINI_TEST_MAIN()
