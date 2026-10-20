// Core of a URL shortener: generated short codes (Snowflake ID -> Base62),
// collision-safe custom aliases, optional expiry and click counting, all in an
// in-memory store. A real service would put a database or key-value store
// behind the same interface and a cache in front of resolve().
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <string>
#include <unordered_map>

#include "base62.hpp"
#include "snowflake_id.hpp"

namespace shortener {

using Clock = std::function<std::int64_t()>;  // Unix time in milliseconds
using Millis = std::chrono::milliseconds;

class AliasTakenError : public std::runtime_error {
public:
    explicit AliasTakenError(const std::string& alias) : std::runtime_error("alias already in use: " + alias) {}
};

struct ShortenOptions {
    std::optional<std::string> custom_alias;  // e.g. "summer-sale"
    std::optional<Millis> ttl;                // no value = never expires
};

struct LinkInfo {
    std::string code;
    std::string long_url;
    std::int64_t created_ms = 0;
    std::optional<std::int64_t> expires_ms;
    std::uint64_t clicks = 0;
    bool custom = false;
};

class UrlShortener {
public:
    explicit UrlShortener(Clock clock = &SnowflakeIdGenerator::system_clock_ms, std::int64_t machine_id = 0)
        : clock_(clock), ids_(machine_id, clock) {}

    // Returns the short code. Throws std::invalid_argument for a bad URL, alias or
    // TTL, and AliasTakenError if the custom alias belongs to a live link.
    std::string shorten(const std::string& long_url, const ShortenOptions& options = {}) {
        validate_url(long_url);
        if (options.ttl && options.ttl->count() <= 0) throw std::invalid_argument("ttl must be positive");
        if (options.custom_alias) validate_alias(*options.custom_alias);

        std::lock_guard<std::mutex> lock(mutex_);
        const std::int64_t now = clock_();

        std::string code;
        if (options.custom_alias) {
            code = *options.custom_alias;
            auto it = links_.find(code);
            if (it != links_.end()) {
                if (!is_expired(it->second, now)) throw AliasTakenError(code);
                links_.erase(it);  // expired aliases can be claimed again
            }
        } else {
            // IDs are unique, so a generated code can only clash with a custom alias
            // that happens to look like Base62. Skip to the next ID in that case.
            do {
                code = base62::encode(ids_.next());
            } while (links_.count(code));
        }

        LinkInfo info;
        info.code = code;
        info.long_url = long_url;
        info.created_ms = now;
        if (options.ttl) info.expires_ms = now + options.ttl->count();
        info.custom = options.custom_alias.has_value();
        links_.emplace(code, std::move(info));
        return code;
    }

    // The redirect path: returns the long URL and counts a click, or nothing if
    // the code is unknown or expired.
    std::optional<std::string> resolve(const std::string& code) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(code);
        if (it == links_.end() || is_expired(it->second, clock_())) return std::nullopt;
        ++it->second.clicks;
        return it->second.long_url;
    }

    // Analytics view; does not count as a click. Expired links are reported as absent.
    std::optional<LinkInfo> info(const std::string& code) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(code);
        if (it == links_.end() || is_expired(it->second, clock_())) return std::nullopt;
        return it->second;
    }

    bool remove(const std::string& code) {
        std::lock_guard<std::mutex> lock(mutex_);
        return links_.erase(code) != 0;
    }

    // Expired entries are hidden lazily; this frees their memory. Returns how many were removed.
    std::size_t purge_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::int64_t now = clock_();
        std::size_t removed = 0;
        for (auto it = links_.begin(); it != links_.end();) {
            if (is_expired(it->second, now)) {
                it = links_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::size_t stored_links() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return links_.size();
    }

    static bool is_valid_alias(const std::string& alias) {
        if (alias.size() < 3 || alias.size() > 32) return false;
        for (char c : alias)
            if (base62::digit_value(c) < 0 && c != '-' && c != '_') return false;
        return true;
    }

private:
    static bool is_expired(const LinkInfo& link, std::int64_t now) { return link.expires_ms && now >= *link.expires_ms; }

    static void validate_url(const std::string& url) {
        const bool http = url.rfind("http://", 0) == 0 && url.size() > 7;
        const bool https = url.rfind("https://", 0) == 0 && url.size() > 8;
        if (!http && !https) throw std::invalid_argument("URL must start with http:// or https://");
        if (url.size() > 2048) throw std::invalid_argument("URL too long");
        for (char ch : url) {
            const auto c = static_cast<unsigned char>(ch);  // same result whether char is signed or not
            if (c <= ' ' || c == 0x7f) throw std::invalid_argument("URL contains whitespace or control characters");
            if (c >= 0x80) throw std::invalid_argument("URL must be ASCII (percent-encode other characters)");
        }
    }

    static void validate_alias(const std::string& alias) {
        if (!is_valid_alias(alias)) throw std::invalid_argument("alias must be 3-32 chars of [0-9a-zA-Z_-]");
    }

    Clock clock_;
    SnowflakeIdGenerator ids_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, LinkInfo> links_;  // short code -> link
};

}  // namespace shortener
