# Consistent hashing

A hash ring with virtual nodes, used to decide **which server owns a key** in a way that survives servers joining and leaving. Code: [consistent_hash_ring.hpp](consistent_hash_ring.hpp), tests: [consistent_hashing_test.cpp](consistent_hashing_test.cpp).

## The problem in real systems

Distributed caches (Memcached clients), databases (Dynamo, Cassandra, Riak), CDNs and load balancers all spread keys over N servers. The naive `server = hash(key) % N` moves almost **every** key when N changes. For a cache that means a mass of misses hitting the database at the same moment.

Consistent hashing places servers and keys on the same ring. A key belongs to the first server clockwise from it, so when a server joins or leaves, only the keys in the arcs it gains or loses move: about **1/N** of them.

## API

| Method | What it does |
|---|---|
| `add_node(name, vnodes)` | Places `vnodes` points for the node on the ring (the default is 160). More points means more weight. |
| `remove_node(name)` | Removes all of the node's points. Only its keys move. |
| `node_for(key)` | Owner lookup: `lower_bound(hash(key))` on a `std::map`, wrapping around. O(log V). |
| `nodes_for(key, n)` | The first `n` distinct nodes clockwise, which is where replicas go (as in Dynamo or Cassandra). |
| `measure_rebalance(keys, before, after)` | Stats: how many keys moved, and where they went. |
| `ModuloSharding` | The naive `hash % N` scheme, for comparison. |

## Measured results (100,000 keys, from the test output)

| Change | Keys moved |
|---|---|
| Consistent hashing, add a 5th node to 4 | **19.5%** (ideal 20%), all to the new node |
| Consistent hashing, remove 1 of 5 nodes | **18.6%** (ideal 20%), only the removed node's keys |
| `hash % N`, 4 to 5 nodes | **80.1%** |

| Load balance, 10 nodes | max / mean load |
|---|---|
| 1 point per node | 2.64 |
| 200 virtual nodes per node | 1.09 |

## Trade-offs

- **Virtual nodes** smooth out the load and spread a departed node's keys over all survivors instead of dumping them on one neighbour. The cost is memory (V points per node) and slightly slower lookups (O(log total points)).
- **Weights** come for free: give a bigger machine more virtual nodes.
- **The hash must be stable** across processes and platforms, because every client must agree on where a key lives. `std::hash` does not guarantee that, so this uses FNV-1a with a splitmix64 finalizer, and the tests pin its output.
- Alternatives: **rendezvous (HRW) hashing** needs no ring and gives perfect minimal movement, but costs O(N) per lookup. **Jump consistent hash** is O(1) memory and very even, but only supports adding or removing the last bucket.

## Run the tests

```bash
mkdir -p build && g++ -std=c++17 -Wall hld/consistent_hashing/consistent_hashing_test.cpp -o build/consistent_hashing_test && ./build/consistent_hashing_test
```
