#include "write_through_cache.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../testing/mini_test.hpp"

namespace {

// Deterministic Zipf(s = 1) sampler over keys 0..n-1: a few keys are very hot,
// most are cold, which is how real read traffic usually looks.
class Zipf {
public:
    Zipf(int n, unsigned seed) : seed_(seed) {
        double sum = 0;
        for (int i = 1; i <= n; ++i) cdf_.push_back(sum += 1.0 / i);
        for (double& c : cdf_) c /= sum;
    }
    int next() {
        seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
        const double u = static_cast<double>(seed_ >> 11) / static_cast<double>(1ull << 53);
        return static_cast<int>(std::lower_bound(cdf_.begin(), cdf_.end(), u) - cdf_.begin());
    }

private:
    std::vector<double> cdf_;
    unsigned long long seed_;
};

using Store = FakeSlowStore<int, std::string>;
using Cache = WriteThroughCache<int, std::string>;

void fill(Store& store, int n) {
    for (int i = 0; i < n; ++i) store.write(i, "value-" + std::to_string(i));
}

}  // namespace

TEST(miss_loads_from_store_then_hits) {
    Store store;
    fill(store, 10);
    Cache cache(store, 5);
    const auto reads_before = store.reads();
    CHECK_EQ(*cache.get(3), std::string("value-3"));
    CHECK_EQ(*cache.get(3), std::string("value-3"));
    CHECK_EQ(store.reads() - reads_before, 1u);  // second read served from memory
    CHECK_EQ(cache.stats().hits, 1u);
    CHECK_EQ(cache.stats().misses, 1u);
}

TEST(missing_key_is_not_cached) {
    Store store;
    Cache cache(store, 5);
    CHECK(!cache.get(42).has_value());
    CHECK(!cache.is_cached(42));
}

TEST(write_goes_to_store_and_cache) {
    Store store;
    Cache cache(store, 5);
    cache.put(1, "one");
    CHECK_EQ(*store.peek(1), std::string("one"));  // persisted immediately
    CHECK(cache.is_cached(1));
    const auto reads = store.reads();
    CHECK_EQ(*cache.get(1), std::string("one"));
    CHECK_EQ(store.reads(), reads);  // no store read needed
}

TEST(update_never_serves_stale_value) {
    Store store;
    Cache cache(store, 5);
    cache.put(1, "v1");
    cache.get(1);
    cache.put(1, "v2");
    CHECK_EQ(*cache.get(1), std::string("v2"));
    CHECK_EQ(*store.peek(1), std::string("v2"));
}

TEST(failed_store_write_leaves_cache_unchanged) {
    Store store;
    Cache cache(store, 5);
    cache.put(1, "v1");
    store.fail_next_write();
    CHECK_THROWS(cache.put(1, "v2"), std::runtime_error);
    CHECK_EQ(*cache.get(1), std::string("v1"));
    CHECK_EQ(*store.peek(1), std::string("v1"));
}

TEST(remove_deletes_from_both) {
    Store store;
    Cache cache(store, 5);
    cache.put(1, "one");
    cache.remove(1);
    CHECK(!cache.is_cached(1));
    CHECK(!store.peek(1).has_value());
    CHECK(!cache.get(1).has_value());
}

TEST(skewed_traffic_gets_high_hit_ratio) {
    const int keys = 10000, capacity = 500, requests = 200000;
    Store store(20);
    fill(store, keys);
    Cache cache(store, capacity);  // holds 5% of the keys
    Zipf zipf(keys, 1);

    const auto ms_before = store.simulated_ms();
    for (int i = 0; i < requests; ++i) REQUIRE(cache.get(zipf.next()).has_value());
    const double ratio = cache.stats().hit_ratio();
    const double avg_ms = static_cast<double>(store.simulated_ms() - ms_before) / requests;
    std::printf("    Zipf traffic, cache = 5%% of keys: hit ratio %.1f%%, avg latency %.2f ms (store: 20 ms)\n",
                100 * ratio, avg_ms);
    CHECK(ratio > 0.6);
    CHECK(avg_ms < 8.0);
}

TEST(uniform_traffic_hit_ratio_is_about_cache_fraction) {
    const int keys = 10000, capacity = 500, requests = 100000;
    Store store;
    fill(store, keys);
    Cache cache(store, capacity);
    unsigned seed = 5;
    for (int i = 0; i < requests; ++i) {
        seed = seed * 1103515245u + 12345u;
        cache.get(static_cast<int>((seed >> 8) % keys));
    }
    const double ratio = cache.stats().hit_ratio();
    std::printf("    uniform traffic, cache = 5%% of keys: hit ratio %.1f%%\n", 100 * ratio);
    CHECK(std::fabs(ratio - 0.05) < 0.02);  // no locality, so caching barely helps
}

TEST(lfu_keeps_hot_keys_through_a_one_off_scan) {
    // An LRU cache would be flushed by the scan; LFU keeps the frequently used keys.
    Store store;
    fill(store, 2000);
    Cache cache(store, 50);
    for (int round = 0; round < 10; ++round)
        for (int hot = 0; hot < 40; ++hot) cache.get(hot);
    for (int cold = 1000; cold < 2000; ++cold) cache.get(cold);  // e.g. a nightly report
    cache.reset_stats();
    for (int hot = 0; hot < 40; ++hot) cache.get(hot);
    CHECK_EQ(cache.stats().hits, 40u);
}

MINI_TEST_MAIN()
