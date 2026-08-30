#ifndef LOADER_H
#define LOADER_H

#include <string>
#include <vector>
#include <deque>
#include <variant>
#include <tuple>
#include <memory>
#include <map>
#include <optional>
#include <set>

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
#include "RooDataSet.h"
#include "RooRealVar.h"
#include "TProfile.h"
#include "TH1.h"
#include "TH2.h"

#include "base.h"
#include "data.h"
#include "module.h"
#include "eventweight.h"
#include "fit_manager.h"
#include "DataStore.h"

// wrapper for std::vector<std::vector<Module::Module*>>
class ModuleList {
private:
    std::vector<std::vector<Module::Module*>> Modules;

    // required variables at each stages
    std::vector<std::optional<std::set<std::string>>> required_variables;

    // variable_names at the end of each stages
    std::vector<std::vector<std::string>> variable_names_end_stage;

    // VariableTypes at the end of each stages
    std::vector<std::vector<std::string>> VariableTypes_end_stage;

    // current variable_names at Loader
    std::vector<std::string>* current_variable_names;

    // current VariableTypes at Loader
    std::vector<std::string>* current_VariableTypes;

    bool meetEndOfStage = false;

public:
    ModuleList(std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : current_variable_names(variable_names_), current_VariableTypes(VariableTypes_) {}

    void push_back(Module::Module* temp_module) {
        if (Modules.empty()) {
            std::vector<Module::Module*> temp_stage;
            Modules.push_back(temp_stage);
        }
        Modules.back().push_back(temp_module);

        if (required_variables.empty()) required_variables.push_back(std::set<std::string>{});
        std::optional<std::set<std::string>> RequiredVariables_module = temp_module->RequiredVariables();
        for (std::optional<std::set<std::string>>& required_variable : required_variables) {
            if ((!RequiredVariables_module.has_value()) || (!required_variable.has_value())) required_variable = std::nullopt;
            else required_variable.value().insert(RequiredVariables_module.begin(), RequiredVariables_module.end());
        }

        if (variable_names_end_stage.empty()) variable_names_end_stage.push_back(*current_variable_names);
        else variable_names_end_stage.back() = *current_variable_names;

        if (VariableTypes_end_stage.empty()) VariableTypes_end_stage.push_back(*current_VariableTypes);
        else VariableTypes_end_stage.back() = *current_VariableTypes;

        if (temp_module->BlocksDownstream()) {
            std::vector<Module::Module*> temp_stage;
            Modules.push_back(temp_stage);
            required_variables.push_back(std::set<std::string>{});

            variable_names_end_stage.push_back(std::vector<std::string>{});
            VariableTypes_end_stage.push_back(std::vector<std::string>{});
        }

    }

    std::size_t size() const noexcept {
        return Modules.size();
    }

    std::vector<Module::Module*>& at(std::size_t index) {
        return Modules.at(index);
    }

    const std::vector<Module::Module*>& at(std::size_t index) const {
        return Modules.at(index);
    }

    const std::optional<std::set<std::string>>& GetRequiredVariables(std::size_t stage) const {
        return required_variables.at(stage);
    }

    const std::vector<std::string>& GetVariableNamesEndStage(std::size_t stage) const {
        return variable_names_end_stage.at(stage);
    }

    const std::vector<std::string>& GetVariableTypesEndStage(std::size_t stage) const {
        return VariableTypes_end_stage.at(stage);
    }

};

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

    // vector of modules.
    ModuleList Modules;

    // label list to assign which one is signal/background
    std::vector<std::string> Signal_label_list;
    std::vector<std::string> Background_label_list;

    // label list to assign which one is MC/data
    std::vector<std::string> MC_label_list;
    std::vector<std::string> Data_label_list;

    // for weight calculation
    std::vector<EventWeight*> eventweights;
    std::vector<std::vector<std::size_t>> variable_indices_list;

    // to save memory, std::deque is used
    std::deque<Data> TotalData;

    // for fitting
    FitManager fitmanager;

    // internal values
    std::map<std::string, double> internal_value;

public:
    Loader(const char* TTree_name_, const std::string& workspace_name_ = "workspace");
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

    /*
     * basic modules for analysis
     */
    void Load(const char* dirname_, const char* including_string_, const char* label_);
    void LoadWithCut(const char* dirname_, const char* including_string_, const char* label_, const char* cut_string_);
    void Cut(const char* cut_string_);
    std::shared_ptr<std::vector<double>> PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_);
    void DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_);
    void DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_, bool LogScale_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, const char* draw_option_);
    void DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_, const char* draw_option_);
    void DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_);
    void DrawStack(const char* expression_, const char* stack_title_, const char* png_name_);
    void DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_, bool LogScale_);
    void PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_);
    void PrintRootFile(const char* output_name_);
    void BCS(const char* expression_, const char* criteria_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void RandomBCS(const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void IsBCSValid(const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_ = { "__experiment__", "__run__", "__event__", "__production__", "__ncandidates__" });
    void PrintEvent(std::vector<std::string> print_variables_);
    void AddWeight(const char* weight_name_, const std::vector<std::pair<std::string, std::string>> variable_name_map_ = {});

    /*
     * for FBDT train/test/validation
     */
    std::shared_ptr<std::vector<double>> DrawFOM(const char* equation_, double MIN_, double MAX_, const char* png_name_);
    std::shared_ptr<std::vector<double>> DrawFOM(const char* equation_, double MIN_, double MAX_, double NBin_, int rank_, const char* png_name_);
    std::shared_ptr<std::vector<double>> DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NSIG_initial_, double alpha_, const char* png_name_);
    std::shared_ptr<std::vector<double>> DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NBin_, double NSIG_initial_, double alpha_, int rank_, const char* png_name_);
    std::shared_ptr<std::vector<double>> Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, double NSIG_initial_, double alpha_, const char* png_name_);
    std::shared_ptr<std::vector<double>> Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, const char* preselection_x_, const char* preselection_y_, double NSIG_initial_, double alpha_, const char* png_name_);
    std::shared_ptr<double> CalculateAUC(const char* equation_, double MIN_, double MAX_, const char* output_name_, const char* write_option_);
    void FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, const char* path_, const char* output_name_ = "");
    void FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, bool balanced_weight_, const char* path_, const char* output_name_ = "");
    void FastBDTApplication(std::vector<std::string> input_variables_, const char* classifier_path_, const char* branch_name_);
    
    /*
     * for variable manipulation
     */
    void DefineNewVariable(const char* equation_, const char* new_variable_name_);
    void RemoveVariable(std::vector<std::string> removed_variable_names_);
    void ConditionalPairDefineNewVariable(std::map<std::string, std::string> condition_equation__criteria_equation_list_, int condition_order_, const char* new_variable_name_);
    void GetAverage(std::vector<std::string> equations_, const char* new_variable_name_);
    void GetStdDev(std::vector<std::string> equations_, const char* new_variable_name_);
    void GetDiff(std::vector<std::string> equations_, int order_, const char* new_variable_name_);
    void GetAdd(std::vector<std::string> equations_, int order_, const char* new_variable_name_);

    /*
     * direct manipulation of TObjects
     */
    void FillDataSet(RooDataSet* dataset_, std::vector<RooRealVar*> realvars_, std::vector<std::string> equations_);
    void FillTProfile(TProfile* tprofile_, std::string equation_x_, std::string equation_y_);
    void FillTH1D(TH1D* th1d_, std::string equation_);
    void FillCustomizedTH1D(TH1D* th1d_, std::vector<std::string> equations_, double (*custom_function_)(std::vector<double>));
    void FillTH2D(TH2D* th2d_, const char* x_expression_, const char* y_expression_);
    void FillCustomizedTH2D(TH2D* th2d_, std::vector<std::string> equations_, double (*x_custom_function_)(std::vector<double>), double (*y_custom_function_)(std::vector<double>));

    /*
     * advanced module for analysis
     */
    std::shared_ptr<std::vector<double>> ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, bool WeightSumError_ = true);
    std::shared_ptr<std::vector<double>> ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, const char* region_Aprime_, const char* region_Bprime_, const char* region_Cprime_, const char* region_Dprime_, bool WeightSumError_ = true);
    
    /*
     * for dedicated fits
     */
    void DefineObservable(const std::string& id_, const std::string& title_, double minimum_, double maximum_, const std::string& unit_ = "");
    void DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_);
    void DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_conditions_);
    void DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_ = "");
    void DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_ = "");
    void DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::string>& states_);
    void DefineAndFillProfile(const std::string& profile_id_, const std::string& title_, int bins_, double xmin_, double xmax_, double ymin_, double ymax_, std::string equation_x_, std::string equation_y_);
    void SetParameterConstant(const std::string& id_, bool constant_ = true);
    void SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_);
    void DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options = {});
    void DefineAddModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, const std::vector<std::string>& coefficient_ids_, bool recursive_fractions_ = false);
    void DefineProductModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, double cutoff_ = 0.0);
    void DefineGenericModel(const std::string& model_id_, const std::string& expression_, const std::vector<std::string>& argument_ids_);
    void DefineSimultaneousModel(const std::string& model_id_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_pdf_ids_);
    void DefineTF1(const std::string& function_id_, const std::string& formula_, double xmin_, double xmax_, const std::vector<TF1ParameterDefinition>& parameters_ = {});
    void Fit(const std::string& fit_id_, const std::string dataset_id_, const std::string& model_id_, const FitOptions& options_ = {});
    void PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const FitPlotOptions& options_ = {});
    void PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const std::string& category_state_, const FitPlotOptions& options_ = {});
    void ExportFitResult(const std::string& filename_, const std::vector<std::string>& fit_ids_);
    void CreateNLL(const std::string& nll_id_, const std::string& dataset_id_, const std::string& model_id_, const FitOptions& options_ = {});
    void PlotNLL(const std::string& nll_id_, const std::string& parameter_id_, const std::string& plot_name_);
    void PlotProfileNLL(const std::string& nll_id_, const std::string& poi_id_, const std::string& plot_name_);
    void SaveWorkspace(const std::string& filename_);
    void LoadWorkspace(const std::string& filename_, const std::string& workspace_name_);

    /*
     * advanced module & end module
     */
    void InsertCustomizedModule(Module::Module* module_);
    void end();

    /*
     * get pointer when make the customized module
     */
    std::vector<std::string>* Getvariable_names_address();
    std::vector<std::string>* VariableTypes_address();
    std::vector<std::string>* SignalLabel_address();
    std::vector<std::string>* BackgroundLabel_address();
    std::vector<std::string>* DataLabel_address();
    std::vector<std::string>* MCLabel_address();
};

Loader::Loader(const char* TTree_name_, const std::string& workspace_name_) : TTree_name(TTree_name_), DataStructureDefined(false), Modules(&variable_names, &VariableTypes), fitmanager(workspace_name_) {}

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
    Module::Module* temp_module = new Module::Load(dirname_, including_string_, label_, &DataStructureDefined, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::LoadWithCut(const char* dirname_, const char* including_string_, const char* label_, const char* cut_string_) {
    Module::Module* temp_module = new Module::LoadWithCut(dirname_, including_string_, label_, cut_string_ , &DataStructureDefined, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::Cut(const char* cut_string_) {
    Module::Module* temp_module = new Module::Cut(cut_string_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

std::shared_ptr<std::vector<double>> Loader::PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::PrintInformation(print_string_, Event_variable_list_, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, nbins_, x_low_, x_high_, png_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, nbins_, x_low_, x_high_, png_name_, normalized_, LogScale_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, png_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_, bool LogScale_) {
    Module::Module* temp_module = new Module::DrawTH1D(expression_, hist_title_, png_name_, normalized_, LogScale_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, const char* draw_option_) {
    Module::Module* temp_module = new Module::DrawTH2D(x_expression_, y_expression_, hist_title_, x_nbins_, x_low_, x_high_, y_nbins_, y_low_, y_high_, png_name_, draw_option_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_, const char* draw_option_) {
    Module::Module* temp_module = new Module::DrawTH2D(x_expression_, y_expression_, hist_title_, png_name_, draw_option_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, nbins_, x_low_, x_high_, png_name_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, nbins_, x_low_, x_high_, png_name_, normalized_, LogScale_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, const char* png_name_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, png_name_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_, bool LogScale_) {
    Module::Module* temp_module = new Module::DrawStack(expression_, stack_title_, png_name_, normalized_, LogScale_, Signal_label_list, Background_label_list, Data_label_list, MC_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_) {
    Module::Module* temp_module = new Module::PrintSeparateRootFile(path_, prefix_, suffix_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::PrintRootFile(const char* output_name_) {
    Module::Module* temp_module = new Module::PrintRootFile(output_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, TTree_name.c_str());
    Modules.push_back(temp_module);
}

void Loader::BCS(const char* expression_, const char* criteria_, const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::BCS(expression_, criteria_, Event_variable_list_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::RandomBCS(const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::RandomBCS(Event_variable_list_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::IsBCSValid(const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::IsBCSValid(Event_variable_list_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_) {
    Module::Module* temp_module = new Module::RandomEventSelection(split_num_, selected_index_, Event_variable_list_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

std::shared_ptr<std::vector<double>> Loader::DrawFOM(const char* expression_, double MIN_, double MAX_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::DrawFOM(expression_, MIN_, MAX_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::DrawFOM(const char* expression_, double MIN_, double MAX_, double NBin_, int rank_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::DrawFOM(expression_, MIN_, MAX_, NBin_, rank_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NSIG_initial_, double alpha_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::DrawPunziFOM(equation_, MIN_, MAX_, NSIG_initial_, alpha_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NBin_, double NSIG_initial_, double alpha_, int rank_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::DrawPunziFOM(equation_, MIN_, MAX_, NBin_, NSIG_initial_, alpha_, rank_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, double NSIG_initial_, double alpha_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::Draw2DPunziFOM(scan_conditions_, NSIG_initial_, alpha_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, const char* preselection_x_, const char* preselection_y_, double NSIG_initial_, double alpha_, const char* png_name_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::Draw2DPunziFOM(scan_conditions_, preselection_x_, preselection_y_, NSIG_initial_, alpha_, png_name_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<double> Loader::CalculateAUC(const char* equation_, double MIN_, double MAX_, const char* output_name_, const char* write_option_) {
    std::shared_ptr<double> temp_ptr = std::make_shared<double>();
    Module::Module* temp_module = new Module::CalculateAUC(equation_, MIN_, MAX_, output_name_, write_option_, Signal_label_list, Background_label_list, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

void Loader::FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, const char* path_, const char* output_name_) {
    Module::Module* temp_module = new Module::FastBDTTrain(input_variables_, Signal_preselection_, Background_preselection_, hyperparameters_, path_, output_name_, Signal_label_list, Background_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, bool balanced_weight_, const char* path_, const char* output_name_) {
    Module::Module* temp_module = new Module::FastBDTTrain(input_variables_, Signal_preselection_, Background_preselection_, hyperparameters_, balanced_weight_, path_, output_name_, Signal_label_list, Background_label_list, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FastBDTApplication(std::vector<std::string> input_variables_, const char* classifier_path_, const char* branch_name_) {
    Module::Module* temp_module = new Module::FastBDTApplication(input_variables_, classifier_path_, branch_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DefineNewVariable(const char* equation_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::DefineNewVariable(equation_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::RemoveVariable(std::vector<std::string> removed_variable_names_) {
    Module::Module* temp_module = new Module::RemoveVariable(removed_variable_names_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::ConditionalPairDefineNewVariable(std::map<std::string, std::string> condition_equation__criteria_equation_list_, int condition_order_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::ConditionalPairDefineNewVariable(condition_equation__criteria_equation_list_, condition_order_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::GetAverage(std::vector<std::string> equations_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::GetAverage(equations_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::GetStdDev(std::vector<std::string> equations_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::GetStdDev(equations_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::GetDiff(std::vector<std::string> equations_, int order_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::GetDiff(equations_, order_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::GetAdd(std::vector<std::string> equations_, int order_, const char* new_variable_name_) {
    Module::Module* temp_module = new Module::GetAdd(equations_, order_, new_variable_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillDataSet(RooDataSet* dataset_, std::vector<RooRealVar*> realvars_, std::vector<std::string> equations_) {
    Module::Module* temp_module = new Module::FillDataSet(dataset_, realvars_, equations_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillTProfile(TProfile* tprofile_, std::string equation_x_, std::string equation_y_) {
    Module::Module* temp_module = new Module::FillTProfile(tprofile_, equation_x_, equation_y_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillTH1D(TH1D* th1d_, std::string equation_) {
    Module::Module* temp_module = new Module::FillTH1D(th1d_, equation_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillCustomizedTH1D(TH1D* th1d_, std::vector<std::string> equations_, double (*custom_function_)(std::vector<double>)) {
    Module::Module* temp_module = new Module::FillCustomizedTH1D(th1d_, equations_, custom_function_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillTH2D(TH2D* th2d_, const char* x_expression_, const char* y_expression_) {
    Module::Module* temp_module = new Module::FillTH2D(th2d_, x_expression_, y_expression_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::FillCustomizedTH2D(TH2D* th2d_, std::vector<std::string> equations_, double (*x_custom_function_)(std::vector<double>), double (*y_custom_function_)(std::vector<double>)) {
    Module::Module* temp_module = new Module::FillCustomizedTH2D(th2d_, equations_, x_custom_function_, y_custom_function_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::PrintEvent(std::vector<std::string> print_variables_) {
    Module::Module* temp_module = new Module::PrintEvent(print_variables_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

std::shared_ptr<std::vector<double>> Loader::ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, bool WeightSumError_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::ABCDmethod(region_A_, region_B_, region_C_, region_D_, WeightSumError_, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

std::shared_ptr<std::vector<double>> Loader::ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, const char* region_Aprime_, const char* region_Bprime_, const char* region_Cprime_, const char* region_Dprime_, bool WeightSumError_) {
    std::shared_ptr<std::vector<double>> temp_ptr = std::make_shared<std::vector<double>>();
    Module::Module* temp_module = new Module::ABCDmethod(region_A_, region_B_, region_C_, region_D_, region_Aprime_, region_Bprime_, region_Cprime_, region_Dprime_, WeightSumError_, temp_ptr, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
    return temp_ptr;
}

void Loader::AddWeight(const char* weight_name_, const std::vector<std::pair<std::string, std::string>> variable_name_map_) {
    Module::Module* temp_module = new Module::AddWeight(weight_name_, variable_name_map_, &DataStructureDefined, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value);
    Modules.push_back(temp_module);
}

void Loader::DefineObservable(const std::string& id_, const std::string& title_, double minimum_, double maximum_, const std::string& unit_) {
    Module::Module* temp_module = new Module::DefineObservable(id_, title_, minimum_, maximum_, unit_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_){
    Module::Module* temp_module = new Module::DefineAndFillDataSet(id_, observable_ids_, expressions_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_conditions_){
    Module::Module* temp_module = new Module::DefineAndFillDataSet(id_, observable_ids_, expressions_, category_id_, state_conditions_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_) {
    Module::Module* temp_module = new Module::DefineFitParameter(id_, title_, init_value_, minimum_, maximum_, unit_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_) {
    Module::Module* temp_module = new Module::DefineConstantParameter(id_, title_, value_, unit_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::string>& states_) {
    Module::Module* temp_module = new Module::DefineCategory(id_, title_, states_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineAndFillProfile(const std::string& profile_id_, const std::string& title_, int bins_, double xmin_, double xmax_, double ymin_, double ymax_, std::string equation_x_, std::string equation_y_) {
    Module::Module* temp_module = new Module::DefineAndFillProfile(profile_id_, title_, bins_, xmin_, xmax_, ymin_, ymax_, equation_x_, equation_y_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::SetParameterConstant(const std::string& id_, bool constant_) {
    Module::Module* temp_module = new Module::SetParameterConstant(id_, constant_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_) {
    Module::Module* temp_module = new Module::SetRange(variable_id_, range_name_, minimum_, maximum_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options) {
    Module::Module* temp_module = new Module::DefineModel(model_id_, model_type_, observable_ids_, parameter_ids_, options, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineAddModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, const std::vector<std::string>& coefficient_ids_, bool recursive_fractions_) {
    Module::Module* temp_module = new Module::DefineAddModel(model_id_, pdf_ids_, coefficient_ids_, recursive_fractions_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineProductModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, double cutoff_) {
    Module::Module* temp_module = new Module::DefineProductModel(model_id_, pdf_ids_, cutoff_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineGenericModel(const std::string& model_id_, const std::string& expression_, const std::vector<std::string>& argument_ids_) {
    Module::Module* temp_module = new Module::DefineGenericModel(model_id_, expression_, argument_ids_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineSimultaneousModel(const std::string& model_id_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_pdf_ids_) {
    Module::Module* temp_module = new Module::DefineSimultaneousModel(model_id_, category_id_, state_pdf_ids_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::DefineTF1(const std::string& function_id_, const std::string& formula_, double xmin_, double xmax_, const std::vector<TF1ParameterDefinition>& parameters_) {
    Module::Module* temp_module = new Module::DefineTF1(function_id_, formula_, xmin_, xmax_, parameters_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::Fit(const std::string& fit_id_, const std::string dataset_id_, const std::string& model_id_, const FitOptions& options_) {
    Module::Module* temp_module = new Module::Fit(fit_id_, dataset_id_, model_id_, options_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const FitPlotOptions& options_) {
    Module::Module* temp_module = new Module::PlotFit(fit_id_, observable_id_, plot_name_, options_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const std::string& category_state_, const FitPlotOptions& options_){
    Module::Module* temp_module = new Module::PlotFit(fit_id_, observable_id_, plot_name_, category_state_, options_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::ExportFitResult(const std::string& filename_, const std::vector<std::string>& fit_ids_) {
    Module::Module* temp_module = new Module::ExportFitResult(filename_, fit_ids_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::CreateNLL(const std::string& nll_id_, const std::string& dataset_id_, const std::string& model_id_, const FitOptions& options_) {
    Module::Module* temp_module = new Module::CreateNLL(nll_id_, dataset_id_, model_id_, options_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::PlotNLL(const std::string& nll_id_, const std::string& parameter_id_, const std::string& plot_name_) {
    Module::Module* temp_module = new Module::PlotNLL(nll_id_, parameter_id_, plot_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::PlotProfileNLL(const std::string& nll_id_, const std::string& poi_id_, const std::string& plot_name_) {
    Module::Module* temp_module = new Module::PlotProfileNLL(nll_id_, poi_id_, plot_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::SaveWorkspace(const std::string& filename_) {
    Module::Module* temp_module = new Module::SaveWorkspace(filename_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::LoadWorkspace(const std::string& filename_, const std::string& workspace_name_) {
    Module::Module* temp_module = new Module::LoadWorkspace(filename_, workspace_name_, &variable_names, &VariableTypes, &eventweights, &variable_indices_list, &internal_value, &fitmanager);
    Modules.push_back(temp_module);
}

void Loader::InsertCustomizedModule(Module::Module* module_) {
    // function to insert the customized module
    Modules.push_back(module_);
}

void Loader::end() {
    // temporary data for BlocksDownstream
    MemoryDataStore input_store;
    MemoryDataStore output_store;

    for (int stage = 0; stage < Modules.size(); stage++) {

        // modules at each stage
        std::vector<Module::Module*> Modules_at = Modules.at(stage);

        // run Start
        for (int i = 0; i < Modules_at.size(); i++) Modules_at.at(i)->Start();

        while (true) {
            bool AreAllFilesRead = true;

            // fill data from upsteam
            if(stage != 0) AreAllFilesRead = !input_store.ReadFromBatch(&TotalData);

            // run Process
            for (int i = 0; i < Modules_at.size(); i++) {
                if (Modules_at.at(i)->Process(&TotalData) == 0) AreAllFilesRead = false;
            }

            // if it is not last stage, save data into DataStream
            if ((Modules.size() - 1) != stage) {
                // get reduced schema
                std::vector<std::string> reduced_variable_names;
                std::vector<std::string> reduced_VariableTypes;
                for (int index = 0; index < Modules.GetVariableNamesEndStage(stage + 1).size(); index++) {
                    const std::string& variable_name = Modules.GetVariableNamesEndStage(stage + 1).at(index);
                    const std::string& VariableType = Modules.GetVariableTypesEndStage(stage + 1).at(index);

                    // there can be "DefineVariable" at the next stage
                    if (std::find(Modules.GetVariableNamesEndStage(stage).begin(), Modules.GetVariableNamesEndStage(stage).end(), variable_name) != Modules.GetVariableNamesEndStage(stage).end()) {
                        if (!Modules.GetRequiredVariables(stage + 1).has_value()) {
                            reduced_variable_names.push_back(variable_name);
                            reduced_VariableTypes.push_back(VariableType);
                        }
                        else if (Modules.GetRequiredVariables(stage + 1).value().find(variable_name) != Modules.GetRequiredVariables(stage + 1).value().end()) {
                            reduced_variable_names.push_back(variable_name);
                            reduced_VariableTypes.push_back(VariableType);
                        }
                    }
                }

                output_store.SetSchema(Modules.GetVariableNamesEndStage(stage), Modules.GetVariableTypesEndStage(stage), reduced_variable_names, reduced_VariableTypes);
                output_store.WriteToBatch(std::move(TotalData));
            }
            TotalData.clear();

            // If all files are read, exit from while loop
            if (AreAllFilesRead) break;
        }

        // run End
        for (int i = 0; i < Modules_at.size(); i++) Modules_at.at(i)->End();

        // delete all modules
        for (int i = 0; i < Modules_at.size(); i++) delete Modules_at.at(i);

        input_store.Clear();
        input_store = std::move(output_store);
        output_store.Clear();
    }

    input_store.Clear();
    output_store.Clear();

    printf("[Loader] loader %s is successfully done\n", loader_name.c_str());
}

std::vector<std::string>* Loader::Getvariable_names_address() {
    return (&variable_names);
}

std::vector<std::string>* Loader::VariableTypes_address() {
    return (&VariableTypes);
}

std::vector<std::string>* Loader::SignalLabel_address() {
    return (&Signal_label_list);
}

std::vector<std::string>* Loader::BackgroundLabel_address() {
    return (&Background_label_list);
}

std::vector<std::string>* Loader::DataLabel_address() {
    return (&Data_label_list);
}

std::vector<std::string>* Loader::MCLabel_address() {
    return (&MC_label_list);
}

#endif 