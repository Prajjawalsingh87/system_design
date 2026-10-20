# URL shortener (core)

The core logic of a TinyURL/Bitly-style service: turning long URLs into short codes, redirecting, expiry, click counting and custom aliases.

| File | What it does |
|---|---|
| [base62.hpp](base62.hpp) | `encode(uint64)` and `decode(string)` with the alphabet `[0-9a-zA-Z]`. Any 64-bit ID fits in at most 11 characters. `decode` rejects bad characters and overflow. |
| [snowflake_id.hpp](snowflake_id.hpp) | `SnowflakeIdGenerator`: 41 bits of milliseconds, 10 bits of machine id and 12 bits of sequence. It is thread-safe, the clock is injectable, it waits for the next millisecond when the sequence runs out, and it refuses to run when the clock goes backwards. |
| [url_shortener.hpp](url_shortener.hpp) | `UrlShortener`: an in-memory store with `shorten`, `resolve` (counts a click), `info`, `remove` and `purge_expired`, plus optional TTLs and collision-safe custom aliases. |
| [url_shortener_test.cpp](url_shortener_test.cpp) | 19 tests. |

## The problem in real systems

A shortener is a **read-heavy key-value lookup** (redirects far outnumber creates) that must hand out **unique, short, hard-to-guess-sequentially IDs** across many servers without a central bottleneck.

```text
POST /shorten  ->  id = snowflake.next()  ->  code = base62(id)  ->  store[code] = url
GET  /{code}   ->  store[code] (cache first)  ->  301/302 redirect, clicks++
```

## Design choices and trade-offs

- **ID generation: Snowflake vs alternatives**
  - *Auto-increment in one DB*: simple, but a single point of failure and a write bottleneck, and the codes are guessable.
  - *Hash of the URL (MD5 + truncate)*: the same URL always gets the same code, but truncation collides, so you need a check-and-retry loop.
  - *Random codes*: also need a uniqueness check on every insert.
  - *Snowflake (used here)*: every machine generates IDs locally with no coordination, they are unique by construction and roughly time ordered. The cost is that you must assign machine ids and trust the clock (we throw if it goes backwards).
- **Base62** keeps codes URL-safe without escaping. A 64-bit ID needs at most 11 characters. Our codes are about 10 characters, because the timestamp occupies the high bits.
- **Custom aliases** share the namespace with generated codes. An alias is rejected while its live owner exists, and can be reclaimed once that link expires. If a generated code ever equals an existing alias, the generator skips to the next ID. Aliases are limited to `[0-9a-zA-Z_-]{3,32}`.
- **Expiry** is enforced **lazily** on read (an expired link looks absent right away) and memory is reclaimed by `purge_expired()`, the job a background sweeper or the store's TTL feature does in production (for example Redis `EXPIRE` or DynamoDB TTL).
- **Click counting** here is an in-memory counter under the same lock. At scale you would not write to the DB on every redirect: emit an event to a log or stream (Kafka) and aggregate it asynchronously.
- **301 vs 302**: a 301 lets browsers cache the redirect (less load, but you lose click analytics), while a 302 always hits the service.

## Complexity

| Operation | Time |
|---|---|
| `base62::encode` / `decode` | O(length) = O(1) for 64-bit IDs |
| `SnowflakeIdGenerator::next` | O(1) (except when it waits for the next ms after 4096 IDs in one ms) |
| `shorten`, `resolve`, `info`, `remove` | O(1) average (hash map) |
| `purge_expired` | O(n) |

## Run the tests

```bash
mkdir -p build && g++ -std=c++17 -Wall hld/url_shortener/url_shortener_test.cpp -o build/url_shortener_test && ./build/url_shortener_test
```
