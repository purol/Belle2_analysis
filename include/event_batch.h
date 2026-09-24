#ifndef EVENT_BATCH_H
#define EVENT_BATCH_H

#include <cmath>
#include <cstddef>
#include <string>
#include <variant>
#include <utility>
#include <vector>

// Own string keys: ROOT reuses its branch buffers on the next entry.
using EventBatchValue = std::variant<int, unsigned int, float, double, std::string>;
using EventBatchKey = std::vector<EventBatchValue>;

// A streaming prepass verifies ordered event keys before any batch is emitted.
// If a grouping is unordered, the reader retains the original whole-file behavior.
class EventBatchPlan {
private:
    std::size_t event_limit;
    std::size_t event_count = 0;
    bool ordered = true;
    bool first_entry = true;
    std::vector<EventBatchKey> previous_keys;
    std::vector<long long> ends;

public:
    explicit EventBatchPlan(std::size_t event_limit_) : event_limit(event_limit_) {}

    void Add(long long entry, const std::vector<EventBatchKey>& keys) {
        if (!ordered) return;
        for (const EventBatchKey& key : keys) {
            for (const EventBatchValue& value : key) {
                if ((value.index() == 2 && !std::isfinite(std::get<float>(value)))
                    || (value.index() == 3 && !std::isfinite(std::get<double>(value)))) {
                    ordered = false;
                    return;
                }
            }
        }
        bool boundary = !first_entry;
        if (!first_entry) {
            for (std::size_t i = 0; i < keys.size(); i++) {
                if (keys.at(i) < previous_keys.at(i)) ordered = false;
                if (keys.at(i) == previous_keys.at(i)) boundary = false;
            }
        }
        if (boundary && event_count >= event_limit) {
            ends.push_back(entry);
            event_count = 0;
        }
        if (first_entry || keys.front() != previous_keys.front()) event_count++;
        previous_keys = keys;
        first_entry = false;
    }

    bool IsOrdered() const { return ordered; }

    std::vector<long long> Finish(long long entries) {
        if (!ordered || event_limit == 0) ends.clear();
        ends.push_back(entries);
        return std::move(ends);
    }
};

#endif
