# Cache: LFU + write-through

| File | What it does |
|---|---|
| [lfu_cache.hpp](lfu_cache.hpp) | Generic `LFUCache<K, V>` with O(1) `get`/`put`: frequency buckets, each an LRU-ordered list, plus a tracked minimum frequency. |
| [write_through_cache.hpp](write_through_cache.hpp) | `WriteThroughCache<K, V, Cache = LFUCache>` in front of a `BackingStore`. Reads are read-through (load on a miss). Writes go to the store first, then the cache. Hit/miss stats included. `FakeSlowStore` simulates a 20 ms database and counts calls and latency. |
| [lfu_cache_test.cpp](lfu_cache_test.cpp) | 8 tests, including a 5000-step comparison against a reference model. |
| [write_through_cache_test.cpp](write_through_cache_test.cpp) | 9 tests, including measured hit ratios. |

## The problem in real systems

Databases are slow (milliseconds) and expensive to scale. Memory is fast (microseconds). Most real traffic is **skewed**: a few keys get most of the reads. A small cache in front of the store absorbs those reads.

## Measured results (from the test output)

10,000 keys, cache capacity 500 (5% of the keys), store latency 20 ms:

| Traffic | Hit ratio | Average read latency |
|---|---|---|
| Zipf (skewed, realistic) | **66.3%** | **6.7 ms** (vs 20 ms without a cache) |
| Uniform random | 5.0% (no locality, so caching barely helps) | |

## Design choices and trade-offs

- **LFU vs LRU**: LFU keeps keys that are popular over the long run and is **scan resistant**: a one-off scan of cold keys (a nightly report) does not flush the hot set (there is a test for this). LRU adapts faster when popularity shifts, while LFU can hold on to keys that *used to be* hot. Production systems often use a mix (Redis `allkeys-lfu` with decay, W-TinyLFU in Caffeine).
- **O(1) LFU**: `key -> (value, freq, position)` plus `freq -> list of keys`, and `min_freq`. A hit moves the key from bucket `f` to bucket `f + 1`. Eviction pops the back (least recent) of the `min_freq` bucket.
- **Write-through vs alternatives**:
  - *Write-through (used here)*: the store is written first, then the cache, so the cache is never ahead of the store, and a failed write leaves the cache untouched (tested). The cost is that writes pay the store latency.
  - *Write-back / write-behind*: writes only touch the cache and are flushed later. Writes are fast, but data can be lost on a crash.
  - *Cache-aside with invalidation*: the application writes the DB and deletes the cache key. This is common with Redis, but there are race windows where a stale value is re-cached.
- **Not thread-safe** on purpose, to keep the algorithm readable. Wrap it in a mutex or shard it by key for concurrent use.

## Run the tests

```bash
mkdir -p build && g++ -std=c++17 -Wall cache/lfu_cache_test.cpp -o build/lfu_cache_test && ./build/lfu_cache_test
mkdir -p build && g++ -std=c++17 -Wall cache/write_through_cache_test.cpp -o build/write_through_cache_test && ./build/write_through_cache_test
```
