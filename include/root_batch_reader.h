#ifndef ROOT_BATCH_READER_H
#define ROOT_BATCH_READER_H

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <TFile.h>
#include <TDirectory.h>
#include <TTree.h>
#include "data.h"
#include "event_batch.h"

// Shared by Load and LoadWithCut. Only one input file and one batch are active.
class RootBatchReader {
private:
    std::unique_ptr<TFile> input_file;
    TTree* tree = nullptr;
    long long next_entry = 0;
    std::size_t next_batch = 0;
    std::size_t input_file_id = 0;
    std::size_t event_limit = 10000;
    std::vector<std::vector<std::string>> event_groups = {
        {"__experiment__", "__run__", "__event__", "__production__", "__ncandidates__"}};
    std::vector<long long> batch_ends;

public:
    void Configure(std::size_t event_limit_, const std::vector<std::vector<std::string>>& event_groups_) {
        event_limit = event_limit_;
        event_groups = event_groups_;
    }

    bool IsOpen() const { return input_file != nullptr; }
    bool Finished() const { return next_batch == batch_ends.size(); }
    std::size_t GetFileId() const { return input_file_id; }

    void Open(const std::string& path, const std::string& tree_name,
        const std::vector<std::string>& names, const std::vector<std::string>& types,
        std::vector<std::variant<int, unsigned int, float, double, std::string*>>& values) {
        TDirectory::TContext directory_context;
        input_file = std::make_unique<TFile>(path.c_str(), "read");
        if (input_file->IsZombie()) throw std::runtime_error("[RootBatchReader] cannot open " + path);
        tree = dynamic_cast<TTree*>(input_file->Get(tree_name.c_str()));
        if (tree == nullptr) throw std::runtime_error("[RootBatchReader] missing tree in " + path);
        static std::size_t next_file_id = 0;
        input_file_id = ++next_file_id;
        next_entry = 0;
        next_batch = 0;

        for (int i = 0; i < tree->GetNbranches(); i++) {
            int status = 0;
            if (types.at(i) == "Double_t") status = tree->SetBranchAddress(names.at(i).c_str(), &std::get<double>(values.at(i)));
            else if (types.at(i) == "Int_t") status = tree->SetBranchAddress(names.at(i).c_str(), &std::get<int>(values.at(i)));
            else if (types.at(i) == "UInt_t") status = tree->SetBranchAddress(names.at(i).c_str(), &std::get<unsigned int>(values.at(i)));
            else if (types.at(i) == "Float_t") status = tree->SetBranchAddress(names.at(i).c_str(), &std::get<float>(values.at(i)));
            else if (types.at(i) == "string") status = tree->SetBranchAddress(names.at(i).c_str(), &std::get<std::string*>(values.at(i)));
            if (status < 0) throw std::runtime_error("[RootBatchReader] cannot bind " + names.at(i));
        }

        batch_ends = {tree->GetEntries()};
        if (event_limit == 0) return;

        std::vector<std::vector<std::size_t>> group_indices;
        for (const auto& group : event_groups) {
            if (group.empty()) return;
            std::vector<std::size_t> indices;
            for (const std::string& name : group) {
                auto iter = std::find(names.begin(), names.end(), name);
                if (iter == names.end() || tree->GetBranch(name.c_str()) == nullptr) {
                    printf("[RootBatchReader] %s: event key %s is unavailable; reading the whole file\n", path.c_str(), name.c_str());
                    return;
                }
                indices.push_back(std::distance(names.begin(), iter));
            }
            group_indices.push_back(std::move(indices));
        }
        if (group_indices.empty()) return;

        // Read only event identifiers, not the candidate payload, in the prepass.
        tree->SetBranchStatus("*", false);
        for (const auto& group : group_indices) {
            for (std::size_t index : group) tree->SetBranchStatus(names.at(index).c_str(), true);
        }
        EventBatchPlan plan(event_limit);
        for (long long entry = 0; entry < tree->GetEntries(); entry++) {
            if (tree->GetEntry(entry) < 0) throw std::runtime_error("[RootBatchReader] event-key read failed");
            std::vector<EventBatchKey> keys;
            for (const auto& group : group_indices) {
                EventBatchKey key;
                for (std::size_t index : group) {
                    const auto& value = values.at(index);
                    if (value.index() == 0) key.push_back(std::get<int>(value));
                    else if (value.index() == 1) key.push_back(std::get<unsigned int>(value));
                    else if (value.index() == 2) key.push_back(std::get<float>(value));
                    else if (value.index() == 3) key.push_back(std::get<double>(value));
                    else {
                        const std::string* text = std::get<std::string*>(value);
                        key.push_back(text == nullptr ? std::string() : *text);
                    }
                }
                keys.push_back(std::move(key));
            }
            plan.Add(entry, keys);
            if (!plan.IsOrdered()) break;
        }
        tree->SetBranchStatus("*", true);
        if (!plan.IsOrdered()) {
            printf("[RootBatchReader] %s: event keys are unordered or non-finite; reading the whole file\n", path.c_str());
        }
        batch_ends = plan.Finish(tree->GetEntries());
        printf("[RootBatchReader] %s: %zu batch(es)\n", path.c_str(), batch_ends.size());
        fflush(stdout);
    }

    long long GetBatchEnd() const { return batch_ends.at(next_batch); }
    long long GetNextEntry() const { return next_entry; }

    void ReadEntry() {
        if (tree->GetEntry(next_entry) < 0) throw std::runtime_error("[RootBatchReader] candidate read failed");
        next_entry++;
    }

    void FinishBatch() { next_batch++; }

    void Close() {
        TDirectory::TContext directory_context;
        if (tree != nullptr) tree->ResetBranchAddresses();
        tree = nullptr;
        input_file.reset();
        batch_ends.clear();
    }

    ~RootBatchReader() { Close(); }
};

#endif
