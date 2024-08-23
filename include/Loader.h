#ifndef LOADER_H
#define LOADER_H

#include <string>
#include <vector>
#include <queue>

#include "TH1.h"
#include "TH2.h"
#include "TList.h"
#include "TLeaf.h"
#include "TMath.h"
#include "TStyle.h"
#include "Rtypes.h"
#include "TKey.h"
#include "TTree.h"
#include "THStack.h"
#include "TCanvas.h"
#include "TObjArray.h"

#include "base.h"
#include "data.h"
#include "module.h"

// the name of tree is "information". It is magic string
# define TREE "information"

class Loader {
private:

    // to load ROOT files
    std::string filepath;
    std::string including_string;

    // set loader name, not necessary
    std::string loader_name;

    // flat to denote weather the ROOT structure is defined or not
    bool DataStructureDefined;

    // vector of modules
    std::vector<Module*> Modules;

    enum VariableType
    {
        int_ = 0,
        unsigned_int_,
        float_,
        double_
    };

    std::vector<std::string> variable_names;
    std::vector<Loader::VariableType> VariableTypes;
    std::vector<Data> TotalData;

public:
    Loader(const char* filepath_, const char* including_string_);
    void SetName(const char* loader_name_);
    void Cut(const char* cut_string_);
    void end();
};

Loader::Loader(const char* filepath_, const char* including_string_) : filepath(filepath_), including_string(including_string_), DataStructureDefined(false) {}

void Loader::SetName(const char* loader_name_) {
    loader_name = std::string(loader_name_);
}

void Loader::Cut(const char* cut_string_) {
    Module* temp_module = new Cut(cut_string_);
    Modules.push_back(temp_module);
}

void Loader::end() {

    std::vector<std::string> filename;
    load_files(filepath.c_str(), &filename, including_string.c_str());

    // load ROOT files
    for (unsigned int i = 0; i < filename.size(); i++) {
        TFile* input_file = new TFile((filepath + std::string("/") + filename.at(i)).c_str(), "read");
        if (!loader_name.empty()) printf("[%s] ", loader_name.c_str());
        printf("%s (%d/%zu)\n", ("Read " + filename.at(i) + "... ").c_str(), i, filename.size());

        // read tree
        TTree* temp_tree = (TTree*)input_file->Get(TREE);

        // read list of branches
        TObjArray* temp_branchList = temp_tree->GetListOfBranches();
        int nBranch = temp_tree->GetNbranches();

        // read name of branches and their type
        for (int i = 0; i < nBranch; i++) {
            const char* temp_branch_name = temp_branchList->At(i)->GetName();
            const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();
            printf("%s %s\n", temp_branch_name, TypeName);
        }

        for (int i = 0; i < Modules.size(); i++) Modules.at(i)->Process();

        input_file->Close();
    }

    for (int i = 0; i < Modules.size(); i++) Modules.at(i)->Print();
    for (int i = 0; i < Modules.size(); i++) delete Modules.at(i);
}

#endif 