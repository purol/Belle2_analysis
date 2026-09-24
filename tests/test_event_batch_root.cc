#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include <tuple>
#include "Loader.h"

class BatchProbe : public Module::Module {
private:
    std::size_t* peak;
public:
    explicit BatchProbe(std::size_t* peak_) : peak(peak_) {}
    bool SupportsEventBatches() const override { return true; }
    void Start() override {}
    int Process(std::deque<Data>* data) override {
        *peak = std::max(*peak, data->size());
        return 1;
    }
    void End() override {}
};

class StageBoundary : public Module::Module {
public:
    bool SupportsEventBatches() const override { return true; }
    bool BlocksDownstream() const override { return true; }
    void Start() override {}
    int Process(std::deque<Data>*) override { return 1; }
    void End() override {}
};

class LegacyProbe : public Module::Module {
public:
    void Start() override {}
    int Process(std::deque<Data>*) override { return 1; }
    void End() override {}
};

void CreateInput(const std::string& path, bool empty = false, bool unordered = false) {
    TFile file(path.c_str(), "recreate");
    TTree tree("events", "");
    int event, row_id = 0;
    double score;
    unsigned int run = 4000000000u;
    float value;
    std::string text;
    tree.Branch("event", &event);
    tree.Branch("row_id", &row_id);
    tree.Branch("score", &score);
    tree.Branch("run", &run);
    tree.Branch("value", &value);
    tree.Branch("text", &text);
    if (!empty) {
        for (int i = 0; i < 17; i++) {
            event = unordered && i == 16 ? 0 : i;
            int candidates = i == 9 ? 50 : 1 + i % 4;
            for (int j = 0; j < candidates; j++) {
                score = j % 3;
                value = float(row_id) / 7;
                text = "candidate_" + std::to_string(row_id++);
                tree.Fill();
            }
        }
    }
    tree.Write();
}

using Row = std::tuple<int, int, double, unsigned int, float, std::string, double>;
std::vector<Row> ReadOutput(const std::string& path) {
    TFile file(path.c_str(), "read");
    assert(!file.IsZombie());
    auto* tree = dynamic_cast<TTree*>(file.Get("events"));
    assert(tree != nullptr && tree->GetNbranches() == 7);
    int event, id;
    unsigned int run;
    float value;
    double score, random_value;
    std::string* text = nullptr;
    tree->SetBranchAddress("event", &event);
    tree->SetBranchAddress("row_id", &id);
    tree->SetBranchAddress("score", &score);
    tree->SetBranchAddress("run", &run);
    tree->SetBranchAddress("value", &value);
    tree->SetBranchAddress("text", &text);
    tree->SetBranchAddress("random_value", &random_value);
    std::vector<Row> result;
    for (Long64_t i = 0; i < tree->GetEntries(); i++) {
        tree->GetEntry(i);
        result.emplace_back(event, id, score, run, value, *text, random_value);
    }
    tree->ResetBranchAddresses();
    delete text;
    return result;
}

struct Result {
    std::vector<Row> rows;
    std::vector<double> counts;
    std::size_t peak;
};

Result Run(const std::string& input, const std::string& output, std::size_t limit,
    int mode, bool cut, bool barrier, bool legacy = false) {
    std::filesystem::create_directories(output);
    Loader loader("events");
    loader.SetEventBatchSize(limit, {"event"});
    // Register two separate input modules with identical basenames. They must
    // reset RNG state independently, even if the first module emits empty batches.
    if (cut) loader.LoadWithCut((input + "/one").c_str(), "same.root", "one", "event >= 12");
    else loader.Load((input + "/one").c_str(), "same.root", "one");
    loader.Load((input + "/two").c_str(), "same.root", "two");
    loader.Load((input + "/empty").c_str(), "empty.root", "empty");
    Result result;
    result.peak = 0;
    loader.InsertCustomizedModule(new BatchProbe(&result.peak));
    if (legacy) loader.InsertCustomizedModule(new LegacyProbe);
    auto counts = loader.PrintInformation("counts", {"event"});
    if (barrier) loader.InsertCustomizedModule(new StageBoundary);
    if (mode == 1) loader.BCS("score", "HIGHEST", {"event"});
    if (mode == 2) loader.RandomBCS({"event"});
    if (mode == 3) loader.RandomEventSelection(2, 0, {"event"});
    loader.GetRandom({"score", "row_id"}, "random_value");
    loader.PrintSeparateRootFile(output.c_str(), "", "");
    loader.PrintRootFile((output + "/combined.root").c_str());
    loader.end();
    result.rows = ReadOutput(output + "/combined.root");
    result.counts = *counts;
    return result;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::filesystem::path work(argv[1]);
    // Never overwrite user data. Supply a fresh directory for each test run.
    assert(!std::filesystem::exists(work));
    for (const std::string name : {"one", "two", "empty"}) std::filesystem::create_directories(work / "input" / name);
    CreateInput((work / "input/one/same.root").string());
    CreateInput((work / "input/two/same.root").string());
    CreateInput((work / "input/empty/empty.root").string(), true);
    for (bool cut : {false, true}) {
        for (bool barrier : {false, true}) {
            for (int mode = 0; mode < 4; mode++) {
                std::string tag = std::to_string(cut) + "_" + std::to_string(barrier) + "_" + std::to_string(mode);
                std::string baseline = (work / (tag + "_whole")).string();
                auto expected = Run((work / "input").string(), baseline, 0, mode, cut, barrier);
                for (std::size_t limit : {1, 2, 7, 10000}) {
                    std::string output = (work / (tag + "_" + std::to_string(limit))).string();
                    auto actual = Run((work / "input").string(), output, limit, mode, cut, barrier);
                    assert(actual.rows == expected.rows && actual.counts == expected.counts);
                    assert(ReadOutput(output + "/same.root") == ReadOutput(baseline + "/same.root"));
                    if (limit == 1) assert(actual.peak <= 50);
                }
            }
        }
    }
    auto whole = Run((work / "input").string(), (work / "legacy_whole").string(), 0, 0, false, false);
    auto legacy = Run((work / "input").string(), (work / "legacy_batch").string(), 1, 0, false, false, true);
    assert(legacy.rows == whole.rows && legacy.counts == whole.counts && legacy.peak == whole.peak);
    CreateInput((work / "input/one/same.root").string(), false, true);
    whole = Run((work / "input").string(), (work / "unordered_whole").string(), 0, 0, false, false);
    auto unordered = Run((work / "input").string(), (work / "unordered_batch").string(), 1, 0, false, false);
    assert(unordered.rows == whole.rows && unordered.counts == whole.counts && unordered.peak == whole.peak);
    std::cout << "ROOT event batch integration tests passed\n";
}
