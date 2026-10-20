#include "url_shortener.hpp"

#include <atomic>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "../../testing/mini_test.hpp"

using namespace shortener;

namespace {

// A clock the test controls. Shared so the shortener and the test see the same time.
struct FakeClock {
    std::shared_ptr<std::atomic<std::int64_t>> ms = std::make_shared<std::atomic<std::int64_t>>(1735689600000);  // 2025-01-01
    Clock fn() const {
        auto p = ms;
        return [p] { return p->load(); };
    }
    void advance(std::int64_t d) { *ms += d; }
};

}  // namespace

// ---- Base62 ----

TEST(base62_known_values) {
    CHECK_EQ(base62::encode(0), std::string("0"));
    CHECK_EQ(base62::encode(61), std::string("Z"));
    CHECK_EQ(base62::encode(62), std::string("10"));
    CHECK_EQ(base62::encode(3843), std::string("ZZ"));  // 62^2 - 1
    CHECK_EQ(base62::decode("10"), 62u);
    CHECK_EQ(base62::encode(UINT64_MAX).size(), 11u);
}

TEST(base62_round_trip) {
    std::uint64_t x = 1;
    for (int i = 0; i < 2000; ++i) {
        REQUIRE_EQ(base62::decode(base62::encode(x)), x);
        x = x * 6364136223846793005ull + 1442695040888963407ull;
    }
    CHECK_EQ(base62::decode(base62::encode(UINT64_MAX)), UINT64_MAX);
}

TEST(base62_rejects_bad_input) {
    CHECK_THROWS(base62::decode(""), std::invalid_argument);
    CHECK_THROWS(base62::decode("abc$"), std::invalid_argument);
    CHECK_THROWS(base62::decode("zzzzzzzzzzzz"), std::overflow_error);  // > 2^64
}

// ---- Snowflake ----

TEST(snowflake_ids_are_unique_and_increasing) {
    FakeClock clock;
    SnowflakeIdGenerator gen(7, clock.fn());
    std::uint64_t prev = gen.next();
    for (int i = 0; i < 10000; ++i) {
        if (i % 100 == 0) clock.advance(1);
        const std::uint64_t id = gen.next();
        REQUIRE(id > prev);
        prev = id;
    }
}

TEST(snowflake_layout_decomposes) {
    FakeClock clock;
    SnowflakeIdGenerator gen(42, clock.fn());
    gen.next();
    const auto parts = gen.decompose(gen.next());
    CHECK_EQ(parts.timestamp_ms, clock.ms->load());
    CHECK_EQ(parts.machine_id, 42);
    CHECK_EQ(parts.sequence, 1);
}

TEST(snowflake_different_machines_never_collide) {
    FakeClock clock;
    SnowflakeIdGenerator a(1, clock.fn()), b(2, clock.fn());
    std::set<std::uint64_t> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.insert(a.next());
        ids.insert(b.next());
    }
    CHECK_EQ(ids.size(), 2000u);
}

TEST(snowflake_waits_for_next_ms_when_sequence_exhausted) {
    // Clock stays on one millisecond for the first 4100 reads, then moves on.
    int reads = 0;
    const std::int64_t start = 1735689600000;
    SnowflakeIdGenerator gen(0, [&reads, start] { return start + (++reads > 4100 ? 1 : 0); });
    std::set<std::uint64_t> ids;
    for (int i = 0; i < 4097; ++i) ids.insert(gen.next());
    CHECK_EQ(ids.size(), 4097u);
    const auto last = gen.decompose(*ids.rbegin());
    CHECK_EQ(last.timestamp_ms, start + 1);
    CHECK_EQ(last.sequence, 0);
}

TEST(snowflake_rejects_clock_going_backwards) {
    FakeClock clock;
    SnowflakeIdGenerator gen(0, clock.fn());
    gen.next();
    clock.advance(-5);
    CHECK_THROWS(gen.next(), std::runtime_error);
}

TEST(snowflake_rejects_bad_machine_id) {
    CHECK_THROWS(SnowflakeIdGenerator(1024), std::invalid_argument);
    CHECK_THROWS(SnowflakeIdGenerator(-1), std::invalid_argument);
}

TEST(snowflake_is_thread_safe) {
    SnowflakeIdGenerator gen(3);  // real clock
    std::vector<std::vector<std::uint64_t>> per_thread(4);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&, t] {
            for (int i = 0; i < 5000; ++i) per_thread[t].push_back(gen.next());
        });
    for (auto& th : threads) th.join();
    std::set<std::uint64_t> all;
    for (auto& v : per_thread) all.insert(v.begin(), v.end());
    CHECK_EQ(all.size(), 20000u);
}

// ---- Shortener ----

TEST(shorten_and_resolve) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    const std::string code = s.shorten("https://example.com/a/very/long/path?q=1");
    CHECK(code.size() <= 11);
    auto url = s.resolve(code);
    REQUIRE(url.has_value());
    CHECK_EQ(*url, std::string("https://example.com/a/very/long/path?q=1"));
    CHECK(!s.resolve("doesNotExist").has_value());
}

TEST(each_shorten_gets_a_new_code) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    std::set<std::string> codes;
    for (int i = 0; i < 1000; ++i) codes.insert(s.shorten("https://example.com/same"));
    CHECK_EQ(codes.size(), 1000u);
}

TEST(clicks_are_counted) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    const std::string code = s.shorten("https://example.com");
    for (int i = 0; i < 5; ++i) s.resolve(code);
    auto info = s.info(code);
    REQUIRE(info.has_value());
    CHECK_EQ(info->clicks, 5u);
    s.info(code);  // viewing stats is not a click
    CHECK_EQ(s.info(code)->clicks, 5u);
}

TEST(links_expire_after_ttl) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    ShortenOptions opts;
    opts.ttl = Millis(60000);
    const std::string code = s.shorten("https://example.com/promo", opts);
    const std::string forever = s.shorten("https://example.com/home");
    clock.advance(59999);
    CHECK(s.resolve(code).has_value());
    clock.advance(1);
    CHECK(!s.resolve(code).has_value());
    CHECK(!s.info(code).has_value());
    CHECK(s.resolve(forever).has_value());
    CHECK_EQ(s.stored_links(), 2u);  // removed lazily...
    CHECK_EQ(s.purge_expired(), 1u);  // ...or by the purge job
    CHECK_EQ(s.stored_links(), 1u);
}

TEST(custom_alias_works_and_is_collision_safe) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    ShortenOptions opts;
    opts.custom_alias = "summer-sale";
    CHECK_EQ(s.shorten("https://shop.example.com/summer", opts), std::string("summer-sale"));
    CHECK_EQ(*s.resolve("summer-sale"), std::string("https://shop.example.com/summer"));
    CHECK_THROWS(s.shorten("https://evil.example.com", opts), AliasTakenError);
    CHECK_EQ(*s.resolve("summer-sale"), std::string("https://shop.example.com/summer"));  // unchanged
    CHECK(s.info("summer-sale")->custom);
}

TEST(expired_alias_can_be_reclaimed) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    ShortenOptions opts;
    opts.custom_alias = "launch";
    opts.ttl = Millis(1000);
    s.shorten("https://old.example.com", opts);
    clock.advance(1000);
    opts.ttl.reset();
    CHECK_EQ(s.shorten("https://new.example.com", opts), std::string("launch"));
    CHECK_EQ(*s.resolve("launch"), std::string("https://new.example.com"));
}

TEST(generated_code_skips_a_custom_alias_that_looks_like_base62) {
    // Predict the next generated code, claim it as an alias, then shorten normally.
    FakeClock clock;
    SnowflakeIdGenerator twin(0, clock.fn());
    const std::string predicted = base62::encode(twin.next());

    UrlShortener s(clock.fn(), 0);
    ShortenOptions opts;
    opts.custom_alias = predicted;
    s.shorten("https://alias.example.com", opts);
    const std::string code = s.shorten("https://generated.example.com");
    CHECK(code != predicted);
    CHECK_EQ(*s.resolve(predicted), std::string("https://alias.example.com"));
    CHECK_EQ(*s.resolve(code), std::string("https://generated.example.com"));
}

TEST(invalid_input_is_rejected) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    CHECK_THROWS(s.shorten("ftp://example.com"), std::invalid_argument);
    CHECK_THROWS(s.shorten("https://"), std::invalid_argument);
    CHECK_THROWS(s.shorten("https://exa mple.com"), std::invalid_argument);
    CHECK_THROWS(s.shorten("https://example.com/caf\xc3\xa9"), std::invalid_argument);  // raw UTF-8
    CHECK(!s.shorten("https://example.com/caf%C3%A9").empty());                          // percent-encoded is fine
    ShortenOptions bad_alias;
    bad_alias.custom_alias = "a/b";
    CHECK_THROWS(s.shorten("https://example.com", bad_alias), std::invalid_argument);
    bad_alias.custom_alias = "ab";
    CHECK_THROWS(s.shorten("https://example.com", bad_alias), std::invalid_argument);
    ShortenOptions bad_ttl;
    bad_ttl.ttl = Millis(0);
    CHECK_THROWS(s.shorten("https://example.com", bad_ttl), std::invalid_argument);
}

TEST(remove_deletes_link) {
    FakeClock clock;
    UrlShortener s(clock.fn());
    const std::string code = s.shorten("https://example.com");
    CHECK(s.remove(code));
    CHECK(!s.remove(code));
    CHECK(!s.resolve(code).has_value());
}

MINI_TEST_MAIN()
