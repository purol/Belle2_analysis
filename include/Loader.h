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

class Loader {
private:

    // to load ROOT files
    std::string filepath;
    std::string including_string;

    // set loader name, not necessary
    std::string loader_name;

    // tree name
    std::string TTree_name;

    // data structure variables
    bool DataStructureDefined;
    std::vector<std::string> variable_names;
    std::vector<std::string> VariableTypes;

    // vector of modules
    std::vector<Module::Module*> Modules;

    std::vector<Data> TotalData;

public:
    Loader(const char* TTree_name_);
    void SetName(const char* loader_name_);
    void SetMC(const char* label_);
    void SetData(const char* label_);

    void Load(const char* dirname_, const char* including_string_, const char* label_);
    void Cut(const char* cut_string_);
    void PrintInformation(const char* print_string_);
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, std::vector<std::string> selected_label_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, std::vector<std::string> selected_label_, const char* png_name_);
    void DrawStackandTH1D(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, std::vector<std::string> stack_label_, std::vector<std::string> hist_label_, const char* hist_legend_name_, bool hist_draw_option_, const char* png_name_);
    void DrawStackandTH1D(const char* expression_, const char* stack_title_, std::vector<std::string> stack_label_, std::vector<std::string> hist_label_, const char* hist_legend_name_, bool hist_draw_option_, const char* png_name_);
    void PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_);
    void PrintRootFile(const char* output_name_);
    void BCS(const char* expression_, const char* criteria_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__ncandidates__" });
    void IsBCSValid(const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__ncandidates__" });
    void DrawFOM(const char* variable_name_, double MIN_, double MAX_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, const char* png_name_);
    void end();
};

Loader::Loader(const char* TTree_name_) : TTree_name(TTree_name_), DataStructureDefined(false) {}

void Loader::SetName(const char* loader_name_) {
    loader_name = std::string(loader_name_);
}

void Loader::Load(const char* dirname_, const char* including_string_, const char* label_) {
    Module::Module* temp_module = new Module::Load(dirname_, including_string_, label_, &DataStructureDefined, &variable_names, &VariableTypes, TTree_name.c_str());
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

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH2D(x_expression_, y_expression_, hist_title_, x_nbins_, x_low_, x_high_, y_nbins_, y_low_, y_high_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH2D(x_expression_, y_expression_, hist_title_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, std::vector<std::string> selected_label_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, nbins_, x_low_, x_high_, selected_label_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, std::vector<std::string> selected_label_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, selected_label_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStackandTH1D(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, std::vector<std::string> stack_label_, std::vector<std::string> hist_label_, const char* hist_legend_name_, bool hist_draw_option_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStackandTH1D(expression_, stack_title_, nbins_, x_low_, x_high_, stack_label_, hist_label_, hist_legend_name_, hist_draw_option_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStackandTH1D(const char* expression_, const char* stack_title_, std::vector<std::string> stack_label_, std::vector<std::string> hist_label_, const char* hist_legend_name_, bool hist_draw_option_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStackandTH1D(expression_, stack_title_, stack_label_, hist_label_, hist_legend_name_, hist_draw_option_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_) {
    Module::Module* temp_module = new Module::PrintSeparateRootFile(path_, prefix_, suffix_, &variable_names, &VariableTypes, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::PrintRootFile(const char* output_name_) {
    Module::Module* temp_module = new Module::PrintRootFile(output_name_, &variable_names, &VariableTypes, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::BCS(const char* expression_, const char* criteria_, const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::BCS(expression_, criteria_, Event_variable_list_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::IsBCSValid(const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::IsBCSValid(Event_variable_list_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawFOM(const char* expression_, double MIN_, double MAX_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawFOM(expression_, MIN_, MAX_, Signal_label_list_, Background_label_list_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::end() {
    // run Start
    for (int i = 0; i < Modules.size(); i++) Modules.at(i)->Start();

    while (true) {
        bool AreAllFilesRead = true;

        // run Process
        for (int i = 0; i < Modules.size(); i++) {
            if (Modules.at(i)->Process(&TotalData) == 0) AreAllFilesRead = false;
        }

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