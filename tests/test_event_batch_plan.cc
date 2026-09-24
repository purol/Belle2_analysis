#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <limits>
#include "event_batch.h"

int main() {
    EventBatchPlan plan(2);
    const std::vector<int> events = {0, 0, 0, 1, 2, 2, 3, 3, 4};
    for (std::size_t i = 0; i < events.size(); i++) plan.Add(i, {{events.at(i)}});
    assert(plan.IsOrdered());
    assert((plan.Finish(events.size()) == std::vector<long long>{4, 8, 9}));

    // Every consumer's grouping must change before a boundary is safe.
    EventBatchPlan coarse(1);
    for (int i = 0; i < 10; i++) coarse.Add(i, {{i}, {i / 3}});
    assert((coarse.Finish(10) == std::vector<long long>{3, 6, 9, 10}));

    // Detect a repeated, nonconsecutive event even after potential boundaries.
    EventBatchPlan unordered(1);
    unordered.Add(0, {{1}}); unordered.Add(1, {{2}}); unordered.Add(2, {{1}});
    assert(!unordered.IsOrdered());
    assert((unordered.Finish(3) == std::vector<long long>{3}));

    EventBatchPlan nan(1);
    nan.Add(0, {{std::numeric_limits<double>::quiet_NaN()}});
    assert(!nan.IsOrdered());
    assert((nan.Finish(1) == std::vector<long long>{1}));

    EventBatchPlan empty(1);
    assert((empty.Finish(0) == std::vector<long long>{0}));
    EventBatchPlan disabled(0);
    for (int i = 0; i < 5; i++) disabled.Add(i, {{i}});
    assert((disabled.Finish(5) == std::vector<long long>{5}));

    // One very large event remains intact; this is deliberately a soft limit.
    EventBatchPlan large(1);
    for (int i = 0; i < 100000; i++) large.Add(i, {{1}});
    large.Add(100000, {{2}});
    assert((large.Finish(100001) == std::vector<long long>{100000, 100001}));

    EventBatchPlan strings(1);
    std::string key = "a";
    strings.Add(0, {{key}}); key = "b";
    strings.Add(1, {{key}});
    assert((strings.Finish(2) == std::vector<long long>{1, 2}));
    std::cout << "Event batch boundary tests passed\n";
}
