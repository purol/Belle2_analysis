#include <stdio.h>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>

#include "Loader.h"
#include "TROOT.h"

const std::vector<std::string> event_variables = {
    "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__"
};

void Check(bool condition, const char* message) {
    if (!condition) {
        printf("[batch_loading] %s\n", message);
        exit(1);
    }
}

void CreateInput(const std::string& path, const std::vector<int>& candidates, int selection = 0) {
    TFile file(path.c_str(), "recreate");
    TTree* tree = new TTree("events", "");
    int experiment = 1, event = 0, ncandidates = 0, row = 0, keep = 0;
    unsigned int run = 2;
    double production = 3.0;
    float event_float = 0.0;
    std::string event_string;
    tree->Branch("__experiment__", &experiment);
    tree->Branch("__run__", &run);
    tree->Branch("__event__", &event);
    tree->Branch("__production__", &production);
    tree->Branch("__ncandidates__", &ncandidates);
    tree->Branch("event_float", &event_float);
    tree->Branch("event_string", &event_string);
    tree->Branch("row", &row);
    tree->Branch("keep", &keep);
    for (int count : candidates) {
        event++;
        event_float = static_cast<float>(event);
        event_string = "event_" + std::to_string(event);
        ncandidates = count;
        for (int i = 0; i < count; i++) {
            keep = selection == 0 ? (row % 2 == 0) : (selection == 1 ? 0 : event != 2);
            tree->Fill();
            row++;
        }
    }
    tree->Write();
    file.Close();
}

struct BatchResult {
    std::vector<std::vector<int>> rows;
    std::vector<std::string> filenames;
    std::map<std::pair<std::string, int>, std::size_t> event_batch;
};

class RecordBatches : public Module::Module {
private:
    BatchResult* result;
    std::size_t row_index;
    std::size_t event_index;
public:
    RecordBatches(BatchResult* result_, const std::vector<std::string>& variable_names) : result(result_) {
        row_index = std::distance(variable_names.begin(), std::find(variable_names.begin(), variable_names.end(), "row"));
        event_index = std::distance(variable_names.begin(), std::find(variable_names.begin(), variable_names.end(), "__event__"));
    }

    void Start() override {}

    int Process(std::deque<Data>* data) override {
        if (data->empty()) return 1;
        std::size_t batch_index = result->rows.size();
        std::vector<int> rows;
        const std::string& filename = data->front().filename;
        for (const Data& entry : *data) {
            Check(entry.filename == filename, "a batch contains multiple files");
            int event = std::get<int>(entry.variable.at(event_index));
            auto inserted = result->event_batch.insert({ { filename, event }, batch_index });
            Check(inserted.second || inserted.first->second == batch_index, "an event was split between batches");
            rows.push_back(std::get<int>(entry.variable.at(row_index)));
        }
        result->rows.push_back(std::move(rows));
        result->filenames.push_back(filename);
        return 1;
    }

    void End() override {}
};

class CheckDirectory : public Module::Module {
private:
    TDirectory* expected;
public:
    CheckDirectory(TDirectory* expected_) : expected(expected_) {}
    void Start() override { Check(gDirectory == expected, "Start changed the caller's ROOT directory"); }
    int Process(std::deque<Data>* data) override {
        Check(gDirectory == expected, "file I/O changed the caller's ROOT directory");
        return 1;
    }
    void End() override { Check(gDirectory == expected, "End changed the caller's ROOT directory"); }
};

void CheckOutput(const std::string& path, const std::vector<int>& expected) {
    TFile file(path.c_str(), "read");
    Check(!file.IsZombie(), "output file was not created");
    TTree* tree = (TTree*)file.Get("events");
    Check(tree != nullptr, "output tree is missing");
    Check(tree->GetEntries() == static_cast<Long64_t>(expected.size()), "output entries were lost or duplicated");
    int row = -1;
    tree->SetBranchAddress("row", &row);
    for (Long64_t i = 0; i < tree->GetEntries(); i++) {
        tree->GetEntry(i);
        Check(row == expected.at(i), "output order or values changed");
    }
    file.Close();
}

BatchResult Run(const std::string& input, const std::string& output, bool with_cut,
    const std::vector<std::string>& keys, std::size_t batch_size, bool random_bcs = false) {
    BatchResult result;
    TDirectory* caller_directory = gDirectory;
    Loader loader("events");
    if (with_cut) loader.LoadWithCut(input.c_str(), ".root", "sample", "keep > 0", keys, batch_size);
    else loader.Load(input.c_str(), ".root", "sample", keys, batch_size);
    Check(gDirectory == caller_directory, "loading the schema changed the caller's ROOT directory");
    loader.InsertCustomizedModule(new RecordBatches(&result, *loader.Getvariable_names_address()));
    if (random_bcs) loader.RandomBCS();
    auto counts = loader.PrintInformation("batch test");
    loader.PrintSeparateRootFile(output.c_str(), "", "");
    loader.InsertCustomizedModule(new CheckDirectory(caller_directory));
    loader.end();
    Check(counts->at(0) == static_cast<double>(result.event_batch.size()), "event count changed across batches");
    if (random_bcs) Check(counts->at(1) == counts->at(0), "RandomBCS did not keep one candidate per event");
    return result;
}

int main(int argc, char* argv[]) {
    gROOT->SetBatch(true);
    // Keep generated ROOT files for inspection; use a fresh directory for every run.
    std::filesystem::path base = std::filesystem::temp_directory_path() / ("belle2_batch_" + generateRandomString(12));
    std::filesystem::create_directories(base / "input");
    CreateInput((base / "input/sample.root").string(), { 2, 4, 1, 3 });

    // These modes should fail before reading any candidates.
    if (argc == 2) {
        Loader loader("events");
        std::string mode = argv[1];
        if (mode == "missing-key") loader.Load((base / "input").string().c_str(), ".root", "sample", { "missing" });
        else if (mode == "empty-key") loader.LoadWithCut((base / "input").string().c_str(), ".root", "sample", "keep > 0", {});
        else if (mode == "zero-size") loader.Load((base / "input").string().c_str(), ".root", "sample", event_variables, 0);
        else return 2;
        loader.end();
        return 0;
    }

    const std::vector<int> all_rows = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const std::vector<int> cut_rows = { 0, 2, 4, 6, 8 };
    for (std::size_t size : { 1, 2, 3, 6, 100 }) {
        for (bool with_cut : { false, true }) {
            std::filesystem::path output = base / (std::to_string(size) + (with_cut ? "_cut" : "_load"));
            std::filesystem::create_directories(output);
            BatchResult result = Run((base / "input").string(), output.string(), with_cut, event_variables, size);
            CheckOutput((output / "sample.root").string(), with_cut ? cut_rows : all_rows);
            if (size == 3) {
                Check(result.rows.size() == 2, "unexpected number of batches");
                Check(result.rows.at(0) == (with_cut ? std::vector<int>{ 0, 2, 4 } : std::vector<int>{ 0, 1, 2, 3, 4, 5 }), "batch target must count input entries before cuts");
            }
        }
    }

    // A string key must compare owned values rather than ROOT's reused string pointer.
    std::filesystem::create_directories(base / "strings");
    BatchResult strings = Run((base / "input").string(), (base / "strings").string(), false, { "event_string", "event_float" }, 1);
    Check(strings.rows.size() == 4, "custom event keys did not identify event boundaries");
    CheckOutput((base / "strings/sample.root").string(), all_rows);

    std::filesystem::create_directories(base / "bcs");
    Run((base / "input").string(), (base / "bcs").string(), false, event_variables, 3, true);

    // Empty files and files rejected by the cut must not stop later input files.
    CreateInput((base / "input/empty.root").string(), {});
    CreateInput((base / "input/rejected.root").string(), { 2, 4, 1, 3 }, 1);
    CreateInput((base / "input/second.root").string(), { 2, 4, 1, 3 });
    std::filesystem::create_directories(base / "multiple");
    Run((base / "input").string(), (base / "multiple").string(), true, event_variables, 3);
    CheckOutput((base / "multiple/sample.root").string(), cut_rows);
    CheckOutput((base / "multiple/second.root").string(), cut_rows);
    Check(!std::filesystem::exists(base / "multiple/rejected.root"), "rejected file should not produce candidates");

    // An empty middle batch must not interleave two Load modules and recreate outputs.
    CreateInput((base / "input/gaps.root").string(), { 2, 4, 1, 3 }, 2);
    std::filesystem::create_directories(base / "gaps");
    {
        Loader loader("events");
        loader.LoadWithCut((base / "input").string().c_str(), "gaps.root", "first", "keep > 0", event_variables, 2);
        loader.Load((base / "input").string().c_str(), "second.root", "second", event_variables, 2);
        loader.PrintSeparateRootFile((base / "gaps").string().c_str(), "", "");
        loader.end();
    }
    CheckOutput((base / "gaps/gaps.root").string(), { 0, 1, 6, 7, 8, 9 });
    CheckOutput((base / "gaps/second.root").string(), all_rows);

    // Two distinct input occurrences with the same name must retain main's
    // overwrite behavior, rather than accidentally appending the second load.
    std::filesystem::create_directories(base / "repeated");
    {
        Loader loader("events");
        loader.Load((base / "input").string().c_str(), "sample.root", "first", event_variables, 2);
        loader.Load((base / "input").string().c_str(), "sample.root", "second", event_variables, 2);
        loader.PrintSeparateRootFile((base / "repeated").string().c_str(), "", "");
        loader.end();
    }
    CheckOutput((base / "repeated/sample.root").string(), all_rows);

    // Trigger automatic histogram construction during Process while a file is
    // current, then close that file before End and the histogram destructor.
    {
        TDirectory::TContext context;
        TFile owner((base / "histogram_owner.root").string().c_str(), "recreate");
        std::vector<std::string> names = { "x" };
        std::vector<std::string> types = { "Double_t" };
        std::vector<EventWeight*> weights;
        std::vector<std::vector<std::size_t>> indices;
        std::map<std::string, double> internal;
        Module::DrawTH1D histogram("x", ";x;entries", (base / "ownership.png").string().c_str(), &names, &types, &weights, &indices, &internal);
        histogram.Start();
        std::deque<Data> data;
        for (int i = 0; i < 1000; i++) {
            Data entry;
            entry.variable.push_back(static_cast<double>(i));
            data.push_back(std::move(entry));
        }
        for (int batch = 0; batch < 1251; batch++) histogram.Process(&data);
        TIter next(owner.GetList());
        while (TObject* object = next()) Check(dynamic_cast<TH1*>(object) == nullptr, "module histogram is owned by a ROOT file");
        owner.Close();
        histogram.End();
    }

    // Exercise the default arguments and the 100,000-entry soft target.
    CreateInput((base / "input/default.root").string(), { 99999, 3, 1 });
    for (bool with_cut : { false, true }) {
        BatchResult result;
        Loader loader("events");
        if (with_cut) loader.LoadWithCut((base / "input").string().c_str(), "default.root", "sample", "row >= 0");
        else loader.Load((base / "input").string().c_str(), "default.root", "sample");
        loader.InsertCustomizedModule(new RecordBatches(&result, *loader.Getvariable_names_address()));
        loader.end();
        Check(result.rows.size() == 2 && result.rows.at(0).size() == 100002 && result.rows.at(1).size() == 1, "default target split an event or used the wrong size");
    }

    // A nonmatching file selection should terminate cleanly without a schema.
    {
        Loader loader("events");
        loader.LoadWithCut((base / "input").string().c_str(), "does_not_exist.root", "sample", "keep > 0");
        loader.end();
    }
    printf("[batch_loading] all checks passed; fixtures: %s\n", base.string().c_str());
    return 0;
}
