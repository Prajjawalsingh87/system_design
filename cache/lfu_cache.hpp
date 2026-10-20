// LFU (least frequently used) cache with O(1) get and put.
//
// Keys are grouped into buckets by access count. Each bucket is a list ordered
// by recency, so ties between equally used keys are broken LRU-style. We track
// the smallest non-empty frequency (min_freq_) to find the eviction victim in O(1).
#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LFUCache {
public:
    explicit LFUCache(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) throw std::invalid_argument("LFUCache capacity must be > 0");
    }

    std::optional<Value> get(const Key& key) {
        auto it = entries_.find(key);
        if (it == entries_.end()) return std::nullopt;
        bump(it->second, key);
        return it->second.value;
    }

    // Inserts or updates (an update also counts as a use). Returns the evicted key, if any.
    std::optional<Key> put(const Key& key, Value value) {
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            it->second.value = std::move(value);
            bump(it->second, key);
            return std::nullopt;
        }

        std::optional<Key> evicted;
        if (entries_.size() == capacity_) {
            std::list<Key>& victims = buckets_[min_freq_];
            evicted = victims.back();  // least recently used among the least frequently used
            victims.pop_back();
            if (victims.empty()) buckets_.erase(min_freq_);
            entries_.erase(*evicted);
        }

        std::list<Key>& ones = buckets_[1];
        ones.push_front(key);
        entries_.emplace(key, Entry{std::move(value), 1, ones.begin()});
        min_freq_ = 1;
        return evicted;
    }

    bool erase(const Key& key) {
        auto it = entries_.find(key);
        if (it == entries_.end()) return false;
        const bool emptied = remove_from_bucket(it->second);
        if (emptied && it->second.freq == min_freq_) recompute_min_freq();
        entries_.erase(it);
        return true;
    }

    bool contains(const Key& key) const { return entries_.count(key) != 0; }
    std::size_t size() const { return entries_.size(); }
    std::size_t capacity() const { return capacity_; }

    std::size_t frequency(const Key& key) const {
        auto it = entries_.find(key);
        return it == entries_.end() ? 0 : it->second.freq;
    }

private:
    struct Entry {
        Value value;
        std::size_t freq;
        typename std::list<Key>::iterator pos;  // position inside buckets_[freq]
    };

    // Returns true if the bucket became empty (and was removed).
    bool remove_from_bucket(Entry& e) {
        auto b = buckets_.find(e.freq);
        b->second.erase(e.pos);
        if (!b->second.empty()) return false;
        buckets_.erase(b);
        return true;
    }

    // Only needed after erase(): O(number of distinct frequencies), not on the get/put path.
    void recompute_min_freq() {
        min_freq_ = 0;
        for (const auto& [freq, keys] : buckets_)
            if (min_freq_ == 0 || freq < min_freq_) min_freq_ = freq;
    }

    // Move a key from bucket f to bucket f + 1. If bucket f was the minimum and is
    // now empty, the new minimum is f + 1 (where this key just went).
    void bump(Entry& e, const Key& key) {
        if (remove_from_bucket(e) && e.freq == min_freq_) ++min_freq_;
        ++e.freq;
        std::list<Key>& bucket = buckets_[e.freq];
        bucket.push_front(key);
        e.pos = bucket.begin();
    }

    std::size_t capacity_;
    std::size_t min_freq_ = 0;
    std::unordered_map<Key, Entry, Hash> entries_;
    std::unordered_map<std::size_t, std::list<Key>> buckets_;  // freq -> keys, front = most recent
};
