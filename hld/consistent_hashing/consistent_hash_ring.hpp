// Consistent hashing ring with virtual nodes.
//
// Every physical node is placed on a 64-bit hash ring many times ("virtual
// nodes"). A key belongs to the first virtual node clockwise from hash(key).
// Adding or removing a node only moves the keys in the arcs that node gains or
// loses (about 1/N of all keys), instead of almost every key as with
// hash(key) % N.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chash {

// Stable 64-bit hash: FNV-1a followed by the splitmix64 finalizer for good
// avalanche. Unlike std::hash it is the same on every platform and run, which
// matters because every server must agree on where a key lives.
inline std::uint64_t hash64(const std::string& s) {
    std::uint64_t h = 14695981039346656037ull;  // FNV-1a 64-bit offset basis
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ull;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebull;
    h ^= h >> 31;
    return h;
}

class ConsistentHashRing {
public:
    explicit ConsistentHashRing(int default_vnodes = 160) : default_vnodes_(default_vnodes) {
        if (default_vnodes <= 0) throw std::invalid_argument("virtual nodes must be positive");
    }

    // `vnodes` acts as a weight: a node with twice the vnodes gets about twice the keys.
    void add_node(const std::string& node, int vnodes = 0) {
        if (vnodes_of_.count(node)) throw std::invalid_argument("node already on ring: " + node);
        if (vnodes == 0) vnodes = default_vnodes_;
        if (vnodes < 0) throw std::invalid_argument("virtual nodes must be positive");

        std::vector<std::uint64_t>& points = vnodes_of_[node];
        for (int i = 0; points.size() < static_cast<std::size_t>(vnodes); ++i) {
            const std::uint64_t point = hash64(node + "#" + std::to_string(i));
            if (ring_.emplace(point, node).second) points.push_back(point);  // skip the rare collision
        }
    }

    bool remove_node(const std::string& node) {
        auto it = vnodes_of_.find(node);
        if (it == vnodes_of_.end()) return false;
        for (std::uint64_t point : it->second) ring_.erase(point);
        vnodes_of_.erase(it);
        return true;
    }

    // O(log V): binary search on the ring for the first point >= hash(key), wrapping around.
    const std::string& node_for(const std::string& key) const {
        if (ring_.empty()) throw std::logic_error("ring has no nodes");
        auto it = ring_.lower_bound(hash64(key));
        if (it == ring_.end()) it = ring_.begin();
        return it->second;
    }

    // The first `count` distinct nodes clockwise from the key: where replicas go
    // (as in Dynamo / Cassandra). The first entry is node_for(key).
    std::vector<std::string> nodes_for(const std::string& key, std::size_t count) const {
        std::vector<std::string> result;
        if (ring_.empty()) return result;
        if (count > vnodes_of_.size()) count = vnodes_of_.size();
        auto it = ring_.lower_bound(hash64(key));
        for (std::size_t steps = 0; result.size() < count && steps < ring_.size(); ++steps, ++it) {
            if (it == ring_.end()) it = ring_.begin();
            bool seen = false;
            for (const std::string& n : result) seen |= (n == it->second);
            if (!seen) result.push_back(it->second);
        }
        return result;
    }

    std::size_t node_count() const { return vnodes_of_.size(); }
    std::size_t vnode_count() const { return ring_.size(); }
    bool has_node(const std::string& node) const { return vnodes_of_.count(node) != 0; }

private:
    int default_vnodes_;
    std::map<std::uint64_t, std::string> ring_;                              // point -> node
    std::unordered_map<std::string, std::vector<std::uint64_t>> vnodes_of_;  // node -> its points
};

// The naive scheme, for comparison: node = nodes[hash(key) % N].
class ModuloSharding {
public:
    explicit ModuloSharding(std::vector<std::string> nodes) : nodes_(std::move(nodes)) {
        if (nodes_.empty()) throw std::invalid_argument("need at least one node");
    }
    const std::string& node_for(const std::string& key) const { return nodes_[hash64(key) % nodes_.size()]; }

private:
    std::vector<std::string> nodes_;
};

// ---- Rebalancing statistics -------------------------------------------------

struct RebalanceStats {
    std::size_t total_keys = 0;
    std::size_t moved_keys = 0;
    std::map<std::string, std::size_t> keys_before;  // node -> key count
    std::map<std::string, std::size_t> keys_after;
    std::map<std::string, std::size_t> moved_to;     // destination node -> keys that moved there
    double moved_fraction() const { return total_keys ? static_cast<double>(moved_keys) / total_keys : 0.0; }
};

using Placement = std::function<std::string(const std::string&)>;

inline RebalanceStats measure_rebalance(const std::vector<std::string>& keys, const Placement& before,
                                        const Placement& after) {
    RebalanceStats stats;
    stats.total_keys = keys.size();
    for (const std::string& key : keys) {
        const std::string a = before(key), b = after(key);
        ++stats.keys_before[a];
        ++stats.keys_after[b];
        if (a != b) {
            ++stats.moved_keys;
            ++stats.moved_to[b];
        }
    }
    return stats;
}

// Largest node load divided by the mean load (1.0 = perfectly balanced).
inline double max_over_mean(const std::map<std::string, std::size_t>& load, std::size_t node_count) {
    std::size_t total = 0, max = 0;
    for (const auto& [node, count] : load) {
        total += count;
        if (count > max) max = count;
    }
    return total ? static_cast<double>(max) * node_count / total : 0.0;
}

}  // namespace chash
