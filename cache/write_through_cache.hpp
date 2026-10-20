// A write-through cache in front of a slow backing store.
//
//   read:  cache hit  -> return from memory
//          cache miss -> read the store, keep the result in the cache (read-through)
//   write: write the store FIRST, then the cache (write-through), so the cache
//          never holds a value the store does not have.
//
// The cache policy is a template parameter (LFUCache by default). FakeSlowStore
// stands in for a database and counts the calls and the simulated latency, so a
// test can show what the cache saves.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "lfu_cache.hpp"

template <typename Key, typename Value>
class BackingStore {
public:
    virtual ~BackingStore() = default;
    virtual std::optional<Value> read(const Key& key) = 0;
    virtual void write(const Key& key, const Value& value) = 0;
    virtual void remove(const Key& key) = 0;
};

// In-memory "database" that pretends every call takes `latency_ms`. It does not
// actually sleep: the latency is added to a counter, so tests stay fast and deterministic.
template <typename Key, typename Value>
class FakeSlowStore : public BackingStore<Key, Value> {
public:
    explicit FakeSlowStore(std::int64_t latency_ms = 20) : latency_ms_(latency_ms) {}

    std::optional<Value> read(const Key& key) override {
        ++reads_;
        simulated_ms_ += latency_ms_;
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    void write(const Key& key, const Value& value) override {
        ++writes_;
        simulated_ms_ += latency_ms_;
        if (fail_next_write_) {
            fail_next_write_ = false;
            throw std::runtime_error("store write failed");
        }
        data_[key] = value;
    }

    void remove(const Key& key) override {
        ++writes_;
        simulated_ms_ += latency_ms_;
        data_.erase(key);
    }

    void fail_next_write() { fail_next_write_ = true; }  // fault injection for tests
    std::optional<Value> peek(const Key& key) const {    // inspect without counting a call
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    std::uint64_t reads() const { return reads_; }
    std::uint64_t writes() const { return writes_; }
    std::int64_t simulated_ms() const { return simulated_ms_; }

private:
    std::int64_t latency_ms_;
    std::unordered_map<Key, Value> data_;
    std::uint64_t reads_ = 0;
    std::uint64_t writes_ = 0;
    std::int64_t simulated_ms_ = 0;
    bool fail_next_write_ = false;
};

struct CacheStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    double hit_ratio() const { return hits + misses ? static_cast<double>(hits) / (hits + misses) : 0.0; }
};

template <typename Key, typename Value, typename Cache = LFUCache<Key, Value>>
class WriteThroughCache {
public:
    WriteThroughCache(BackingStore<Key, Value>& store, std::size_t capacity) : store_(store), cache_(capacity) {}

    std::optional<Value> get(const Key& key) {
        if (auto cached = cache_.get(key)) {
            ++stats_.hits;
            return cached;
        }
        ++stats_.misses;
        std::optional<Value> loaded = store_.read(key);
        if (loaded) cache_.put(key, *loaded);
        return loaded;
    }

    // If the store write throws, the exception propagates and the cache is left
    // untouched, so a later read never sees a value that was not persisted.
    void put(const Key& key, const Value& value) {
        store_.write(key, value);
        cache_.put(key, value);
    }

    void remove(const Key& key) {
        store_.remove(key);
        cache_.erase(key);
    }

    const CacheStats& stats() const { return stats_; }
    void reset_stats() { stats_ = {}; }
    bool is_cached(const Key& key) const { return cache_.contains(key); }

private:
    BackingStore<Key, Value>& store_;
    Cache cache_;
    CacheStats stats_;
};
