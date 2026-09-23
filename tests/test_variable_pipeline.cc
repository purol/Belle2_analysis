#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>

#include "Loader.h"

// Force a stage transition without changing candidates or their order.
class StageBoundary : public Module::Module {
public:
    void Start() override {}
    int Process(std::deque<Data>*) override { return 1; }
    void End() override {}
    bool BlocksDownstream() const override { return true; }
};

class AddFloat : public Module::Module {
public:
    explicit AddFloat(Loader& loader) {
        loader.Getvariable_names_address()->push_back("float_result");
        loader.VariableTypes_address()->push_back("Float_t");
    }
    void Start() override {}
    int Process(std::deque<Data>* data) override {
        for (Data& row : *data) row.variable.push_back(0.125f);
        return 1;
    }
    void End() override {}
};

void CreateInput(const std::string& path) {
    TFile file(path.c_str(), "RECREATE");
    {
        TTree tree("events", "");
        int id, event;
        unsigned int run = 4000000000u;
        float f;
        double d;
        std::string text;
        tree.Branch("id", &id);
        tree.Branch("event", &event);
        tree.Branch("run", &run);
        tree.Branch("f", &f);
        tree.Branch("d", &d);
        tree.Branch("text", &text);
        for (id = 0; id < 12; id++) {
            event = id / 3;
            f = float(id) * 0.25f;
            d = 0.1 + id;
            text = "candidate " + std::to_string(id);
            tree.Fill();
        }
        tree.Write();
    }
}

std::vector<int> SelectedIds(bool random) {
    if (!random) return {2, 5, 8, 11};
    std::mt19937 rng(static_cast<unsigned int>(std::hash<std::string>{}("input.root")));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::vector<int> result;
    for (int event = 0; event < 4; event++) {
        int selected = -1;
        double highest = -1.0;
        for (int id = event * 3; id < event * 3 + 3; id++) {
            if (id == 1) continue;
            double value = dist(rng);
            if (value > highest) { highest = value; selected = id; }
        }
        result.push_back(selected);
    }
    return result;
}

void CheckOutput(const std::string& path, const std::vector<int>& expected, bool after_stage) {
    TFile file(path.c_str(), "READ");
    auto tree = file.Get<TTree>("events");
    assert(tree && tree->GetEntries() == static_cast<Long64_t>(expected.size()));
    assert(std::string(tree->FindLeaf("run")->GetTypeName()) == "UInt_t");
    if (after_stage) {
        assert(!tree->GetBranch("f") && !tree->GetBranch("d"));
        assert(std::string(tree->FindLeaf("float_result")->GetTypeName()) == "Float_t");
    }
    int id;
    unsigned int run;
    double sum;
    std::string* text = nullptr;
    tree->SetBranchAddress("id", &id);
    tree->SetBranchAddress("run", &run);
    tree->SetBranchAddress("sum", &sum);
    tree->SetBranchAddress("text", &text);
    for (std::size_t i = 0; i < expected.size(); i++) {
        tree->GetEntry(i);
        assert(id == expected.at(i));
        assert(run == 4000000000u);
        assert(*text == "candidate " + std::to_string(id));
        assert(sum == (0.1 + id) + static_cast<double>(float(id) * 0.25f));
        if (after_stage) assert(tree->GetLeaf("float_result")->GetValue() == 0.125);
    }
    tree->ResetBranchAddresses();
    delete text;
}

int main(int argc, char* argv[]) {
    assert(argc == 2);
    const std::filesystem::path directory(argv[1]);
    // Require a new directory so that an existing analysis result cannot be overwritten.
    assert(std::filesystem::create_directory(directory));
    CreateInput((directory / "input.root").string());
    EventWeights::Register("test_weight", EventWeight({"id"}, std::vector<WeightBin>{{{{-0.5, 11.5}}, 2.0}}, false));

    for (bool random : {false, true}) {
        const auto output = directory / (random ? "random" : "best");
        std::filesystem::create_directory(output);
        Loader loader("events");
        if (random) loader.LoadWithCut(directory.string().c_str(), "input.root", "signal", "id >= 0");
        else loader.Load(directory.string().c_str(), "input.root", "signal");
        loader.AddWeight("test_weight", {{"id", "id"}});
        loader.DefineNewVariable("d+f", "sum");
        loader.Cut("id != 1");
        auto before = loader.PrintInformation("before BCS", {"event"});
        if (random) loader.RandomBCS({"event"});
        else loader.BCS("id", "highest", {"event"});
        auto after = loader.PrintInformation("after BCS", {"event"});
        loader.PrintSeparateRootFile(output.string().c_str(), "", "");
        loader.InsertCustomizedModule(new StageBoundary());
        loader.RemoveVariable({"d", "f"});
        loader.InsertCustomizedModule(new AddFloat(loader));
        const std::string final_output = (output / "after_stage.root").string();
        loader.PrintRootFile(final_output.c_str());
        loader.end();
        assert(before->at(0) == 8.0 && before->at(1) == 22.0);
        assert(after->at(0) == 8.0 && after->at(1) == 8.0);
        CheckOutput((output / "input.root").string(), SelectedIds(random), false);
        CheckOutput(final_output, SelectedIds(random), true);
    }
    std::cout << "ROOT pipeline regression tests passed\n";
}
