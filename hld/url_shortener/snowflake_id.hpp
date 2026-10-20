// Snowflake-style 64-bit ID generator (the layout Twitter popularised):
//
//   | 1 bit unused | 41 bits ms since custom epoch | 10 bits machine id | 12 bits sequence |
//
// IDs are unique across machines without coordination, roughly time ordered,
// and fit in an int64. 41 bits of milliseconds is about 69 years; 12 bits of
// sequence allows 4096 IDs per millisecond per machine.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>

class SnowflakeIdGenerator {
public:
    using Clock = std::function<std::int64_t()>;  // returns Unix time in milliseconds

    static constexpr int kTimestampBits = 41;
    static constexpr int kMachineBits = 10;
    static constexpr int kSequenceBits = 12;
    static constexpr std::int64_t kMaxMachineId = (1 << kMachineBits) - 1;
    static constexpr std::int64_t kMaxSequence = (1 << kSequenceBits) - 1;
    static constexpr std::int64_t kMaxTimestamp = (std::int64_t{1} << kTimestampBits) - 1;
    static constexpr std::int64_t kDefaultEpochMs = 1704067200000;  // 2024-01-01T00:00:00Z

    static std::int64_t system_clock_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    explicit SnowflakeIdGenerator(std::int64_t machine_id, Clock clock = &system_clock_ms,
                                  std::int64_t epoch_ms = kDefaultEpochMs)
        : machine_id_(machine_id), clock_(std::move(clock)), epoch_ms_(epoch_ms) {
        if (machine_id < 0 || machine_id > kMaxMachineId) throw std::invalid_argument("machine id must be in [0, 1023]");
        if (!clock_) throw std::invalid_argument("clock is required");
    }

    std::uint64_t next() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::int64_t now = clock_();
        if (now < last_ms_) {
            // The wall clock moved backwards (e.g. NTP correction). Issuing IDs now
            // could repeat old ones, so refuse instead.
            throw std::runtime_error("clock moved backwards");
        }
        if (now == last_ms_) {
            sequence_ = (sequence_ + 1) & kMaxSequence;
            if (sequence_ == 0) {
                // 4096 IDs already issued this millisecond: wait for the next one.
                while ((now = clock_()) <= last_ms_) {
                    if (now < last_ms_) throw std::runtime_error("clock moved backwards");
                }
            }
        } else {
            sequence_ = 0;
        }
        last_ms_ = now;

        const std::int64_t ts = now - epoch_ms_;
        if (ts < 0 || ts > kMaxTimestamp) throw std::runtime_error("timestamp outside the 41-bit range for this epoch");
        return (static_cast<std::uint64_t>(ts) << (kMachineBits + kSequenceBits)) |
               (static_cast<std::uint64_t>(machine_id_) << kSequenceBits) | static_cast<std::uint64_t>(sequence_);
    }

    struct Parts {
        std::int64_t timestamp_ms;  // Unix ms
        std::int64_t machine_id;
        std::int64_t sequence;
    };

    Parts decompose(std::uint64_t id) const {
        return Parts{static_cast<std::int64_t>(id >> (kMachineBits + kSequenceBits)) + epoch_ms_,
                     static_cast<std::int64_t>((id >> kSequenceBits) & kMaxMachineId),
                     static_cast<std::int64_t>(id & kMaxSequence)};
    }

private:
    const std::int64_t machine_id_;
    Clock clock_;
    const std::int64_t epoch_ms_;
    std::mutex mutex_;
    std::int64_t last_ms_ = -1;
    std::int64_t sequence_ = 0;
};
