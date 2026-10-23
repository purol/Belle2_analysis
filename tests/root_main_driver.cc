#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <iomanip>

#include "Loader.h"
#include "TROOT.h"

const std::vector<std::string> root_event_variables = {
    "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__"
};

void CreateFixture(const std::string& path, int shift) {
    TDirectory::TContext context;
    TFile file(path.c_str(), "RECREATE");
    TTree* tree = new TTree("events", "");
    int experiment = 1, run = 2, event = 0, production = 3, candidates = 0, row = 0, keep = 0;
    double x = 0;
    float y = 0;
    tree->Branch("__experiment__", &experiment);
    tree->Branch("__run__", &run);
    tree->Branch("__event__", &event);
    tree->Branch("__production__", &production);
    tree->Branch("__ncandidates__", &candidates);
    tree->Branch("row", &row);
    tree->Branch("keep", &keep);
    tree->Branch("x", &x);
    tree->Branch("y", &y);
    // More than 100,000 entries, with partially and fully rejected event groups.
    for (event = 0; event < 24000; event++) {
        candidates = 1 + (event * 5) % 9;
        for (int candidate = 0; candidate < candidates; candidate++) {
            x = static_cast<double>((event * 13 + candidate + shift) % 400) / 16.0;
            y = static_cast<float>((event * 3 + candidate) % 100) / 8.0f;
            keep = (event % 37 >= 12) && ((candidate + event) % 4 != 0);
            tree->Fill();
            row++;
        }
    }
    tree->Write();
}

void DumpTree(const std::filesystem::path& path, std::ostream& output) {
    TDirectory::TContext context;
    TFile file(path.string().c_str(), "READ");
    if (file.IsZombie()) exit(1);
    TTree* tree = (TTree*)file.Get("events");
    if (tree == nullptr) exit(1);
    output << "TREE " << tree->GetEntries() << '\n';
    TObjArray* branches = tree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); i++) {
        const char* name = branches->At(i)->GetName();
        output << name << ' ' << tree->FindLeaf(name)->GetTypeName() << '\n';
    }
    for (Long64_t entry = 0; entry < tree->GetEntries(); entry++) {
        tree->GetEntry(entry);
        for (int i = 0; i < branches->GetEntries(); i++) {
            output << std::hexfloat << tree->FindLeaf(branches->At(i)->GetName())->GetValue() << ' ';
        }
        output << '\n';
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 2;
    gROOT->SetBatch(true);
    // The unmodified baseline otherwise attaches auto histograms to its output
    // TFile and deletes them twice at shutdown. Apply the same ownership policy
    // to both binaries; batch_loading.cc separately tests the default policy.
    TH1::AddDirectory(false);
    std::string command = argv[1];
    std::filesystem::path directory = argv[2];
    if (command == "create") {
        std::filesystem::create_directories(directory);
        CreateFixture((directory / "alpha.root").string(), 0);
        CreateFixture((directory / "beta.root").string(), 7);
        return 0;
    }
    if (command == "dump") {
        std::ofstream output(directory / "trees.txt");
        for (const std::string& name : { "all.root", "mid/alpha.root", "mid/beta.root", "final/alpha.root", "final/beta.root" }) {
            output << name << '\n';
            DumpTree(directory / name, output);
        }
        return output.good() ? 0 : 1;
    }
    if (command != "run" || argc != 7) return 2;
    std::filesystem::path output_directory = argv[3];
    std::size_t batch_size = std::stoull(argv[4]);
    bool with_cut = std::stoi(argv[5]) != 0;
    bool random_modules = std::stoi(argv[6]) != 0;
    std::filesystem::create_directories(output_directory / "mid");
    std::filesystem::create_directories(output_directory / "final");
    TH1D hist("snapshot_hist", "", 40, 0, 30);
    hist.SetDirectory(nullptr);
    hist.Sumw2();
    TH2D hist2("snapshot_hist2", "", 20, 0, 30, 10, 0, 15);
    hist2.SetDirectory(nullptr);
    hist2.Sumw2();
    Loader loader("events");
    // Explicit load order makes the baseline independent of filesystem enumeration.
    for (const char* filename : { "alpha.root", "beta.root" }) {
#ifdef BATCH_FEATURE
        if (with_cut) loader.LoadWithCut(directory.string().c_str(), filename, "sample", "keep > 0", root_event_variables, batch_size);
        else loader.Load(directory.string().c_str(), filename, "sample", root_event_variables, batch_size);
#else
        if (with_cut) loader.LoadWithCut(directory.string().c_str(), filename, "sample", "keep > 0");
        else loader.Load(directory.string().c_str(), filename, "sample");
#endif
    }
    auto initial = loader.PrintInformation("initial");
    loader.DefineNewVariable("x+y", "sum");
    loader.GetAverage({ "x", "y" }, "average");
    loader.GetStdDev({ "x", "y" }, "stddev");
    loader.ConditionalPairDefineNewVariable({ { "x", "y" }, { "y", "x" } }, 0, "paired");
    // Force wholly empty downstream batches as well as partially accepted batches.
    loader.Cut("(__event__ < 200) || (__event__ > 700)");
    loader.PrintSeparateRootFile((output_directory / "mid").string().c_str(), "", "");
    if (random_modules) {
        loader.GetRandom({ "x", "y", "sum" }, "random_value");
        loader.RandomBCS();
        loader.RandomEventSelection(2, 0);
    }
    else loader.BCS("x", "HIGHEST");
    auto final = loader.PrintInformation("final");
    loader.FillTH1D(&hist, "x");
    loader.FillTH2D(&hist2, "x", "y");
    loader.DrawTH1D("x", ";x;entries", (output_directory / "auto_hist.png").string().c_str());
    loader.DrawTH2D("x", "y", ";x;y", (output_directory / "auto_hist2.png").string().c_str(), "COLZ");
    loader.PrintSeparateRootFile((output_directory / "final").string().c_str(), "", "");
    loader.PrintRootFile((output_directory / "all.root").string().c_str());
    loader.end();
    std::ofstream summary(output_directory / "summary.txt");
    for (auto counts : { initial, final }) {
        for (double value : *counts) summary << std::hexfloat << value << ' ';
        summary << '\n';
    }
    for (TH1* histogram : { static_cast<TH1*>(&hist), static_cast<TH1*>(&hist2) }) {
        summary << std::hexfloat << histogram->GetEntries() << '\n';
        for (int i = 0; i < histogram->GetNcells(); i++) {
            summary << histogram->GetBinContent(i) << ' ' << histogram->GetBinError(i) << '\n';
        }
    }
    return summary.good() ? 0 : 1;
}
