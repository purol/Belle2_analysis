#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "random_modules.h"
#include "DataStore.h"

int main(int argc, char* argv[]) {
    if (argc != 3) return 2;
    std::string mode = argv[1];
    std::size_t target = std::stoull(argv[2]);
    std::vector<std::string> names = { "event", "candidate", "value" };
    std::vector<std::string> types = { "Int_t", "Int_t", "Double_t" };
    std::vector<EventWeight*> weights;
    std::vector<std::vector<std::size_t>> indices;
    std::map<std::string, double> internal;
    std::vector<std::unique_ptr<Module::Module>> modules;
    if (mode == "random" || mode == "pipeline") {
        modules.emplace_back(new Module::GetRandom({ "value", "value+100", "value+200" }, "random1", &names, &types, &weights, &indices));
        modules.emplace_back(new Module::GetRandom({ "value+300", "value+400" }, "random2", &names, &types, &weights, &indices));
    }
    if (mode == "bcs" || mode == "pipeline") {
        modules.emplace_back(new Module::RandomBCS({ "event" }, &names, &types, &weights, &indices, &internal));
    }
    if (mode == "selection0" || mode == "selection1" || mode == "pipeline") {
        modules.emplace_back(new Module::RandomEventSelection(2, mode == "selection1" ? 1 : 0, { "event" }, &names, &types, &weights, &indices, &internal));
    }
    if (modules.empty()) return 2;
    for (auto& module : modules) module->Start();

    // Include consecutive equal filenames, a return to an earlier filename, and
    // a completely rejected file. File IDs are metadata, never part of the seed.
    const std::vector<std::string> files = { "alpha.root", "empty.root", "beta.root", "same.root", "same.root", "alpha.root" };
    for (std::size_t file = 0; file < files.size(); file++) {
        std::deque<Data> batch;
        std::size_t input_entries = 0;
        auto process = [&]() {
            // Exercise the real stage store, including preservation of file identity.
            if (!batch.empty()) {
                MemoryDataStore store;
                std::vector<std::string> input_names = { "event", "candidate", "value" };
                std::vector<std::string> input_types = { "Int_t", "Int_t", "Double_t" };
                store.SetSchema(input_names, input_types, input_names, input_types);
                store.WriteToBatch(std::move(batch));
                if (!store.ReadFromBatch(&batch)) exit(3);
#ifdef HAS_FILE_ID
                if (batch.front().file_id != file + 1) exit(3);
#endif
            }
            for (auto& module : modules) module->Process(&batch);
            for (const Data& entry : batch) {
                std::cout << file << ' ' << entry.filename << ' ' << std::get<int>(entry.variable.at(0)) << ' ' << std::get<int>(entry.variable.at(1));
                for (std::size_t i = 2; i < entry.variable.size(); i++) std::cout << ' ' << std::hexfloat << std::get<double>(entry.variable.at(i));
                std::cout << '\n';
            }
            batch.clear();
            input_entries = 0;
            // Simulate an upstream cut rejecting whole batches between outputs.
            for (auto& module : modules) module->Process(&batch);
        };
        for (int event = 0; event < 73; event++) {
            int count = 1 + (event * 5) % 9;
            for (int candidate = 0; candidate < count; candidate++) {
                input_entries++;
                // Preserve complete input events, but drop candidates and whole events.
                if (file == 1 || (event >= 9 && event < 22) || (candidate + event) % 4 == 0) continue;
                Data entry;
                entry.filename = files.at(file);
                entry.label = "sample";
#ifdef HAS_FILE_ID
                entry.file_id = file + 1;
#endif
                entry.variable = { event, candidate, static_cast<double>(event * 10 + candidate) / 8.0 };
                batch.push_back(std::move(entry));
            }
            if (target != 0 && input_entries >= target) process();
        }
        process();
    }
    for (auto& module : modules) module->End();
    return 0;
}
