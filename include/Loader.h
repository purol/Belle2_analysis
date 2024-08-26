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
    void PrintInformation(const char* print_string_);
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_);
    void end();
};

Loader::Loader() : DataStructureDefined(false) {}

void Loader::SetName(const char* loader_name_) {
    loader_name = std::string(loader_name_);
}

void Loader::Load(const char* dirname_, const char* including_string_, const char* category_) {
    Module::Module* temp_module = new Module::Load(dirname_, including_string_, category_, &DataStructureDefined, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::Cut(const char* cut_string_) {
    Module::Module* temp_module = new Module::Cut(cut_string_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::PrintInformation(const char* print_string_) {
    Module::Module* temp_module = new Module::PrintInformation(print_string_);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, nbins_, x_low_, x_high_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH2D(x_expression_, y_expression_, hist_title_, x_nbins_, x_low_, x_high_, y_nbins_, y_low_, y_high_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void end() {
    // run Start
    for (int i = 0; i < Modules.size(); i++) Modules.at(i)->Start();

    while (true) {
        AreAllFilesRead = true;

        // run Process
        if (Modules.at(j)->Process(&TotalData) == 0) AreAllFilesRead = false;

        // clear remaining data
        TotalData.clear();

        // If all files are read, exit from while loop
        if (AreAllFilesRead) break;
    }

    // run End
    for (int i = 0; i < Modules.size(); i++) Modules.at(i)->End();

    // delete all modules
    for (int i = 0; i < Modules.size(); i++) delete Modules.at(i);
}

#endif 