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

    // label list to assign which one is signal/background
    std::vector<std::string> Signal_label_list;
    std::vector<std::string> Background_label_list;

    // label list to assign which one is MC/data
    std::vector<std::string> MC_label_list;
    std::vector<std::string> Data_label_list;

    std::vector<Data> TotalData;

public:
    Loader(const char* TTree_name_);
    void SetName(const char* loader_name_);

    /*
     * set MC and data sample by label.
     * This classification is used for `DrawStack`
     */
    void SetMC(std::vector<std::string> labels_);
    void SetData(std::vector<std::string> labels_);

    /*
     * set signal and background sample by label.
     * This classification is used for `DrawFOM` and `DrawStack`
     */
    void SetSignal(std::vector<std::string> labels_);
    void SetBackground(std::vector<std::string> labels_);

    void Load(const char* dirname_, const char* including_string_, const char* label_);
    void Cut(const char* cut_string_);
    void PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_);
    void DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_);
    void DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_);
    void DrawStack(const char* expression_, const char* stack_title_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_);
    void PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_);
    void PrintRootFile(const char* output_name_);
    void BCS(const char* expression_, const char* criteria_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void IsBCSValid(const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void DrawFOM(const char* equation_, double MIN_, double MAX_, const char* png_name_);
    void FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, bool MEMORY_SAFE_, const char* path_);
    void InsertCustomizedModule(Module::Module* module_);
    void end();

    /*
     * get pointer when make the customized module
     */
    std::vector<std::string>* Getvariable_names_address();
    std::vector<std::string>* VariableTypes_address();
};

Loader::Loader(const char* TTree_name_) : TTree_name(TTree_name_), DataStructureDefined(false) {}

void Loader::SetName(const char* loader_name_) {
    loader_name = std::string(loader_name_);
}

void Loader::SetMC(std::vector<std::string> labels_) {
    MC_label_list = labels_;
}

void Loader::SetData(std::vector<std::string> labels_) {
    Data_label_list = labels_;
}

void Loader::SetSignal(std::vector<std::string> labels_) {
    Signal_label_list = labels_;
}

void Loader::SetBackground(std::vector<std::string> labels_) {
    Background_label_list = labels_;
}

void Loader::Load(const char* dirname_, const char* including_string_, const char* label_) {
    Module::Module* temp_module = new Module::Load(dirname_, including_string_, label_, &DataStructureDefined, &variable_names, &VariableTypes, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::Cut(const char* cut_string_) {
    Module::Module* temp_module = new Module::Cut(cut_string_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::PrintInformation(print_string_, Event_variable_list_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, nbins_, x_low_, x_high_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, nbins_, x_low_, x_high_, png_name_, normalized_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, png_name_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, png_name_, normalized_, &variable_names, &VariableTypes);
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

void Loader::DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, nbins_, x_low_, x_high_, png_name_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, nbins_, x_low_, x_high_, png_name_, normalized_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, png_name_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, png_name_, normalized_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes);
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

void Loader::RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::RandomEventSelection(split_num_, selected_index_, Event_variable_list_, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::DrawFOM(const char* expression_, double MIN_, double MAX_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawFOM(expression_, MIN_, MAX_, png_name_, Signal_label_list, Background_label_list, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, bool MEMORY_SAFE_, const char* path_) {
    Module::Module* temp_module = new Module::FastBDTTrain(input_variables_, Signal_preselection_, Background_preselection_, hyperparameters_, MEMORY_SAFE_, path_, Signal_label_list, Background_label_list, &variable_names, &VariableTypes);
    Modules.push_back(temp_module);
}

void Loader::InsertCustomizedModule(Module::Module* module_) {
    // function to insert the customized module
    Modules.push_back(module_);
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

    printf("[Loader] loader %s is successfully done\n", loader_name.c_str());
}

std::vector<std::string>* Loader::Getvariable_names_address() {
    return (&variable_names);
}

std::vector<std::string>* Loader::VariableTypes_address() {
    return (&VariableTypes);
}

#endif 