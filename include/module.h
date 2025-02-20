#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <fstream>

#include "data.h"
#include "string_equation.h"
#include "base.h"

#include "Classifier.h"

#include <TGraph.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TPaveText.h>
#include <TFile.h>
#include <RooDataSet.h>
#include <RooRealVar.h>
#include <RooArgSet.h>
#include <TProfile.h>
#include <TH1.h>
#include <TH2.h>

// for the comparison of `std::vector<std::variant<int, unsigned int, float, double, std::string*>>`
struct CompareHistory {
    bool operator()(const std::vector<std::variant<int, unsigned int, float, double, std::string*>>& lhs, const std::vector<std::variant<int, unsigned int, float, double, std::string*>>& rhs) const {
        size_t size = std::min(lhs.size(), rhs.size());

        for (size_t i = 0; i < size; ++i) {
            // Compare by index (type) first
            if (lhs[i].index() != rhs[i].index()) {
                return lhs[i].index() < rhs[i].index();
            }

            // Compare values based on the type in the variant
            if (lhs[i].index() == 0) { // int
                if (std::get<int>(lhs[i]) < std::get<int>(rhs[i])) return true;
                if (std::get<int>(lhs[i]) > std::get<int>(rhs[i])) return false;
            }
            else if (lhs[i].index() == 1) { // unsigned int
                if (std::get<unsigned int>(lhs[i]) < std::get<unsigned int>(rhs[i])) return true;
                if (std::get<unsigned int>(lhs[i]) > std::get<unsigned int>(rhs[i])) return false;
            }
            else if (lhs[i].index() == 2) { // float
                if (std::get<float>(lhs[i]) < std::get<float>(rhs[i])) return true;
                if (std::get<float>(lhs[i]) > std::get<float>(rhs[i])) return false;
            }
            else if (lhs[i].index() == 3) { // double
                if (std::get<double>(lhs[i]) < std::get<double>(rhs[i])) return true;
                if (std::get<double>(lhs[i]) > std::get<double>(rhs[i])) return false;
            }
            else if (lhs[i].index() == 4) { // std::string*
                std::string* lhs_str = std::get<std::string*>(lhs[i]);
                std::string* rhs_str = std::get<std::string*>(rhs[i]);

                if (!lhs_str || !rhs_str) return lhs_str < rhs_str; // Handle null pointers safely
                if (*lhs_str < *rhs_str) return true;
                if (*lhs_str > *rhs_str) return false;
            }
        }

        // If all elements are equal, compare by vector size
        return lhs.size() < rhs.size();
    }
};

/*
* reserved function which always return 1.0
*/
double reserve_function(std::vector<Data>::iterator data_) {
    return 1.0;
}

/*
* global function pointer. This can be modified outside module.h
*/
double (*ObtainWeight)(std::vector<Data>::iterator) = reserve_function;

namespace Module {

    class Module {
    public:
        /*
        * design philosophy:
        * 1. data structure should be modified in constructor. Do not touch data structure in `start`, `process`, and `End` function.
        */
        Module() {}
        virtual ~Module() {}
        /*
        * `Start` function is called after the data structure is determined. It is called only one time.
        */
        virtual void Start() = 0;
        /*
        * `Process` function is called every time for each ROOT file.
        * return: For `Load` module, if it cannot read ROOT file, because there is no more file to read, it is 1. Otherwise, it is 0.
        * For other all modules, it is always 1.
        */
        virtual int Process(std::vector<Data>* data) = 0;
        /*
        * `End` function is called after all ROOT files are read. It is called only once.
        */
        virtual void End() = 0;
    };

    class Load : public Module {
    private:
        std::vector<std::string> filename;
        std::string dirname;
        int Nentry;
        int Currententry;
        std::string label;

        // temporary variable to extract data from branch
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_variable;

        bool* DataStructureDefined;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string TTree_name;
    public:
        Load(const char* dirname_, const char* including_string_, const char* label_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, const char* TTree_name_) : Module(), dirname(dirname_), label(label_), DataStructureDefined(DataStructureDefined_), TTree_name(TTree_name_){
            // load file list and initialize entry counter
            load_files(dirname.c_str(), &filename, including_string_);
            Nentry = filename.size();
            Currententry = 0;

            // check data structure
            for (int i = 0; i < Nentry; i++) {
                TFile* input_file = new TFile((dirname + std::string("/") + filename.at(i)).c_str(), "read");

                // read tree
                TTree* temp_tree = (TTree*)input_file->Get(TTree_name.c_str());

                // read list of branches
                TObjArray* temp_branchList = temp_tree->GetListOfBranches();

                // read/check name of branches and their type
                if ((*DataStructureDefined) == false) {
                    for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                        const char* temp_branch_name = temp_branchList->At(j)->GetName();
                        const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();

                        variable_names_->push_back(temp_branch_name);
                        VariableTypes_->push_back(std::string(TypeName));
                    }
                    (*DataStructureDefined) = true;
                }
                else {
                    for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                        const char* temp_branch_name = temp_branchList->At(j)->GetName();
                        const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();

                        if (variable_names_->at(j) != std::string(temp_branch_name)) {
                            printf("variable name is different: %s %s\n", variable_names_->at(j).c_str(), temp_branch_name);
                            exit(1);
                        }
                        else if (VariableTypes_->at(j) != std::string(TypeName)) {
                            printf("type is different: %s %s\n", VariableTypes_->at(j).c_str(), TypeName);
                            exit(1);
                        }
                    }
                }

                input_file->Close();
                delete input_file;
            }

            // copy variable name and variable type
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);
        }
        ~Load() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes.size(); i++) {
                if (strcmp(VariableTypes.at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "string") == 0) {
                    temp_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }
        }

        int Process(std::vector<Data>* data) override {
            // read Currententry'th file. If there is not file to read, just return 1
            if (Currententry == Nentry) return 1;

            // if there is remaining data, do not extract additional one
            if (data->empty() == false) return 0;

            // read file
            TFile* input_file = new TFile((dirname + std::string("/") + filename.at(Currententry)).c_str(), "read");
            printf("%s (%d/%d)\n", ("Read " + filename.at(Currententry) + "... ").c_str(), Currententry, Nentry);

            // read tree
            TTree* temp_tree = (TTree*)input_file->Get(TTree_name.c_str());

            // set branch addresses
            for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                if (strcmp(VariableTypes.at(j).c_str(), "Double_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names.at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "Int_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names.at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "UInt_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names.at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "Float_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names.at(j).c_str(), &std::get<float>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "string") == 0) {
                    temp_tree->SetBranchAddress(variable_names.at(j).c_str(), &std::get<std::string*>(temp_variable.at(j)));
                }
            }

            // fill Data vector
            for (unsigned int j = 0; j < temp_tree->GetEntries(); j++) {
                temp_tree->GetEntry(j);

                Data temp = { temp_variable, label, filename.at(Currententry) };
                data->push_back(temp);
            }

            input_file->Close();
            delete input_file;
            Currententry++;
            return 0;
        }

        void End() override {}
    };

    class Cut : public Module {
    private:
        std::string cut_string;
        std::string replaced_expr;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        Cut(const char* cut_string_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), cut_string(cut_string_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~Cut() {}

        void Start() {
            replaced_expr = replaceVariables(cut_string, &variable_names);
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                if (result < 0.5) {
                    data->erase(iter);
                }
                else ++iter;
            }

            return 1;
        }

        void End() override {}
    };

    class PrintInformation : public Module {
        /*
        * In this module, we assume that
        * 1. candidates from the same event are in the same ROOT file
        */
    private:
        std::string print_string;
        std::vector<std::string> Event_variable_list;
        double Nevt;
        double Ncandidate;

        // temporary variable to extract event variable
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        // event variable history
        std::set<std::vector<std::variant<int, unsigned int, float, double, std::string*>>, CompareHistory> history_event_variable;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), print_string(print_string_), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), Nevt(0), Ncandidate(0){}
        ~PrintInformation() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("event variable for PrintInformation should exist.\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names.begin(), variable_names.end(), Event_variable_list.at(i)) - variable_names.begin();

                if (event_variable_index == variable_names.size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                    temp_event_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                        temp_event_variable.at(i) = std::get<std::string*>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                        exit(1);
                    }
                }

                if (history_event_variable.find(temp_event_variable) == history_event_variable.end()) {
                    history_event_variable.insert(temp_event_variable);
                    Nevt = Nevt + ObtainWeight(iter);
                }

                Ncandidate = Ncandidate + ObtainWeight(iter);
                ++iter;
            }

            // clear the vector under the assumption
            history_event_variable.clear();

            return 1;
        }

        void End() override {
            printf("%s\n", print_string.c_str());
            printf("Number of event: %lf\n", Nevt);
            printf("Number of candidate: %lf\n", Ncandidate);
        }
    };

    class DrawTH1D : public Module {
    private:
        TH1D* hist;
        std::string hist_title;
        int nbins;
        double x_low;
        double x_high;
        bool normalized;
        bool LogScale;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string expression;
        std::string replaced_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> weight;
    public:
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(false), LogScale(false), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(false), LogScale(false), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~DrawTH1D() {
            delete hist;
        }

        void Start() override {
            hist = nullptr;

            // change variable name into placeholder
            replaced_expr = replaceVariables(expression, &variable_names);

            // if range is determined, make histogram first
            if ((x_low != std::numeric_limits<double>::max()) && (x_high != std::numeric_limits<double>::max())) {
                std::string hist_name = generateRandomString(12);
                hist = new TH1D(hist_name.c_str(), hist_title.c_str(), nbins, x_low, x_high);
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);

                if (hist == nullptr) {
                    x_variable.push_back(result);
                    weight.push_back(ObtainWeight(iter));
                }
                else {
                    hist->Fill(result, ObtainWeight(iter));
                }

                // if saved variable exceed 10MB, calculate max, min and create histogram. It is to save memory
                if ((sizeof(double) * x_variable.size() > 10000000.0) && (hist == nullptr)) {
                    std::vector<double>::iterator min_it = std::min_element(x_variable.begin(), x_variable.end());
                    std::vector<double>::iterator max_it = std::max_element(x_variable.begin(), x_variable.end());

                    x_low = *min_it;
                    x_high = *max_it;
                    
                    std::string hist_name = generateRandomString(12);
                    hist = new TH1D(hist_name.c_str(), hist_title.c_str(), nbins, x_low, x_high);

                    // fill histogram
                    for (int i = 0; i < weight.size(); i++) {
                        hist->Fill(x_variable.at(i), weight.at(i));
                    }

                    x_variable.clear();
                    std::vector<double>().swap(x_variable);
                    weight.clear();
                    std::vector<double>().swap(weight);
                }

                ++iter;
            }

            return 1;
        }

        void End() override {
            // if range is not determined, determined from this side
            if ((x_low == std::numeric_limits<double>::max()) && (x_high == std::numeric_limits<double>::max())) {
                std::vector<double>::iterator min_it = std::min_element(x_variable.begin(), x_variable.end());
                std::vector<double>::iterator max_it = std::max_element(x_variable.begin(), x_variable.end());

                x_low = *min_it;
                x_high = *max_it;
            }

            // create histogram
            if (hist == nullptr) {
                std::string hist_name = generateRandomString(12);
                hist = new TH1D(hist_name.c_str(), hist_title.c_str(), nbins, x_low, x_high);
            }

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                hist->Fill(x_variable.at(i), weight.at(i));
            }

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            std::vector<double>().swap(x_variable);
            weight.clear();
            std::vector<double>().swap(weight);

            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            if (LogScale) gPad->SetLogy(1);
            else gPad->SetLogy(0);
            hist->SetStats(false);
            if (normalized) hist->Scale(1.0 / hist->Integral(), "width");
            hist->Draw("Hist");
            c_temp->SaveAs(png_name.c_str());
            delete c_temp;
        }

    };

    class DrawTH2D : public Module {
    private:
        TH2D* hist;
        std::string hist_title;
        int x_nbins;
        double x_low;
        double x_high;
        int y_nbins;
        double y_low;
        double y_high;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string x_expression;
        std::string x_replaced_expr;
        std::string y_expression;
        std::string y_replaced_expr;

        std::string png_name;
        std::string draw_option;

        std::vector<double> x_variable;
        std::vector<double> y_variable;
        std::vector<double> weight;
    public:
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, const char* draw_option_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(x_nbins_), x_low(x_low_), x_high(x_high_), y_nbins(y_nbins_), y_low(y_low_), y_high(y_high_), png_name(png_name_), draw_option(draw_option_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_, const char* draw_option_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), y_nbins(50), y_low(std::numeric_limits<double>::max()), y_high(std::numeric_limits<double>::max()), png_name(png_name_), draw_option(draw_option_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~DrawTH2D() {
            delete hist;
        }

        void Start() override {
            hist = nullptr;

            // change variable name into placeholder
            x_replaced_expr = replaceVariables(x_expression, &variable_names);
            y_replaced_expr = replaceVariables(y_expression, &variable_names);

            // if range is determined, make histogram first
            if ((x_low != std::numeric_limits<double>::max()) && (x_high != std::numeric_limits<double>::max()) && (y_low != std::numeric_limits<double>::max()) && (y_high != std::numeric_limits<double>::max())) {
                std::string hist_name = generateRandomString(12);
                hist = new TH2D(hist_name.c_str(), hist_title.c_str(), x_nbins, x_low, x_high, y_nbins, y_low, y_high);
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double x_result = evaluateExpression(x_replaced_expr, iter->variable, &VariableTypes);
                double y_result = evaluateExpression(y_replaced_expr, iter->variable, &VariableTypes);

                if (hist == nullptr) {
                    x_variable.push_back(x_result);
                    y_variable.push_back(y_result);
                    weight.push_back(ObtainWeight(iter));
                }
                else {
                    hist->Fill(x_result, y_result, ObtainWeight(iter));
                }

                // if saved variable exceed 40MB, calculate max, min and create histogram. It is to save memory
                if ((sizeof(double) * x_variable.size() > 40000000.0) && (hist == nullptr)) {
                    std::vector<double>::iterator x_min_it = std::min_element(x_variable.begin(), x_variable.end());
                    std::vector<double>::iterator x_max_it = std::max_element(x_variable.begin(), x_variable.end());
                    std::vector<double>::iterator y_min_it = std::min_element(y_variable.begin(), y_variable.end());
                    std::vector<double>::iterator y_max_it = std::max_element(y_variable.begin(), y_variable.end());

                    x_low = *x_min_it;
                    x_high = *x_max_it;
                    y_low = *y_min_it;
                    y_high = *y_max_it;

                    std::string hist_name = generateRandomString(12);
                    hist = new TH2D(hist_name.c_str(), hist_title.c_str(), x_nbins, x_low, x_high, y_nbins, y_low, y_high);

                    // fill histogram
                    for (int i = 0; i < weight.size(); i++) {
                        hist->Fill(x_variable.at(i), y_variable.at(i), weight.at(i));
                    }

                    x_variable.clear();
                    std::vector<double>().swap(x_variable);
                    y_variable.clear();
                    std::vector<double>().swap(y_variable);
                    weight.clear();
                    std::vector<double>().swap(weight);
                }

                ++iter;
            }

            return 1;
        }

        void End() override {
            // if range is not determined, determined from this side
            if ((x_low == std::numeric_limits<double>::max()) && (x_high == std::numeric_limits<double>::max()) && (y_low == std::numeric_limits<double>::max()) && (y_high == std::numeric_limits<double>::max())) {
                std::vector<double>::iterator x_min_it = std::min_element(x_variable.begin(), x_variable.end());
                std::vector<double>::iterator x_max_it = std::max_element(x_variable.begin(), x_variable.end());
                std::vector<double>::iterator y_min_it = std::min_element(y_variable.begin(), y_variable.end());
                std::vector<double>::iterator y_max_it = std::max_element(y_variable.begin(), y_variable.end());

                x_low = *x_min_it;
                x_high = *x_max_it;
                y_low = *y_min_it;
                y_high = *y_max_it;
            }

            // create histogram
            if (hist == nullptr) {
                std::string hist_name = generateRandomString(12);
                hist = new TH2D(hist_name.c_str(), hist_title.c_str(), x_nbins, x_low, x_high, y_nbins, y_low, y_high);
            }

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                hist->Fill(x_variable.at(i), y_variable.at(i), weight.at(i));
            }

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            std::vector<double>().swap(x_variable);
            y_variable.clear();
            std::vector<double>().swap(y_variable);
            weight.clear();
            std::vector<double>().swap(weight);

            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            hist->SetStats(false);
            hist->Draw(draw_option.c_str());
            c_temp->SaveAs(png_name.c_str());
            delete c_temp;
        }

    };

    class PrintSeparateRootFile : public Module {
    private:
        std::string path;
        std::string prefix;
        std::string suffix;

        // temporary variable to save data into branch
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_variable;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string TTree_name;
    public:
        PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, const char* TTree_name_) : Module(), path(path_), prefix(prefix_), suffix(suffix_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), TTree_name(TTree_name_){}

        ~PrintSeparateRootFile() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes.size(); i++) {
                if (strcmp(VariableTypes.at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "string") == 0) {
                    temp_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }
        }

        int Process(std::vector<Data>* data) override {

            std::string filename;
            std::string basename;
            std::string extension;
            TFile* temp_file = nullptr;
            TTree* temp_tree = nullptr;
            for (int i = 0; i < data->size(); i++) {

                // if filename changes
                // 1. set basename and extension again
                // 2. make ROOT file and TTree
                if (filename != data->at(i).filename) {
                    // save the previous file
                    if (temp_file != nullptr) {
                        temp_file->cd();
                        temp_tree->Write();
                        temp_file->Close();
                        delete temp_file;
                    }

                    filename = data->at(i).filename;

                    // separate basenamd and extension
                    size_t dotPos = filename.find_last_of('.');

                    if (dotPos != std::string::npos) {
                        // Split the filename into basename and extension
                        basename = filename.substr(0, dotPos);
                        extension = filename.substr(dotPos + 1);
                    }
                    else {
                        // If no dot is found, the entire filename is the basename
                        basename = filename;
                        extension = "";
                    }

                    // make ROOT file
                    temp_file = new TFile((path + "/" + prefix + basename + suffix + "." + extension).c_str(), "recreate");
                    temp_file->cd();
                    temp_tree = new TTree(TTree_name.c_str(), "");

                    // set Branch
                    for (int j = 0; j < VariableTypes.size(); j++) {
                        if (strcmp(VariableTypes.at(j).c_str(), "Double_t") == 0) {
                            temp_tree->Branch(variable_names.at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes.at(j).c_str(), "Int_t") == 0) {
                            temp_tree->Branch(variable_names.at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes.at(j).c_str(), "UInt_t") == 0) {
                            temp_tree->Branch(variable_names.at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes.at(j).c_str(), "Float_t") == 0) {
                            temp_tree->Branch(variable_names.at(j).c_str(), &std::get<float>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes.at(j).c_str(), "string") == 0) {
                            temp_tree->Branch(variable_names.at(j).c_str(), &std::get<std::string*>(temp_variable.at(j)));
                        }
                    }

                }

                temp_file->cd();
                temp_variable = data->at(i).variable;
                temp_tree->Fill();
            }

            // save branches and file
            if (temp_file != nullptr) {
                temp_file->cd();
                temp_tree->Write();
                temp_file->Close();
                delete temp_file;
            }

            return 1;
        }

        void End() override {}
    };

    class PrintRootFile : public Module {
    private:
        std::string output_name;
        TFile* temp_file = nullptr;
        TTree* temp_tree = nullptr;

        // temporary variable to save data into branch
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_variable;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string TTree_name;
    public:
        PrintRootFile(const char* output_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, const char* TTree_name_) : Module(), output_name(output_name_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), TTree_name(TTree_name_) {}

        ~PrintRootFile() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes.size(); i++) {
                if (strcmp(VariableTypes.at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(i).c_str(), "string") == 0) {
                    temp_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }

            temp_file = new TFile(output_name.c_str(), "recreate");
            temp_file->cd();
            temp_tree = new TTree(TTree_name.c_str(), "");

            // set Branch
            for (int j = 0; j < VariableTypes.size(); j++) {
                if (strcmp(VariableTypes.at(j).c_str(), "Double_t") == 0) {
                    temp_tree->Branch(variable_names.at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "Int_t") == 0) {
                    temp_tree->Branch(variable_names.at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "UInt_t") == 0) {
                    temp_tree->Branch(variable_names.at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "Float_t") == 0) {
                    temp_tree->Branch(variable_names.at(j).c_str(), &std::get<float>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes.at(j).c_str(), "string") == 0) {
                    temp_tree->Branch(variable_names.at(j).c_str(), &std::get<std::string*>(temp_variable.at(j)));
                }
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                temp_file->cd();
                temp_variable = iter->variable;
                temp_tree->Fill();
                ++iter;
            }

            return 1;
        }

        void End() override {
            // save branches and file
            if (temp_file != nullptr) {
                temp_file->cd();
                temp_tree->Write();
                temp_file->Close();
                delete temp_file;
            }
        }
    };

    class BCS : public Module {
        /*
        * In this module, we assume that 
        * 1. candidates from the same event are consecutive
        * 2. candidates from the same event are in the same ROOT file
        */
    private:
        std::string equation;
        std::string criteria;
        std::vector<std::string> Event_variable_list;

        // temporary variable to extract event variable
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        std::string replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        static char to_upper(char c) {
            return std::toupper(static_cast<unsigned char>(c));
        }
    public:
        BCS(const char* equation_, const char* criteria_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), criteria(criteria_), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        
        ~BCS() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("event variable for BCS should exist.\n");
                exit(1);
            }

            // convert `criteria` into upper case
            std::transform(criteria.begin(), criteria.end(), criteria.begin(), to_upper);

            if ((criteria != "HIGHEST") && (criteria != "LOWEST")) {
                printf("criteria for BCS should be `highest` or `lowest`\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names.begin(), variable_names.end(), Event_variable_list.at(i)) - variable_names.begin();

                if (event_variable_index == variable_names.size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                    temp_event_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }

            replaced_expr = replaceVariables(equation, &variable_names);
        }

        int Process(std::vector<Data>* data) override {

            // It is temporary data to save Data before/after BCS is done.
            std::vector<Data> temp_data;
            std::vector<Data> temp_data_after_BCS;

            // initialize extreme value/index
            double extreme_value;
            if (criteria == "HIGHEST") extreme_value = -std::numeric_limits<double>::max();
            else if (criteria == "LOWEST") extreme_value = std::numeric_limits<double>::max();
            else {
                printf("criteria for BCS should be `highest` or `lowest`\n");
                exit(1);
            }
            std::vector<int> selected_indices;

            // initialization flag previous event variable
            bool ItIsTheFirstData = true; // we erase data from std::vector<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                        temp_event_variable.at(i) = std::get<std::string*>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                        exit(1);
                    }
                }
                if (ItIsTheFirstData) {
                    previous_event_variable = temp_event_variable;
                    ItIsTheFirstData = false;
                }

                // if event variable changes, do BCS
                if (previous_event_variable != temp_event_variable) {
                    if (selected_indices.size() != 0) {
                        for (int i = 0; i < selected_indices.size(); i++) {
                            Data temp = temp_data.at(selected_indices.at(i));
                            temp_data_after_BCS.push_back(temp);
                        }

                        temp_data.clear();

                        // reset extreme value/index
                        if (criteria == "HIGHEST") extreme_value = -std::numeric_limits<double>::max();
                        else if (criteria == "LOWEST") extreme_value = std::numeric_limits<double>::max();
                        else {
                            printf("criteria for BCS should be `highest` or `lowest`\n");
                            exit(1);
                        }
                        selected_indices.clear();
                    }
                    else {
                        printf("[BCS] unexpected error");
                        exit(1);
                    }
                }

                // get BCS variable
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                
                // check the BCS criteria
                if (criteria == "HIGHEST") {
                    if (result > extreme_value) {
                        extreme_value = result;
                        selected_indices.clear();
                        selected_indices.push_back(temp_data.size());
                    }
                    else if (result == extreme_value) {
                        selected_indices.push_back(temp_data.size());
                    }
                }
                else if (criteria == "LOWEST") {
                    if (result < extreme_value) {
                        extreme_value = result;
                        selected_indices.clear();
                        selected_indices.push_back(temp_data.size());
                    }
                    else if (result == extreme_value) {
                        selected_indices.push_back(temp_data.size());
                    }
                }

                // get Data
                temp_data.push_back(*iter);
                data->erase(iter);

                previous_event_variable = temp_event_variable;

            }

            // do BCS for the final dataset
            if (selected_indices.size() != 0) {
                for (int i = 0; i < selected_indices.size(); i++) {
                    Data temp = temp_data.at(selected_indices.at(i));
                    temp_data_after_BCS.push_back(temp);
                }

                temp_data.clear();

                // reset extreme value/index
                if (criteria == "HIGHEST") extreme_value = -std::numeric_limits<double>::max();
                else if (criteria == "LOWEST") extreme_value = std::numeric_limits<double>::max();
                else {
                    printf("criteria for BCS should be `highest` or `lowest`\n");
                    exit(1);
                }
                selected_indices.clear();
            }

            // use swap instead of copy to save computing resource
            data->swap(temp_data_after_BCS);

            return 1;
        }

        void End() override {}
    };

    class RandomBCS : public Module {
        /*
        * In this module, we assume that
        * 1. candidates from the same event are consecutive
        * 2. candidates from the same event are in the same ROOT file
        */
    private:
        std::vector<std::string> Event_variable_list;

        // temporary variable to extract event variable
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        RandomBCS(const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~RandomBCS() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("event variable for RandomBCS should exist.\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names.begin(), variable_names.end(), Event_variable_list.at(i)) - variable_names.begin();

                if (event_variable_index == variable_names.size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                    temp_event_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }

        }

        int Process(std::vector<Data>* data) override {

            // Convert the string to a size_t hash value
            std::hash<std::string> hasher;
            size_t hashValue;
            if (data->size() > 0) hashValue = hasher(data->at(0).filename);
            else hashValue = 42;

            // Initialize the random number generator with the hash value
            std::mt19937 rng(static_cast<unsigned int>(hashValue));
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            // It is temporary data to save Data before/after BCS is done.
            std::vector<Data> temp_data;
            std::vector<Data> temp_data_after_BCS;

            // initialize extreme value/index
            double extreme_value = -std::numeric_limits<double>::max();
            std::vector<int> selected_indices;

            // initialization flag previous event variable
            bool ItIsTheFirstData = true; // we erase data from std::vector<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                        temp_event_variable.at(i) = std::get<std::string*>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                        exit(1);
                    }
                }
                if (ItIsTheFirstData) {
                    previous_event_variable = temp_event_variable;
                    ItIsTheFirstData = false;
                }

                // if event variable changes, do BCS
                if (previous_event_variable != temp_event_variable) {
                    if (selected_indices.size() != 0) {
                        for (int i = 0; i < selected_indices.size(); i++) {
                            Data temp = temp_data.at(selected_indices.at(i));
                            temp_data_after_BCS.push_back(temp);
                        }

                        temp_data.clear();

                        // reset extreme value/index
                        extreme_value = -std::numeric_limits<double>::max();
                        selected_indices.clear();
                    }
                    else {
                        printf("[RandomBCS] unexpected error");
                        exit(1);
                    }
                }

                // get random variable
                double result = dist(rng);

                // check the BCS criteria
                if (result > extreme_value) {
                    extreme_value = result;
                    selected_indices.clear();
                    selected_indices.push_back(temp_data.size());
                }
                else if (result == extreme_value) {
                    selected_indices.push_back(temp_data.size());
                }

                // get Data
                temp_data.push_back(*iter);
                data->erase(iter);

                previous_event_variable = temp_event_variable;

            }

            // do BCS for the final dataset
            if (selected_indices.size() != 0) {
                for (int i = 0; i < selected_indices.size(); i++) {
                    Data temp = temp_data.at(selected_indices.at(i));
                    temp_data_after_BCS.push_back(temp);
                }

                temp_data.clear();

                // reset extreme value/index
                extreme_value = -std::numeric_limits<double>::max();
                selected_indices.clear();
            }

            // use swap instead of copy to save computing resource
            data->swap(temp_data_after_BCS);

            return 1;
        }

        void End() override {}
    };

    class IsBCSValid : public Module {
        /*
        * In this module, we assume that
        * 1. candidates from the same event are in the same ROOT file
        */
    private:
        std::vector<std::string> Event_variable_list;

        // temporary variable to extract event variable
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        // event variable history
        std::set<std::vector<std::variant<int, unsigned int, float, double, std::string*>>, CompareHistory> history_event_variable;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        IsBCSValid(const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~IsBCSValid() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("event variable for IsBCSValid should exist.\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names.begin(), variable_names.end(), Event_variable_list.at(i)) - variable_names.begin();

                if (event_variable_index == variable_names.size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                    temp_event_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                        temp_event_variable.at(i) = std::get<std::string*>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                        exit(1);
                    }
                }

                if (history_event_variable.find(temp_event_variable) == history_event_variable.end()) {
                    history_event_variable.insert(temp_event_variable);
                }
                else {
                    printf("BCS is not valid\n");
                    exit(1);
                }

                ++iter;
            }

            // clear the vector under the assumption
            history_event_variable.clear();

            return 1;
        }

        void End() override {}
    };

    class DrawFOM : public Module {
    private:
        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        double* Cuts;
        double* NSIGs;
        double* NBKGs;
        double* FOMs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        std::string png_name;

        double MyEPSILON;
    public:
        DrawFOM(const char* equation_, double MIN_, double MAX_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            // just 50
            NBin = 50;

            // just 0.000001
            MyEPSILON = 0.000001;
        }
        DrawFOM(const char* equation_, double MIN_, double MAX_, int NBin_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NBin(NBin_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~DrawFOM() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceVariables(equation, &variable_names);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NBKGs[i] = 0.0;
            }
        }

        int Process(std::vector<Data>* data) {

            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;

                for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                    bool DoesItPassCriteria = false;
                    double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                    if (result > variable_value) DoesItPassCriteria = true;
                    else DoesItPassCriteria = false;

                    if (DoesItPassCriteria) {
                        if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) NSIGs[i] = NSIGs[i] + ObtainWeight(iter);
                        if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) NBKGs[i] = NBKGs[i] + ObtainWeight(iter);
                    }

                    ++iter;
                }

            }

            return 1;
        }

        void End() {

            FOMs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                if ((NSIGs[i] + NBKGs[i]) < MyEPSILON) FOMs[i] = 0.0;
                else {
                    FOMs[i] = NSIGs[i] / std::sqrt(NSIGs[i] + NBKGs[i]);
                }
            }

            double MinimumFOM = std::numeric_limits<double>::max();
            for (int i = 0; i < NBin; i++) {
                if (MinimumFOM > FOMs[i]) MinimumFOM = FOMs[i];
            }

            double MaximumFOM = -std::numeric_limits<double>::max();
            int MaximumIndex = -1;
            for (int i = 0; i < NBin; i++) {
                if (MaximumFOM < FOMs[i]) {
                    MaximumFOM = FOMs[i];
                    MaximumIndex = i;
                }
            }

            // print result
            printf("FOM scan result for %s:\n", equation.c_str());
            printf("Maximum FOM value: %lf\n", MaximumFOM);
            printf("Cut value: %lf\n", Cuts[MaximumIndex]);
            printf("NSIG: %lf\n", NSIGs[MaximumIndex]);
            printf("NBKG: %lf\n", NBKGs[MaximumIndex]);

            // draw FOM plot
            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();

            TGraph* gr3 = new TGraph(NBin, Cuts, FOMs);
            gr3->SetTitle((";" + equation + " cut; #frac{S}{#sqrt{S + B}}").c_str());
            gr3->SetMarkerStyle(0);
            gr3->SetMinimum(MinimumFOM);
            gr3->Draw("");

            c_temp->SaveAs(png_name.c_str());

            free(Cuts);
            free(NSIGs);
            free(NBKGs);
            free(FOMs);

            delete c_temp;
        }
    };

    class DrawPunziFOM : public Module {
    private:
        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        double* Cuts;
        double* NSIGs;
        double* NBKGs;
        double* FOMs;

        // vars for PunziFOM
        double NSIG_initial;
        double alpha;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        std::string png_name;

        double MyEPSILON;
    public:
        DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NSIG_initial_, double alpha_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NSIG_initial(NSIG_initial_), alpha(alpha_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            // just 50
            NBin = 50;

            // just 0.000001
            MyEPSILON = 0.000001;
        }
        DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NBin_, double NSIG_initial_, double alpha_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NBin(NBin_), NSIG_initial(NSIG_initial_), alpha(alpha_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~DrawPunziFOM() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceVariables(equation, &variable_names);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NBKGs[i] = 0.0;
            }
        }

        int Process(std::vector<Data>* data) {

            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;

                for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                    bool DoesItPassCriteria = false;
                    double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                    if (result > variable_value) DoesItPassCriteria = true;
                    else DoesItPassCriteria = false;

                    if (DoesItPassCriteria) {
                        if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) NSIGs[i] = NSIGs[i] + ObtainWeight(iter);
                        if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) NBKGs[i] = NBKGs[i] + ObtainWeight(iter);
                    }

                    ++iter;
                }

            }

            return 1;
        }

        void End() {

            FOMs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                if ((NSIGs[i] + NBKGs[i]) < MyEPSILON) FOMs[i] = 0.0;
                else {
                    FOMs[i] = (NSIGs[i] / NSIG_initial) / (alpha / 2.0 + std::sqrt(NBKGs[i]));
                }
            }

            double MinimumFOM = std::numeric_limits<double>::max();
            for (int i = 0; i < NBin; i++) {
                if (MinimumFOM > FOMs[i]) MinimumFOM = FOMs[i];
            }

            double MaximumFOM = -std::numeric_limits<double>::max();
            int MaximumIndex = -1;
            for (int i = 0; i < NBin; i++) {
                if (MaximumFOM < FOMs[i]) {
                    MaximumFOM = FOMs[i];
                    MaximumIndex = i;
                }
            }

            // print result
            printf("FOM scan result for %s:\n", equation.c_str());
            printf("Maximum FOM value: %lf\n", MaximumFOM);
            printf("Cut value: %lf\n", Cuts[MaximumIndex]);
            printf("NSIG: %lf\n", NSIGs[MaximumIndex]);
            printf("NBKG: %lf\n", NBKGs[MaximumIndex]);

            // draw FOM plot
            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();

            TGraph* gr3 = new TGraph(NBin, Cuts, FOMs);
            gr3->SetTitle((";" + equation + " cut; Punzi FOM").c_str());
            gr3->SetMarkerStyle(0);
            gr3->SetMinimum(MinimumFOM);
            gr3->Draw("");

            c_temp->SaveAs(png_name.c_str());

            free(Cuts);
            free(NSIGs);
            free(NBKGs);
            free(FOMs);

            delete c_temp;
        }
    };

    class CalculateAUC : public Module {
    private:
        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        double* Cuts;
        double* NSIGs;
        double* NBKGs;

        double NSIGs_total;
        double NBKGs_total;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        std::string output_name;
        std::string write_option;

        double MyEPSILON;
    public:
        CalculateAUC(const char* equation_, double MIN_, double MAX_, const char* output_name_, const char* write_option_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), output_name(output_name_), write_option(write_option_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            // just 100
            NBin = 100;

            NSIGs_total = 0;
            NBKGs_total = 0;
        }

        ~CalculateAUC() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceVariables(equation, &variable_names);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NBKGs[i] = 0.0;
            }

            // check write option
            if (write_option == "w") {}
            else if (write_option == "a") {}
            else {
                printf("[CalculateAUC] write option should be one of `w` or `a`\n");
                exit(1);
            }
        }

        int Process(std::vector<Data>* data) {

            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;

                for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                    bool DoesItPassCriteria = false;
                    double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                    if (result > variable_value) DoesItPassCriteria = true;
                    else DoesItPassCriteria = false;

                    if (DoesItPassCriteria) {
                        if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) NSIGs[i] = NSIGs[i] + ObtainWeight(iter);
                        if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) NBKGs[i] = NBKGs[i] + ObtainWeight(iter);
                    }

                    ++iter;
                }

            }

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) NSIGs_total = NSIGs_total + ObtainWeight(iter);
                if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) NBKGs_total = NBKGs_total + ObtainWeight(iter);

                ++iter;
            }

            return 1;
        }

        void End() {
            // get AUC
            double AUC = 0;
            for (int i = 0; i < NBin; i++) {
                if (i != (NBin - 1)) {
                    double del_FPR = (NBKGs[i] / NBKGs_total) - (NBKGs[i + 1] / NBKGs_total);
                    double avg_TPR = ((NSIGs[i + 1] / NSIGs_total) + (NSIGs[i] / NSIGs_total)) / 2.0;
                    AUC = AUC + del_FPR * avg_TPR;
                }
                else {
                    double del_FPR = (NBKGs[i] / NBKGs_total) - 0.0;
                    double avg_TPR = ((NSIGs[i] / NSIGs_total) + 0.0) / 2.0;
                    AUC = AUC + del_FPR * avg_TPR;
                }
            }

            // print AUC
            FILE* fp = fopen(output_name.c_str(), write_option.c_str());
            fprintf(fp, "%lf ", AUC);
            fclose(fp);

            free(Cuts);
            free(NSIGs);
            free(NBKGs);
        }
    };

    class DrawStack : public Module {
    private:
        THStack* stack;
        TH1D** stack_hist;
        TH1D* stack_error;
        TH1D* hist;
        TH1D* RatioorPull;
        std::string stack_title;
        int nbins;
        double x_low;
        double x_high;
        bool normalized;
        bool LogScale;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::string expression;
        std::string replaced_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> weight;
        std::vector<std::string> label;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;
        std::vector<std::string> data_label_list;
        std::vector<std::string> MC_label_list;

        std::vector<std::string> stack_label_list;
        std::vector<std::string> hist_label_list;

        /*
        * draw option:
        * 0: `SetMC` and `SetData`. stack MC and black dot data
        * 1: `SetSignal` and `SetBackground`. red line signal and stack background
        * 2: `SetMC` only. stack only
        */
        int hist_draw_option;

    public:
        DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(false), LogScale(false), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(false), LogScale(false), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~DrawStack() {
            delete stack;
            for (int i = 0; i < stack_label_list.size(); i++) delete stack_hist[i];
            free(stack_hist);
            delete stack_error;
            delete hist;
        }

        void Start() override {
            stack = nullptr;
            stack_hist = nullptr;
            stack_error = nullptr;
            hist = nullptr;
            RatioorPull = nullptr;

            // actually, the first and third else-if can be written in one line. However, I write them into the two line explicitly
            if ((data_label_list.size() != 0) && (MC_label_list.size() != 0)) {}
            else if ((Signal_label_list.size() != 0) && (Background_label_list.size() != 0)) {}
            else if ((data_label_list.size() == 0) && (MC_label_list.size() != 0)) {}
            else {
                printf("`DrawStack` requires one of them:\n");
                printf("1. `SetMC` and `SetData`\n");
                printf("2. `SetSignal` and `SetBackground`\n");
                printf("3. `SetMC` only\n");
                exit(1);
            }

            if ((data_label_list.size() != 0) && (MC_label_list.size() != 0)) {
                hist_label_list = data_label_list;
                stack_label_list = MC_label_list;
                hist_draw_option = 0;
            }
            else if ((Signal_label_list.size() != 0) && (Background_label_list.size() != 0)) {
                hist_label_list = Signal_label_list;
                stack_label_list = Background_label_list;
                hist_draw_option = 1;
            }
            else if ((data_label_list.size() == 0) && (MC_label_list.size() != 0)) {
                hist_label_list = {};
                stack_label_list = MC_label_list;
                hist_draw_option = 2;
            }
            else {
                printf("never reach\n");
                exit(1);
            }

            // change variable name into placeholder
            replaced_expr = replaceVariables(expression, &variable_names);

            if ((x_low != std::numeric_limits<double>::max()) && (x_high != std::numeric_limits<double>::max())) {
                std::string hist_name = generateRandomString(12);
                hist = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                // create histogram for stack
                stack_hist = (TH1D**)malloc(sizeof(TH1D*) * stack_label_list.size());
                for (int i = 0; i < stack_label_list.size(); i++) {
                    std::string hist_name = generateRandomString(12);
                    stack_hist[i] = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
                }
                hist_name = generateRandomString(12);
                stack_error = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                // create pull or ratio histogram
                hist_name = generateRandomString(12);
                RatioorPull = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                if ( (std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) != stack_label_list.end()) || (std::find(hist_label_list.begin(), hist_label_list.end(), iter->label) != hist_label_list.end())) {

                    if (stack_hist == nullptr) {
                        x_variable.push_back(result);
                        weight.push_back(ObtainWeight(iter));
                        label.push_back(iter->label);
                    }
                    else {
                        if (std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) != stack_label_list.end()) {
                            int label_index = std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) - stack_label_list.begin();
                            stack_hist[label_index]->Fill(result, ObtainWeight(iter));
                            stack_error->Fill(result, ObtainWeight(iter));
                        }
                        else if (std::find(hist_label_list.begin(), hist_label_list.end(), iter->label) != hist_label_list.end()) {
                            hist->Fill(result, ObtainWeight(iter));
                        }
                    }

                    // if saved variable exceed 10MB, calculate max, min and create histogram. It is to save memory
                    if ((sizeof(double) * x_variable.size() > 10000000.0) && (stack_hist == nullptr)) {
                        std::vector<double>::iterator min_it = std::min_element(x_variable.begin(), x_variable.end());
                        std::vector<double>::iterator max_it = std::max_element(x_variable.begin(), x_variable.end());

                        x_low = *min_it;
                        x_high = *max_it;

                        // create histogram
                        std::string hist_name = generateRandomString(12);
                        hist = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                        // create histogram for stack
                        stack_hist = (TH1D**)malloc(sizeof(TH1D*) * stack_label_list.size());
                        for (int i = 0; i < stack_label_list.size(); i++) {
                            std::string hist_name = generateRandomString(12);
                            stack_hist[i] = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
                        }
                        hist_name = generateRandomString(12);
                        stack_error = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                        // create pull or ratio histogram
                        hist_name = generateRandomString(12);
                        RatioorPull = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                        // fill histogram
                        for (int i = 0; i < weight.size(); i++) {
                            if (std::find(hist_label_list.begin(), hist_label_list.end(), label.at(i)) != hist_label_list.end()) {
                                hist->Fill(x_variable.at(i), weight.at(i));
                            }
                        }

                        // fill histogram for stack
                        for (int i = 0; i < weight.size(); i++) {
                            if (std::find(stack_label_list.begin(), stack_label_list.end(), label.at(i)) != stack_label_list.end()) {
                                int label_index = std::find(stack_label_list.begin(), stack_label_list.end(), label.at(i)) - stack_label_list.begin();
                                stack_hist[label_index]->Fill(x_variable.at(i), weight.at(i));
                                stack_error->Fill(x_variable.at(i), weight.at(i));
                            }
                        }

                        x_variable.clear();
                        std::vector<double>().swap(x_variable);
                        weight.clear();
                        std::vector<double>().swap(weight);
                        label.clear();
                        std::vector<std::string>().swap(label);
                    }

                }

                ++iter;
            }

            return 1;
        }

        void End() override {
            // if range is not determined, determined from this side
            if ((x_low == std::numeric_limits<double>::max()) && (x_high == std::numeric_limits<double>::max())) {
                std::vector<double>::iterator min_it = std::min_element(x_variable.begin(), x_variable.end());
                std::vector<double>::iterator max_it = std::max_element(x_variable.begin(), x_variable.end());

                x_low = *min_it;
                x_high = *max_it;
            }

            // create stack
            std::string stack_name = generateRandomString(12);
            stack = new THStack(stack_name.c_str(), stack_title.c_str());

            if (stack_hist == nullptr) {
                // create histogram
                std::string hist_name = generateRandomString(12);
                hist = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                // create histogram for stack
                stack_hist = (TH1D**)malloc(sizeof(TH1D*) * stack_label_list.size());
                for (int i = 0; i < stack_label_list.size(); i++) {
                    std::string hist_name = generateRandomString(12);
                    stack_hist[i] = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
                }
                hist_name = generateRandomString(12);
                stack_error = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);

                // create pull or ratio histogram
                hist_name = generateRandomString(12);
                RatioorPull = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
            }

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                if (std::find(hist_label_list.begin(), hist_label_list.end(), label.at(i)) != hist_label_list.end()) {
                    hist->Fill(x_variable.at(i), weight.at(i));
                }
            }

            // fill histogram for stack
            for (int i = 0; i < weight.size(); i++) {
                if (std::find(stack_label_list.begin(), stack_label_list.end(), label.at(i)) != stack_label_list.end()) {
                    int label_index = std::find(stack_label_list.begin(), stack_label_list.end(), label.at(i)) - stack_label_list.begin();
                    stack_hist[label_index]->Fill(x_variable.at(i), weight.at(i));
                    stack_error->Fill(x_variable.at(i), weight.at(i));
                }
            }

            // fill pull or ratio
            RatioorPull->SetLineColor(kBlack); RatioorPull->SetMarkerStyle(21); RatioorPull->Sumw2(); RatioorPull->SetStats(0);
            RatioorPull->Divide(hist, stack_error);

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            std::vector<double>().swap(x_variable);
            weight.clear();
            std::vector<double>().swap(weight);
            label.clear();
            std::vector<std::string>().swap(label);

            if (normalized) {
                if(hist_draw_option == 0) printf("[DrawStack] normalized option does not work when there is data\n");
                else if(hist_draw_option == 1) {
                    double sum_int = 0;
                    for (int i = 0; i < stack_label_list.size(); i++) sum_int = sum_int + stack_hist[i]->Integral();
                    for (int i = 0; i < stack_label_list.size(); i++) stack_hist[i]->Scale(1.0 / sum_int, "width");
                    stack_error->Scale(1.0 / stack_error->Integral(), "width");
                    hist->Scale(1.0 / hist->Integral(), "width");
                }
                else if(hist_draw_option == 2) {
                    double sum_int = 0;
                    for (int i = 0; i < stack_label_list.size(); i++) sum_int = sum_int + stack_hist[i]->Integral();
                    for (int i = 0; i < stack_label_list.size(); i++) stack_hist[i]->Scale(1.0 / sum_int, "width");
                    stack_error->Scale(1.0 / stack_error->Integral(), "width");
                }
            }

            // stack histogram
            for (int i = 0; i < stack_label_list.size(); i++) {
                stack->Add(stack_hist[i]);
            }

            // set palette
            gStyle->SetPalette(kPastel);

            // set maximum value of y-axis
            double ymax_1 = stack->GetMaximum();
            double ymax_2 = hist->GetMaximum();
            double real_max = 0;
            if (ymax_1 > ymax_2) real_max = ymax_1;
            else real_max = ymax_2;

            if (LogScale) stack->SetMaximum(std::pow(real_max, 1.4));
            else stack->SetMaximum(real_max * 1.4);

            if (hist_draw_option == 0) {
                // define Canvas and pad
                TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
                TPad* pad1 = new TPad("pad1", "pad1", 0.0, 0.3, 1.0, 1.0);
                pad1->SetBottomMargin(0.02); pad1->SetLeftMargin(0.15);
                pad1->SetGridx(); pad1->Draw(); pad1->cd();
                if (LogScale) pad1->SetLogy(1);
                else pad1->SetLogy(0);

                stack->Draw("pfc Hist"); stack->GetXaxis()->SetLabelSize(0.0); stack->GetXaxis()->SetTitleSize(0.0);

                stack_error->SetFillColor(12);
                stack_error->SetLineWidth(0);
                stack_error->SetFillStyle(3004);
                stack_error->Draw("e2 SAME");

                hist->SetLineWidth(2);
                hist->SetLineColor(kBlack);
                hist->SetMarkerStyle(8);
                hist->Draw("SAME eP EX0");

                // set legend
                TLegend* legend = new TLegend(0.94, 0.9, 0.44, 0.7);
                for (int i = 0; i < stack_label_list.size(); i++) {
                    legend->AddEntry(stack_hist[i], stack_label_list.at(i).c_str(), "f");
                }
                legend->AddEntry(stack_error, "MC stat error", "f");
                legend->AddEntry(hist, "data", "LP");
                legend->SetNColumns(3);

                legend->SetFillStyle(0); legend->SetLineWidth(0);
                legend->Draw();

                // write Belle text
                TPaveText* pt_belle = new TPaveText(0.13, 0.83, 0.37, 0.88, "NDC NB");
                pt_belle->SetTextSize(0.035); pt_belle->SetFillStyle(0); pt_belle->SetLineWidth(0); pt_belle->SetTextAlign(11); pt_belle->AddText("Belle II"); pt_belle->Draw();
                TPaveText* pt_lumi = new TPaveText(0.13, 0.79, 0.37, 0.81, "NDC NB");
                pt_lumi->SetTextSize(0.035); pt_lumi->SetFillStyle(0); pt_lumi->SetLineWidth(0); pt_lumi->SetTextAlign(11); pt_lumi->AddText("#int L dt = 365.4 fb^{-1}"); pt_lumi->Draw();

                // draw ratio/pull
                c_temp->cd();
                TPad* pad2 = new TPad("pad2", "pad2", 0.0, 0.0, 1, 0.3);
                pad2->SetTopMargin(0.05);
                pad2->SetBottomMargin(0.2);
                pad2->SetLeftMargin(0.15);
                pad2->SetGridx();
                pad2->Draw();
                pad2->cd();

                RatioorPull->SetMinimum(0.5); RatioorPull->SetMaximum(1.5); RatioorPull->SetLineWidth(2);
                RatioorPull->GetYaxis()->SetTitleSize(0.08); RatioorPull->GetYaxis()->SetTitleOffset(0.5); RatioorPull->GetYaxis()->SetLabelSize(0.08);
                RatioorPull->GetXaxis()->SetLabelSize(0.08); RatioorPull->GetXaxis()->SetTitleSize(0.08);
                RatioorPull->Draw("eP EX0");
                TLine* line = new TLine(RatioorPull->GetXaxis()->GetXmin(), 1.0, RatioorPull->GetXaxis()->GetXmax(), 1.0);
                line->SetLineColor(kRed);
                line->SetLineStyle(1); line->SetLineWidth(3);
                line->Draw();

                c_temp->SaveAs(png_name.c_str());
                delete c_temp;
            }
            else if (hist_draw_option == 1) {
                // define Canvas and pad
                TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
                if (LogScale) gPad->SetLogy(1);
                else gPad->SetLogy(0);

                stack->Draw("pfc Hist");

                stack_error->SetFillColor(12);
                stack_error->SetLineWidth(0);
                stack_error->SetFillStyle(3004);
                stack_error->Draw("e2 SAME");

                hist->SetLineWidth(3);
                hist->SetLineColor(2);
                hist->SetFillStyle(0);
                hist->Draw("Hist SAME");

                // set legend
                TLegend* legend = new TLegend(0.94, 0.9, 0.44, 0.7);
                for (int i = 0; i < stack_label_list.size(); i++) {
                    legend->AddEntry(stack_hist[i], stack_label_list.at(i).c_str(), "f");
                }
                legend->AddEntry(stack_error, "MC stat error", "f");
                legend->AddEntry(hist, "signal", "f");
                legend->SetNColumns(3);

                legend->SetFillStyle(0); legend->SetLineWidth(0);
                legend->Draw();

                // write Belle text
                TPaveText* pt_belle = new TPaveText(0.13, 0.83, 0.37, 0.88, "NDC NB");
                pt_belle->SetTextSize(0.035); pt_belle->SetFillStyle(0); pt_belle->SetLineWidth(0); pt_belle->SetTextAlign(11); pt_belle->AddText("Belle II"); pt_belle->Draw();
                TPaveText* pt_lumi = new TPaveText(0.13, 0.79, 0.37, 0.81, "NDC NB");
                pt_lumi->SetTextSize(0.035); pt_lumi->SetFillStyle(0); pt_lumi->SetLineWidth(0); pt_lumi->SetTextAlign(11); pt_lumi->AddText("#int L dt = 365.4 fb^{-1}"); pt_lumi->Draw();

                c_temp->SaveAs(png_name.c_str());
                delete c_temp;
            }
            else if (hist_draw_option == 2) {
                // define Canvas and pad
                TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
                if (LogScale) gPad->SetLogy(1);
                else gPad->SetLogy(0);

                stack->Draw("pfc Hist");

                stack_error->SetFillColor(12);
                stack_error->SetLineWidth(0);
                stack_error->SetFillStyle(3004);
                stack_error->Draw("e2 SAME");

                // set legend
                TLegend* legend = new TLegend(0.94, 0.9, 0.44, 0.7);
                for (int i = 0; i < stack_label_list.size(); i++) {
                    legend->AddEntry(stack_hist[i], stack_label_list.at(i).c_str(), "f");
                }
                legend->AddEntry(stack_error, "MC stat error", "f");
                legend->SetNColumns(3);

                legend->SetFillStyle(0); legend->SetLineWidth(0);
                legend->Draw();

                // write Belle text
                TPaveText* pt_belle = new TPaveText(0.13, 0.83, 0.37, 0.88, "NDC NB");
                pt_belle->SetTextSize(0.035); pt_belle->SetFillStyle(0); pt_belle->SetLineWidth(0); pt_belle->SetTextAlign(11); pt_belle->AddText("Belle II"); pt_belle->Draw();
                TPaveText* pt_lumi = new TPaveText(0.13, 0.79, 0.37, 0.81, "NDC NB");
                pt_lumi->SetTextSize(0.035); pt_lumi->SetFillStyle(0); pt_lumi->SetLineWidth(0); pt_lumi->SetTextAlign(11); pt_lumi->AddText("#int L dt = 365.4 fb^{-1}"); pt_lumi->Draw();

                c_temp->SaveAs(png_name.c_str());
                delete c_temp;
            }
            else {
                printf("never reach\n");
                exit(1);
            }

        }
    };

    class FastBDTTrain : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::string> replaced_exprs;

        std::string Signal_equation;
        std::string Signal_replaced_expr;

        std::string Background_equation;
        std::string Background_replaced_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        std::map<std::string, double> hyperparameters;

        // input variables
        std::vector<std::vector<float>> InputVariables;
        std::vector<float>* InputVariable;
        std::vector<bool> IsItSignal;
        std::vector<float> weight;

        std::string path;

        // FBDT class
        FastBDT::Classifier classifier;

    public:
        FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, const char* path_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equations(input_variables_), Signal_equation(Signal_preselection_), Background_equation(Background_preselection_), hyperparameters(hyperparameters_), path(path_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
        }

        ~FastBDTTrain() {}

        void Start() {
            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                replaced_exprs.push_back(replaceVariables(equations.at(i), &variable_names));
            }
            Signal_replaced_expr = replaceVariables(Signal_equation, &variable_names);
            Background_replaced_expr = replaceVariables(Background_equation, &variable_names);

            // set hyperparmater
            if (hyperparameters.find("NTrees") == hyperparameters.end()) hyperparameters["NTrees"] = 100;
            if (hyperparameters.find("Depth") == hyperparameters.end()) hyperparameters["Depth"] = 3;
            if (hyperparameters.find("Shrinkage") == hyperparameters.end()) hyperparameters["Shrinkage"] = 0.1;
            if (hyperparameters.find("Subsample") == hyperparameters.end()) hyperparameters["Subsample"] = 0.5;
            if (hyperparameters.find("Binning") == hyperparameters.end()) hyperparameters["Binning"] = 8;

            classifier.SetNTrees(static_cast<unsigned int>(hyperparameters["NTrees"]));
            classifier.SetDepth(static_cast<unsigned int>(hyperparameters["Depth"]));
            classifier.SetShrinkage(static_cast<double>(hyperparameters["Shrinkage"]));
            classifier.SetSubsample(static_cast<double>(hyperparameters["Subsample"]));
            std::vector<unsigned int> binning(replaced_exprs.size(), static_cast<unsigned int>(hyperparameters["Binning"]));
            classifier.SetBinning(binning);

            // malloc input variables
            InputVariable = new std::vector<float>[replaced_exprs.size()];
        }

        int Process(std::vector<Data>* data) {

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                // care about preselection first
                double preselection_result = -1;

                if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) {
                    if (Signal_replaced_expr == "") preselection_result = 1;
                    else {
                        preselection_result = evaluateExpression(Signal_replaced_expr, iter->variable, &VariableTypes);
                    }
                }
                else if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) {
                    if (Background_replaced_expr == "") preselection_result = 1;
                    else {
                        preselection_result = evaluateExpression(Background_replaced_expr, iter->variable, &VariableTypes);
                    }
                }
                else {
                    preselection_result = -1; // label is not registered. Do not use this data
                }

                if (preselection_result > 0.5) { // put input variables
                    for (int i = 0; i < replaced_exprs.size(); i++) {
                        double result = evaluateExpression(replaced_exprs.at(i), iter->variable, &VariableTypes);
                        InputVariable[i].push_back(result);
                    }

                    // put answer
                    if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) IsItSignal.push_back(true);
                    else if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) IsItSignal.push_back(false);

                    // put weight
                    weight.push_back(static_cast<float>(ObtainWeight(iter)));
                }

                ++iter;
            }

            return 1;
        }

        void End() {
            // fill
            for (int i = 0; i < replaced_exprs.size(); i++) {
                InputVariables.push_back(InputVariable[i]);
            }

            // fit
            classifier.fit(InputVariables, IsItSignal, weight);

            // free memory
            delete[] InputVariable;

            // save model
            std::fstream out_stream((path + "/" + std::to_string(hyperparameters["NTrees"]) + "_" + std::to_string(hyperparameters["Depth"]) + "_" + std::to_string(hyperparameters["Shrinkage"]) + "_" + std::to_string(hyperparameters["Subsample"]) + "_" + std::to_string(hyperparameters["Binning"]) + ".weightfile").c_str(), std::ios_base::out | std::ios_base::trunc);
            out_stream << classifier << std::endl;
            out_stream.close();
        }
    };

    class FastBDTApplication : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::string> replaced_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        // FBDT class
        std::string classifier_path;
        FastBDT::Classifier classifier;

        std::string branch_name;

    public:
        FastBDTApplication(std::vector<std::string> input_variables_, const char* classifier_path_, const char* branch_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equations(input_variables_), classifier_path(classifier_path_), branch_name(branch_name_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                replaced_exprs.push_back(replaceVariables(equations.at(i), variable_names_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), branch_name) != variable_names_->end()) {
                printf("[FastBDTApplication] there is already %s variable\n", branch_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(branch_name);
            VariableTypes_->push_back("Float_t");
        }

        ~FastBDTApplication() {}

        void Start() {

            // load FBDT
            std::fstream in_stream(classifier_path.c_str(), std::ios_base::in);
            classifier = FastBDT::Classifier(in_stream);

        }

        int Process(std::vector<Data>* data) {

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                std::vector<float> inputs;
                for (int i = 0; i < replaced_exprs.size(); i++) {
                    double result = evaluateExpression(replaced_exprs.at(i), iter->variable, &VariableTypes);
                    inputs.push_back(result);
                }

                float Output_FBDT = classifier.predict(inputs);
                iter->variable.push_back(static_cast<float>(Output_FBDT));

                ++iter;
            }

            return 1;
        }

        void End() {

        }
    };

    class RandomEventSelection : public Module {
        /*
        * In this module, we assume that
        * 1. candidates from the same event are consecutive
        * 2. candidates from the same event are in the same ROOT file
        * 
        * you can use this module when you want to split the Ntuple.
        * NOTE: It is NOT random BCS
        */
    private:
        std::vector<std::string> Event_variable_list;

        // temporary variable to extract event variable
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        // the number of split and which one do you want to select?
        int split_num;
        int selected_index;

    public:
        RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), split_num(split_num_), selected_index(selected_index_), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}

        ~RandomEventSelection() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("[RandomSplit] event variable should exist.\n");
                exit(1);
            }

            if (split_num % 2 != 0) {
                printf("[RandomSplit] split_num should be even number\n");
                exit(1);
            }

            if (split_num <= 0) {
                printf("[RandomSplit] split_num should be large than 0\n");
                exit(1);
            }

            if ((selected_index >= split_num) || (selected_index < 0)) {
                printf("[RandomSplit] selected_index_ should be within [0, split_num_ - 1]\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names.begin(), variable_names.end(), Event_variable_list.at(i)) - variable_names.begin();

                if (event_variable_index == variable_names.size()) {
                    printf("[RandomSplit] cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                    temp_event_variable.push_back(static_cast<std::string*>(nullptr));
                }
                else {
                    printf("[RandomSplit] unexpected data type: %s\n", VariableTypes.at(i).c_str());
                    exit(1);
                }
            }


        }

        int Process(std::vector<Data>* data) override {

            // Convert the string to a size_t hash value
            std::hash<std::string> hasher;
            size_t hashValue;
            if (data->size() > 0) hashValue = hasher(data->at(0).filename);
            else hashValue = 42;

            // Initialize the random number generator with the hash value
            std::mt19937 rng(static_cast<unsigned int>(hashValue));
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            // It is temporary data to save Data before/after selection is done.
            std::vector<Data> temp_data;
            std::vector<Data> temp_data_after_selection;

            // initialization flag previous event variable
            bool ItIsTheFirstData = true; // we erase data from std::vector<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes.at(event_variable_index).c_str(), "string") == 0) {
                        temp_event_variable.at(i) = std::get<std::string*>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes.at(i).c_str());
                        exit(1);
                    }
                }
                if (ItIsTheFirstData) {
                    previous_event_variable = temp_event_variable;
                    ItIsTheFirstData = false;
                }

                // if event variable changes, do random selection
                if (previous_event_variable != temp_event_variable) {
                    double MIN_threshold = (1.0 / split_num) * selected_index;
                    double MAX_threshold = (1.0 / split_num) * (selected_index + 1.0);
                    double random_number = dist(rng);

                    if ((random_number > MIN_threshold) && (random_number <= MAX_threshold)) {
                        for (int i = 0; i < temp_data.size(); i++) {
                            Data temp = temp_data.at(i);
                            temp_data_after_selection.push_back(temp);
                        }

                    }
                    temp_data.clear();
                }

                // get Data
                temp_data.push_back(*iter);
                data->erase(iter);

                previous_event_variable = temp_event_variable;

            }

            // do random selection for the final dataset
            double MIN_threshold = (1.0 / split_num) * selected_index;
            double MAX_threshold = (1.0 / split_num) * (selected_index + 1.0);
            double random_number = dist(rng);

            if ((random_number > MIN_threshold) && (random_number <= MAX_threshold)) {
                for (int i = 0; i < temp_data.size(); i++) {
                    Data temp = temp_data.at(i);
                    temp_data_after_selection.push_back(temp);
                }

            }
            temp_data.clear();

            // use swap instead of copy to save computing resource
            data->swap(temp_data_after_selection);

            return 1;
        }

        void End() override {}
    };

    class DefineNewVariable : public Module {
    private:
        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        // FBDT class
        std::string new_variable_name;

    public:
        DefineNewVariable(const char* equation_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), new_variable_name(new_variable_name_) {
            // change variable name into placeholder
            replaced_expr = replaceVariables(equation, variable_names_);

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[DefineNewVariable] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~DefineNewVariable() {}

        void Start() {

        }

        int Process(std::vector<Data>* data) {

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);

                iter->variable.push_back(static_cast<double>(result));

                ++iter;
            }

            return 1;
        }

        void End() {

        }
    };

    class ConditionalPairDefineNewVariable : public Module {
    private:
        std::map<std::string, std::string> condition_equation__criteria_equation_list;
        std::map<std::string, std::string> condition_replaced_expr__criteria_replaced_expr_list;

        int condition_order; // start from 0. 0 means highest

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

        // FBDT class
        std::string new_variable_name;

    public:
        ConditionalPairDefineNewVariable(std::map<std::string, std::string> condition_equation__criteria_equation_list_, int condition_order_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), condition_equation__criteria_equation_list(condition_equation__criteria_equation_list_), condition_order(condition_order_), new_variable_name(new_variable_name_) {
            // change variable name into placeholder
            for (std::map<std::string, std::string>::iterator iter_eq = condition_equation__criteria_equation_list.begin(); iter_eq != condition_equation__criteria_equation_list.end(); ++iter_eq) {
                std::string condition_replaced_expr = replaceVariables(iter_eq->first, variable_names_);
                std::string criteria_replaced_expr = replaceVariables(iter_eq->second, variable_names_);

                condition_replaced_expr__criteria_replaced_expr_list.insert(std::make_pair(condition_replaced_expr, criteria_replaced_expr));
            }

            // check `condition_order` is valid
            if (condition_order >= condition_equation__criteria_equation_list.size()) {
                printf("[ConditionalPairDefineNewVariable] condition order (%d) should be smaller than size of condition_equation__criteria_equation_list (%d)\n", condition_order, condition_equation__criteria_equation_list.size());
                exit(1);
            }
            if (condition_order < 0) {
                printf("[ConditionalPairDefineNewVariable] condition order (%d) should be larger or equal to 0.\n", condition_order);
                exit(1);
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[ConditionalPairDefineNewVariable] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~ConditionalPairDefineNewVariable() {}

        void Start() {

        }

        int Process(std::vector<Data>* data) {

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double condition_result = -1;
                std::vector<double> condition_results;
                double criteria_result = std::numeric_limits<double>::max();
                std::vector<std::string> criteria_replaced_exprs;

                for (std::map<std::string, std::string>::iterator iter_eq = condition_replaced_expr__criteria_replaced_expr_list.begin(); iter_eq != condition_replaced_expr__criteria_replaced_expr_list.end(); ++iter_eq) {
                    double temp_ = evaluateExpression(iter_eq->first, iter->variable, &VariableTypes);
                    condition_results.push_back(temp_);
                    criteria_replaced_exprs.push_back(iter_eq->second);
                }

                std::vector<double> temp_condition_results = condition_results;
                std::nth_element(temp_condition_results.begin(), temp_condition_results.begin() + condition_order, temp_condition_results.end(), std::greater<double>());

                // The n-th largest value
                condition_result = temp_condition_results.at(condition_order);

                // Find the original index of the n-th largest value
                std::vector<double>::iterator iter_condition_results = std::find(condition_results.begin(), condition_results.end(), condition_result);
                std::size_t index = std::distance(condition_results.begin(), iter_condition_results);

                criteria_result = evaluateExpression(criteria_replaced_exprs.at(index), iter->variable, &VariableTypes);

                iter->variable.push_back(static_cast<double>(criteria_result));

                ++iter;
            }

            return 1;
        }

        void End() {

        }
    };

    class FillDataSet : public Module {
        /*
        * This module is used to fill RooDataSet
        */
    private:

        RooDataSet* dataset;
        std::vector<RooRealVar*> realvars;

        std::vector<std::string> equations;
        std::vector<std::string> replaced_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillDataSet(RooDataSet* dataset_, std::vector<RooRealVar*> realvars_, std::vector<std::string> equations_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), dataset(dataset_), realvars(realvars_), equations(equations_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillDataSet() {}
        void Start() {
            for (int i = 0; i < equations.size(); i++) {
                std::string equation = equations.at(i);
                std::string replaced_expr = replaceVariables(equation, &variable_names);
                replaced_exprs.push_back(replaced_expr);
            }

        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                for (int i = 0; i < replaced_exprs.size(); i++) {
                    std::string replaced_expr = replaced_exprs.at(i);
                    double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);
                    *(realvars.at(i)) = result;
                }

                RooArgSet temp_;
                for (int i = 0; i < replaced_exprs.size(); i++) temp_.add(*(realvars.at(i)));

                dataset->add(temp_, ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

    class FillTProfile : public Module {
        /*
        * This module is used to fill RooDataSet
        */
    private:

        RooDataSet* dataset;
        std::vector<RooRealVar*> realvars;

        TProfile* tprofile;

        std::string equation_x;
        std::string replaced_expr_x;

        std::string equation_y;
        std::string replaced_expr_y;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillTProfile(TProfile* tprofile_, std::string equation_x_, std::string equation_y_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), tprofile(tprofile_), equation_x(equation_x_), equation_y(equation_y_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillTProfile() {}
        void Start() {
            replaced_expr_x = replaceVariables(equation_x, &variable_names);
            replaced_expr_y = replaceVariables(equation_y, &variable_names);
        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result_x = evaluateExpression(replaced_expr_x, iter->variable, &VariableTypes);
                double result_y = evaluateExpression(replaced_expr_y, iter->variable, &VariableTypes);

                tprofile->Fill(result_x, result_y, ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

    class FillTH1D : public Module {
        /*
        * This module is used to fill TH1D
        */
    private:

        TH1D* th1d;

        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillTH1D(TH1D* th1d_, std::string equation_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), th1d(th1d_), equation(equation_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillTH1D() {}
        void Start() {
            replaced_expr = replaceVariables(equation, &variable_names);
        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);

                th1d->Fill(result, ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

    class FillCustomizedTH1D : public Module {
        /*
        * This module is used to fill TH1D with customized function
        */
    private:

        TH1D* th1d;
        double (*custom_function)(double);

        std::string equation;
        std::string replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillCustomizedTH1D(TH1D* th1d_, std::string equation_, double (*custom_function_)(double), std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), th1d(th1d_), equation(equation_), custom_function(custom_function_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillCustomizedTH1D() {}
        void Start() {
            replaced_expr = replaceVariables(equation, &variable_names);
        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, &VariableTypes);

                th1d->Fill(custom_function(result), ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

    class FillTH2D : public Module {
        /*
        * This module is used to fill TH2D
        */
    private:

        TH2D* th2d;

        std::string x_expression;
        std::string x_replaced_expr;
        std::string y_expression;
        std::string y_replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillTH2D(TH2D* th2d_, const char* x_expression_, const char* y_expression_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), th2d(th2d_), x_expression(x_expression_), y_expression(y_expression_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillTH2D() {}
        void Start() {
            x_replaced_expr = replaceVariables(x_expression, &variable_names);
            y_replaced_expr = replaceVariables(y_expression, &variable_names);
        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double x_result = evaluateExpression(x_replaced_expr, iter->variable, &VariableTypes);
                double y_result = evaluateExpression(y_replaced_expr, iter->variable, &VariableTypes);

                th2d->Fill(x_result, y_result, ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

    class FillCustomizedTH2D : public Module {
        /*
        * This module is used to fill TH2D with customized function
        */
    private:

        TH2D* th2d;
        double (*x_custom_function)(double, double);
        double (*y_custom_function)(double, double);

        std::string x_expression;
        std::string x_replaced_expr;
        std::string y_expression;
        std::string y_replaced_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

    public:
        FillCustomizedTH2D(TH2D* th2d_, const char* x_expression_, const char* y_expression_, double (*x_custom_function_)(double, double), double (*y_custom_function_)(double, double), std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), th2d(th2d_), x_expression(x_expression_), y_expression(y_expression_), x_custom_function(x_custom_function_), y_custom_function(y_custom_function_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {}
        ~FillCustomizedTH2D() {}
        void Start() {
            x_replaced_expr = replaceVariables(x_expression, &variable_names);
            y_replaced_expr = replaceVariables(y_expression, &variable_names);
        }
        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double x_result = evaluateExpression(x_replaced_expr, iter->variable, &VariableTypes);
                double y_result = evaluateExpression(y_replaced_expr, iter->variable, &VariableTypes);

                th2d->Fill(x_custom_function(x_result, y_result), y_custom_function(x_result, y_result), ObtainWeight(iter));

                ++iter;
            }
            return 1;
        }
        void End() override {}
    };

}

#endif 

