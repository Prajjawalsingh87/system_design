#include "consistent_hash_ring.hpp"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../../testing/mini_test.hpp"

using namespace chash;

namespace {

std::vector<std::string> make_keys(int n) {
    std::vector<std::string> keys;
    keys.reserve(n);
    for (int i = 0; i < n; ++i) keys.push_back("user:" + std::to_string(i));
    return keys;
}

ConsistentHashRing ring_with(int nodes, int vnodes = 160) {
    ConsistentHashRing ring(vnodes);
    for (int i = 0; i < nodes; ++i) ring.add_node("cache-" + std::to_string(i));
    return ring;
}

Placement placement(const ConsistentHashRing& ring) {
    return [&ring](const std::string& key) { return ring.node_for(key); };
}

}  // namespace

TEST(hash_is_stable_across_runs) {
    // Pinned values: the ring layout must never depend on the platform, compiler or run.
    CHECK_EQ(hash64(""), std::uint64_t{6137631918817817679ull});
    CHECK_EQ(hash64("user:42"), std::uint64_t{14509267577577503866ull});
    CHECK(hash64("a") != hash64("b"));
}

TEST(empty_ring_has_no_owner) {
    ConsistentHashRing ring;
    CHECK_THROWS(ring.node_for("k"), std::logic_error);
    CHECK(ring.nodes_for("k", 3).empty());
}

TEST(single_node_owns_everything) {
    ConsistentHashRing ring;
    ring.add_node("only");
    for (const auto& key : make_keys(100)) CHECK_EQ(ring.node_for(key), std::string("only"));
}

TEST(lookup_is_deterministic) {
    ConsistentHashRing a = ring_with(5), b = ring_with(5);
    for (const auto& key : make_keys(1000)) REQUIRE_EQ(a.node_for(key), b.node_for(key));
}

TEST(add_and_remove_bookkeeping) {
    ConsistentHashRing ring(50);
    ring.add_node("a");
    ring.add_node("b", 100);
    CHECK_EQ(ring.node_count(), 2u);
    CHECK_EQ(ring.vnode_count(), 150u);
    CHECK_THROWS(ring.add_node("a"), std::invalid_argument);
    CHECK(ring.remove_node("a"));
    CHECK(!ring.remove_node("a"));
    CHECK_EQ(ring.vnode_count(), 100u);
    CHECK(!ring.has_node("a"));
}

TEST(adding_a_node_moves_about_one_nth_of_keys_and_only_to_it) {
    const auto keys = make_keys(100000);
    ConsistentHashRing before = ring_with(4);
    ConsistentHashRing after = ring_with(4);
    after.add_node("cache-4");

    RebalanceStats s = measure_rebalance(keys, placement(before), placement(after));
    std::printf("    add 5th node: %zu of %zu keys moved (%.1f%%, ideal 20%%)\n", s.moved_keys, s.total_keys,
                100 * s.moved_fraction());
    CHECK(s.moved_fraction() > 0.14 && s.moved_fraction() < 0.26);
    CHECK_EQ(s.moved_to.size(), 1u);  // every moved key went to the new node
    CHECK_EQ(s.moved_to["cache-4"], s.moved_keys);
}

TEST(removing_a_node_moves_only_its_keys) {
    const auto keys = make_keys(100000);
    ConsistentHashRing before = ring_with(5);
    ConsistentHashRing after = ring_with(5);
    after.remove_node("cache-2");

    RebalanceStats s = measure_rebalance(keys, placement(before), placement(after));
    std::printf("    remove 1 of 5 nodes: %.1f%% of keys moved (ideal 20%%)\n", 100 * s.moved_fraction());
    CHECK_EQ(s.moved_keys, s.keys_before["cache-2"]);  // exactly the removed node's keys
    CHECK_EQ(s.keys_after.count("cache-2"), 0u);
    CHECK(s.moved_to.size() >= 3u);  // spread over the survivors thanks to virtual nodes
}

TEST(modulo_sharding_moves_most_keys) {
    const auto keys = make_keys(100000);
    ModuloSharding before({"n0", "n1", "n2", "n3"});
    ModuloSharding after({"n0", "n1", "n2", "n3", "n4"});
    RebalanceStats s = measure_rebalance(
        keys, [&](const std::string& k) { return before.node_for(k); },
        [&](const std::string& k) { return after.node_for(k); });
    std::printf("    modulo 4 -> 5 nodes: %.1f%% of keys moved (expected ~80%%)\n", 100 * s.moved_fraction());
    CHECK(s.moved_fraction() > 0.75);
}

TEST(virtual_nodes_balance_the_load) {
    const auto keys = make_keys(100000);
    auto load_ratio = [&](int vnodes) {
        ConsistentHashRing ring = ring_with(10, vnodes);
        std::map<std::string, std::size_t> load;
        for (const auto& k : keys) ++load[ring.node_for(k)];
        return max_over_mean(load, 10);
    };
    const double one = load_ratio(1), many = load_ratio(200);
    std::printf("    max/mean load, 10 nodes: 1 vnode = %.2f, 200 vnodes = %.2f\n", one, many);
    CHECK(many < 1.2);
    CHECK(many < one);
}

TEST(vnode_count_works_as_weight) {
    // Weights only average out with enough points per node, hence 500 vs 1500.
    ConsistentHashRing ring;
    ring.add_node("small", 500);
    ring.add_node("big", 1500);
    std::map<std::string, std::size_t> load;
    for (const auto& k : make_keys(100000)) ++load[ring.node_for(k)];
    const double ratio = static_cast<double>(load["big"]) / load["small"];
    std::printf("    weight 1:3 -> key ratio 1:%.2f\n", ratio);
    CHECK(ratio > 2.5 && ratio < 3.5);
}

TEST(replica_nodes_are_distinct_and_start_with_owner) {
    ConsistentHashRing ring = ring_with(5);
    for (const auto& key : make_keys(500)) {
        auto replicas = ring.nodes_for(key, 3);
        REQUIRE_EQ(replicas.size(), 3u);
        CHECK_EQ(replicas[0], ring.node_for(key));
        CHECK_EQ(std::set<std::string>(replicas.begin(), replicas.end()).size(), 3u);
    }
    CHECK_EQ(ring.nodes_for("k", 10).size(), 5u);  // capped at the number of nodes
}

MINI_TEST_MAIN()
