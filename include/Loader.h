#ifndef LOADER_H
#define LOADER_H

#include <string>
#include <vector>
#include <variant>

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

    // data structure variables
    bool DataStructureDefined;
    std::vector<std::string> variable_names;
    std::vector<std::string> VariableTypes;

    // vector of modules
    std::vector<Module::Module*> Modules;

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
    Module::Module* temp_module = new Module::Cut(cut_string_);
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

        // read/check name of branches and their type
        std::vector<std::variant<int, unsigned int, float, double>> temp_variable;
        for (int j = 0; j < temp_tree->GetNbranches(); j++) {
            const char* temp_branch_name = temp_branchList->At(j)->GetName();
            const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();

            if (DataStructureDefined == false) {

                if (strcmp(TypeName, "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(TypeName, "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(TypeName, "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(TypeName, "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    if (!loader_name.empty()) printf("[%s] ", loader_name.c_str());
                    printf("unexpected data type: %s\n", TypeName);
                }

                variable_names.push_back(temp_branch_name);
                VariableTypes.push_back(std::string(TypeName));

                DataStructureDefined = true;
            }
            else {
                if (variable_names.at(j) != std::string(temp_branch_name)) {
                    if (!loader_name.empty()) printf("[%s] ", loader_name.c_str());
                    printf("variable name is different: %s %s\n", variable_names.at(j).c_str(), temp_branch_name);
                    exit(1);
                }
                else if (VariableTypes.at(j) != std::string(TypeName)) {
                    if (!loader_name.empty()) printf("[%s] ", loader_name.c_str());
                    printf("type is different: %s %s\n", VariableTypes.at(j).c_str(), TypeName);
                    exit(1);
                }
            }

            // set branch addresses
            if (strcmp(TypeName, "Double_t") == 0) {
                temp_tree->SetBranchAddress(temp_branch_name, &std::get<double>(temp_variable.at(j)));
            }
            else if (strcmp(TypeName, "Int_t") == 0) {
                temp_tree->SetBranchAddress(temp_branch_name, &std::get<int>(temp_variable.at(j)));
            }
            else if (strcmp(TypeName, "UInt_t") == 0) {
                temp_tree->SetBranchAddress(temp_branch_name, &std::get<unsigned int>(temp_variable.at(j)));
            }
            else if (strcmp(TypeName, "Float_t") == 0) {
                temp_tree->SetBranchAddress(temp_branch_name, &std::get<float>(temp_variable.at(j)));
            }

        }

        // fill Data vector
        for (unsigned int j = 0; j < temp_tree->GetEntries(); j++) {
            temp_tree->GetEntry(j);
            
            Data temp = { temp_variable };
            TotalData.push_back(temp);
        }

        for (int j = 0; j < Modules.size(); j++) Modules.at(j)->Process(&TotalData);

        input_file->Close();
    }

    for (int i = 0; i < Modules.size(); i++) Modules.at(i)->Print();
    for (int i = 0; i < Modules.size(); i++) delete Modules.at(i);
}

#endif 