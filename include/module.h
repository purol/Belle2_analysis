#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <deque>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <fstream>
#include <memory>
#include <utility>
#include <optional>
#include <cctype>

#include "data.h"
#include "string_equation.h"
#include "base.h"
#include "eventweight.h"
#include "fit_manager.h"

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

// to remove duplicate elements in std::vector
void removeDuplicates(std::vector<std::string>& labels)
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> unique_labels;

    for (const std::string& label : labels) {
        if (seen.insert(label).second) {
            unique_labels.push_back(label);
        }
    }

    labels = std::move(unique_labels);
}

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

namespace Module {

    class Module {
    public:
        /*
        * design philosophy:
        * 1. data structure should be modified in constructor. Do not touch data structure in `start`, `process`, and `End` function.
        * 2. eventweight should be modified in constructor. Do not touch data structure in `start`, `process`, and `End` function.
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
        virtual int Process(std::deque<Data>* data) = 0;
        /*
        * `End` function is called after all ROOT files are read. It is called only once.
        */
        virtual void End() = 0;
        /*
        * `RequiredVariables` returns the name of variables needed.
        * It returns `std::nullopt` when all variables are needed.
        * This function is used for the memory optimization.
        */
        virtual std::optional<std::set<std::string>> RequiredVariables() const { return std::nullopt; }

        /*
        * If it returns false, the module does not wait the upstream modules
        */
        virtual bool BlocksDownstream() const { return false; }
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string TTree_name;
    public:
        Load(const char* dirname_, const char* including_string_, const char* label_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, const char* TTree_name_) : Module(), dirname(dirname_), label(label_), DataStructureDefined(DataStructureDefined_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), TTree_name(TTree_name_){
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
        ~Load() {
            for (std::size_t i = 0; i < temp_variable.size(); i++) {
                if (VariableTypes.at(i) == "string") delete std::get<std::string*>(temp_variable.at(i));
            }
        }

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

        int Process(std::deque<Data>* data) override {
            // read Currententry'th file. If there is no file to read, just return 1
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

                Data temp;

                // assing 50 more slots to avoid vector memory spike
                temp.variable.reserve(VariableTypes.size() + 50);

                // copy from temp_variable
                for (std::size_t i = 0; i < temp_variable.size(); i++) {
                    if (VariableTypes.at(i) == "string") {
                        std::string* root_string = std::get<std::string*>(temp_variable.at(i));
                        if (root_string != nullptr) temp.PushString(*root_string);
                        else temp.PushString("");
                    }
                    else temp.variable.push_back(temp_variable.at(i));
                }
                temp.label = label;
                temp.filename = filename.at(Currententry);

                // use std::move to avoid copy
                data->push_back(std::move(temp));
            }

            input_file->Close();
            delete input_file;
            Currententry++;
            return 0;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class LoadWithCut : public Module {
    private:
        std::vector<std::string> filename;
        std::string dirname;
        int Nentry;
        int Currententry;
        std::string label;

        // temporary variable to extract data from branch
        std::vector<std::variant<int, unsigned int, float, double, std::string*>> temp_variable;

        std::string cut_string;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        bool* DataStructureDefined;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string TTree_name;
    public:
        LoadWithCut(const char* dirname_, const char* including_string_, const char* label_, const char* cut_string_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, const char* TTree_name_) : Module(), dirname(dirname_), label(label_), cut_string(cut_string_), DataStructureDefined(DataStructureDefined_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), TTree_name(TTree_name_) {
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
            eventweights = (*eventweights_);
            variable_indices_list = (*variable_indices_list_);
        }
        ~LoadWithCut() {
            for (std::size_t i = 0; i < temp_variable.size(); i++) {
                if (VariableTypes.at(i) == "string") delete std::get<std::string*>(temp_variable.at(i));
            }
        }

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

            replaced_expr = replaceInternalValues(cut_string, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);
        }

        int Process(std::deque<Data>* data) override {
            // read Currententry'th file. If there is no file to read, just return 1
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

                double result = EvaluatePostfixExpression(postfix_expr, temp_variable, &VariableTypes);

                if (result > 0.5) {
                    Data temp;

                    // assing 50 more slots to avoid vector memory spike
                    temp.variable.reserve(VariableTypes.size() + 50);

                    // copy from temp_variable
                    for (std::size_t i = 0; i < temp_variable.size(); i++) {
                        if (VariableTypes.at(i) == "string") {
                            std::string* root_string = std::get<std::string*>(temp_variable.at(i));
                            if (root_string != nullptr) temp.PushString(*root_string);
                            else temp.PushString("");
                        }
                        else temp.variable.push_back(temp_variable.at(i));
                    }
                    temp.label = label;
                    temp.filename = filename.at(Currententry);

                    // use std::move to avoid copy
                    data->push_back(std::move(temp));
                }
            }

            input_file->Close();
            delete input_file;
            Currententry++;
            return 0;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class Cut : public Module {
    private:
        std::string cut_string;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        Cut(const char* cut_string_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), cut_string(cut_string_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~Cut() {}

        void Start() {
            replaced_expr = replaceInternalValues(cut_string, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);
        }

        int Process(std::deque<Data>* data) override {
            // Use std::stable_partition. 
            // It reorders 'data' so that elements where the lambda returns 'true' come first.
            // It returns an iterator to the first 'bad' element.
            std::deque<Data>::iterator it_end_of_good = std::stable_partition(data->begin(), data->end(),
                [&](const Data& d) {
                    // Return true to KEEP the event
                    double result = EvaluatePostfixExpression(postfix_expr, d.variable, &VariableTypes);
                    return result > 0.5;
                }
            );

            // Erase everything from the first bad element to the end
            data->erase(it_end_of_good, data->end());

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return GetVariablesFromExpression(cut_string, variable_names);
        }
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

        std::shared_ptr<std::vector<double>> output_handle;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        PrintInformation(const char* print_string_, const std::vector<std::string> Event_variable_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), print_string(print_string_), Event_variable_list(Event_variable_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), Nevt(0), Ncandidate(0){}
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

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

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
                    Nevt = Nevt + totalweight;
                }

                Ncandidate = Ncandidate + totalweight;
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

            output_handle->clear();

            output_handle->push_back(Nevt);
            output_handle->push_back(Ncandidate);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(Event_variable_list, variable_names));

            for (const std::vector<std::size_t> variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string expression;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> weight;
    public:
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), hist_title(hist_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(false), LogScale(false), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), hist_title(hist_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), hist_title(hist_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(false), LogScale(false), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), hist_title(hist_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

        ~DrawTH1D() {
            delete hist;
        }

        void Start() override {
            hist = nullptr;

            // change variable name into placeholder
            replaced_expr = replaceInternalValues(expression, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);

            // if range is determined, make histogram first
            if ((x_low != std::numeric_limits<double>::max()) && (x_high != std::numeric_limits<double>::max())) {
                std::string hist_name = generateRandomString(12);
                hist = new TH1D(hist_name.c_str(), hist_title.c_str(), nbins, x_low, x_high);
            }
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                if (hist == nullptr) {
                    x_variable.push_back(result);
                    weight.push_back(totalweight);
                }
                else {
                    hist->Fill(result, totalweight);
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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(expression, variable_names));

            for (const std::vector<std::size_t> variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string x_expression;
        std::string x_replaced_expr;
        std::vector<Token> x_postfix_expr;
        std::string y_expression;
        std::string y_replaced_expr;
        std::vector<Token> y_postfix_expr;

        std::string png_name;
        std::string draw_option;

        std::vector<double> x_variable;
        std::vector<double> y_variable;
        std::vector<double> weight;
    public:
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, const char* draw_option_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(x_nbins_), x_low(x_low_), x_high(x_high_), y_nbins(y_nbins_), y_low(y_low_), y_high(y_high_), png_name(png_name_), draw_option(draw_option_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_, const char* draw_option_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), y_nbins(50), y_low(std::numeric_limits<double>::max()), y_high(std::numeric_limits<double>::max()), png_name(png_name_), draw_option(draw_option_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

        ~DrawTH2D() {
            delete hist;
        }

        void Start() override {
            hist = nullptr;

            // change variable name into placeholder
            x_replaced_expr = replaceInternalValues(x_expression, internal_value);
            y_replaced_expr = replaceInternalValues(y_expression, internal_value);
            x_replaced_expr = replaceVariables(x_replaced_expr, &variable_names);
            y_replaced_expr = replaceVariables(y_replaced_expr, &variable_names);
            x_postfix_expr = PostfixExpression(x_replaced_expr, &VariableTypes);
            y_postfix_expr = PostfixExpression(y_replaced_expr, &VariableTypes);

            // if range is determined, make histogram first
            if ((x_low != std::numeric_limits<double>::max()) && (x_high != std::numeric_limits<double>::max()) && (y_low != std::numeric_limits<double>::max()) && (y_high != std::numeric_limits<double>::max())) {
                std::string hist_name = generateRandomString(12);
                hist = new TH2D(hist_name.c_str(), hist_title.c_str(), x_nbins, x_low, x_high, y_nbins, y_low, y_high);
            }
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double x_result = EvaluatePostfixExpression(x_postfix_expr, iter->variable, &VariableTypes);
                double y_result = EvaluatePostfixExpression(y_postfix_expr, iter->variable, &VariableTypes);

                if (hist == nullptr) {
                    x_variable.push_back(x_result);
                    y_variable.push_back(y_result);
                    weight.push_back(totalweight);
                }
                else {
                    hist->Fill(x_result, y_result, totalweight);
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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(x_expression, variable_names));
            result.merge(GetVariablesFromExpression(y_expression, variable_names));

            for (const std::vector<std::size_t> variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string TTree_name;
    public:
        PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, const char* TTree_name_) : Module(), path(path_), prefix(prefix_), suffix(suffix_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), TTree_name(TTree_name_){}

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

        int Process(std::deque<Data>* data) override {

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

                if (temp_variable.size() != data->at(i).variable.size()) {
                    printf("Error: [PrintSeparateRootFile] size mismatch!\n");
                    exit(1);
                }
                for (size_t j = 0; j < temp_variable.size(); j++) temp_variable.at(j) = data->at(i).variable.at(j);

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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::nullopt;
        }
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string TTree_name;
    public:
        PrintRootFile(const char* output_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, const char* TTree_name_) : Module(), output_name(output_name_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), TTree_name(TTree_name_) {}

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

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                temp_file->cd();
                if (temp_variable.size() != iter->variable.size()) {
                    printf("Error: [PrintRootFile] size mismatch!\n");
                    exit(1);
                }
                for (size_t j = 0; j < temp_variable.size(); j++) temp_variable.at(j) = iter->variable.at(j);
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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::nullopt;
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
        std::vector<Token> postfix_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        static char to_upper(char c) {
            return std::toupper(static_cast<unsigned char>(c));
        }
    public:
        BCS(const char* equation_, const char* criteria_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), criteria(criteria_), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        
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

            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);
        }

        int Process(std::deque<Data>* data) override {

            // It is temporary data to save Data before/after BCS is done.
            std::deque<Data> temp_data;
            std::deque<Data> temp_data_after_BCS;

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
            bool ItIsTheFirstData = true; // we erase data from std::deque<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); iter++) {
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
                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);
                
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
            data->clear();
            data->swap(temp_data_after_BCS);

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation, variable_names));
            result.merge(GetVariablesFromExpressions(Event_variable_list, variable_names));

            return result;
        }
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        RandomBCS(const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

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

        int Process(std::deque<Data>* data) override {

            // Convert the string to a size_t hash value
            std::hash<std::string> hasher;
            size_t hashValue;
            if (data->size() > 0) hashValue = hasher(data->at(0).filename);
            else hashValue = 42;

            // Initialize the random number generator with the hash value
            std::mt19937 rng(static_cast<unsigned int>(hashValue));
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            // It is temporary data to save Data before/after BCS is done.
            std::deque<Data> temp_data;
            std::deque<Data> temp_data_after_BCS;

            // initialize extreme value/index
            double extreme_value = -std::numeric_limits<double>::max();
            std::vector<int> selected_indices;

            // initialization flag previous event variable
            bool ItIsTheFirstData = true; // we erase data from std::deque<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); iter++) {
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
            data->clear();
            data->swap(temp_data_after_BCS);

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(Event_variable_list, variable_names));

            return result;
        }
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        IsBCSValid(const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

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

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(Event_variable_list, variable_names));

            return result;
        }
    };

    class DrawFOM : public Module {
    private:
        std::string equation;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // For the O(1) look-up
        std::unordered_set<std::string> Signal_label_set;
        std::unordered_set<std::string> Background_label_set;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        /*
        * print point when FOM value is `rank`'th, counting from maximum point to negative direction
        * this option is useful if you do not want to highly optimize the result
        */
        int rank;

        double* Cuts;
        double* NSIGs;
        double* NSIGs_cumulative;
        double* NBKGs;
        double* NBKGs_cumulative;
        double* FOMs;

        std::shared_ptr<std::vector<double>> output_handle;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string png_name;

        double MyEPSILON;
    public:
        DrawFOM(const char* equation_, double MIN_, double MAX_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), rank(0), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 50
            NBin = 50;

            // just 0.000001
            MyEPSILON = 0.000001;
        }
        DrawFOM(const char* equation_, double MIN_, double MAX_, int NBin_, int rank_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NBin(NBin_), rank(rank_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~DrawFOM() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // Convert from vector to set
            Signal_label_set.clear();
            Signal_label_set.insert(Signal_label_list.begin(), Signal_label_list.end());
            Background_label_set.clear();
            Background_label_set.insert(Background_label_list.begin(), Background_label_list.end());

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NSIGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NSIGs_cumulative[i] = 0.0;
                NBKGs[i] = 0.0;
                NBKGs_cumulative[i] = 0.0;
            }

            // initialize cuts
            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;
            }

            // check `rank` variable
            if ((rank < 0) || (rank > (NBin - 1))) {
                printf("rank should be within [%d, %d]; current: %d\n", 0, NBin - 1, rank);
                exit(1);
            }
        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                int first_bin = -1;
                if (result < MIN) first_bin = -1;
                else if (result >= MAX) first_bin = NBin - 1;
                else first_bin = std::min(NBin - 1, int(std::floor((result - MIN) / ((MAX - MIN) / NBin))));
                if (first_bin >= 0) {
                    if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[first_bin] = NSIGs[first_bin] + totalweight;
                    if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[first_bin] = NBKGs[first_bin] + totalweight;
                }

                ++iter;
            }

            return 1;
        }

        void End() {

            // calculate cumulative sum
            for (int i = NBin - 1; i >= 0; i--) {
                if (i == (NBin - 1)) {
                    NSIGs_cumulative[i] = NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs[i];
                }
                else {
                    NSIGs_cumulative[i] = NSIGs_cumulative[i + 1] + NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs_cumulative[i + 1] + NBKGs[i];
                }
            }

            FOMs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                if ((NSIGs_cumulative[i] + NBKGs_cumulative[i]) < MyEPSILON) FOMs[i] = 0.0;
                else {
                    FOMs[i] = NSIGs_cumulative[i] / std::sqrt(NSIGs_cumulative[i] + NBKGs_cumulative[i]);
                }
            }


            // Store FOMs with their corresponding indices
            std::vector<std::pair<double, int>> FOM_with_index;
            for (int i = 0; i < NBin; ++i) {
                FOM_with_index.push_back(std::make_pair(FOMs[i], i));
            }

            // sort
            std::sort(FOM_with_index.begin(), FOM_with_index.end(),
                [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
                    return a.first > b.first;
                });

            // get maximum index, and minimum FOM
            int MaximumIndex = FOM_with_index[0].second;
            double MinimumFOM = FOM_with_index[FOM_with_index.size() - 1].first;

            // remove the data whose FOM cut is higher than the maximized point
            FOM_with_index.erase(
                std::remove_if(FOM_with_index.begin(), FOM_with_index.end(),
                    [MaximumIndex](const std::pair<double, int>& p) {
                        return p.second > MaximumIndex;
                    }),
                FOM_with_index.end()
            );

            // If `rank` is too high or maximized point is close to 0, this can happend
            if (rank >= (int)FOM_with_index.size()) {
                printf("You try to find too far from maximized point. Just smallest value is set.\n");
                rank = FOM_with_index.size() - 1;
            }

            int OptimizedIndex = FOM_with_index[rank].second;
            double OptimizedFOM = FOM_with_index[rank].first;

            // print result
            printf("FOM scan result for %s:\n", equation.c_str());
            printf("try to find %d-th highest value\n", rank);
            printf("Optimized FOM value: %lf\n", OptimizedFOM);
            printf("Cut value: %lf\n", Cuts[OptimizedIndex]);
            printf("NSIG: %lf\n", NSIGs_cumulative[OptimizedIndex]);
            printf("NBKG: %lf\n", NBKGs_cumulative[OptimizedIndex]);

            output_handle->clear();

            output_handle->push_back((double)rank);
            output_handle->push_back(OptimizedFOM);
            output_handle->push_back(Cuts[OptimizedIndex]);
            output_handle->push_back(NSIGs_cumulative[OptimizedIndex]);
            output_handle->push_back(NBKGs_cumulative[OptimizedIndex]);

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
            free(NSIGs_cumulative);
            free(NBKGs);
            free(NBKGs_cumulative);
            free(FOMs);

            delete c_temp;
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class DrawPunziFOM : public Module {
    private:
        std::string equation;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // For the O(1) look-up
        std::unordered_set<std::string> Signal_label_set;
        std::unordered_set<std::string> Background_label_set;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        /*
        * print point when FOM value is `rank`'th, counting from maximum point to negative direction
        * this option is useful if you do not want to highly optimize the result
        */
        int rank;

        double* Cuts;
        double* NSIGs;
        double* NSIGs_cumulative;
        double* NBKGs;
        double* NBKGs_cumulative;
        double* FOMs;

        // vars for PunziFOM
        double NSIG_initial;
        double alpha;

        std::shared_ptr<std::vector<double>> output_handle;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string png_name;

        double MyEPSILON;
    public:
        DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NSIG_initial_, double alpha_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NSIG_initial(NSIG_initial_), alpha(alpha_), rank(0), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 50
            NBin = 50;

            // just 0.000001
            MyEPSILON = 0.000001;
        }
        DrawPunziFOM(const char* equation_, double MIN_, double MAX_, double NBin_, double NSIG_initial_, double alpha_, int rank_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), NBin(NBin_), NSIG_initial(NSIG_initial_), alpha(alpha_), rank(rank_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~DrawPunziFOM() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // Convert from vector to set
            Signal_label_set.clear();
            Signal_label_set.insert(Signal_label_list.begin(), Signal_label_list.end());
            Background_label_set.clear();
            Background_label_set.insert(Background_label_list.begin(), Background_label_list.end());

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NSIGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NSIGs_cumulative[i] = 0.0;
                NBKGs[i] = 0.0;
                NBKGs_cumulative[i] = 0.0;
            }

            // initialize cuts
            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;
            }

            // check `rank` variable
            if ((rank < 0) || (rank > (NBin - 1))) {
                printf("rank should be within [%d, %d]; current: %d\n", 0, NBin - 1, rank);
                exit(1);
            }
        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                int first_bin = -1;
                if (result < MIN) first_bin = -1;
                else if (result >= MAX) first_bin = NBin - 1;
                else first_bin = std::min(NBin - 1, int(std::floor((result - MIN) / ((MAX - MIN) / NBin))));
                if (first_bin >= 0) {
                    if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[first_bin] = NSIGs[first_bin] + totalweight;
                    if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[first_bin] = NBKGs[first_bin] + totalweight;
                }

                ++iter;
            }

            return 1;
        }

        void End() {

            // calculate cumulative sum
            for (int i = NBin - 1; i >= 0; i--) {
                if (i == (NBin - 1)) {
                    NSIGs_cumulative[i] = NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs[i];
                }
                else {
                    NSIGs_cumulative[i] = NSIGs_cumulative[i + 1] + NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs_cumulative[i + 1] + NBKGs[i];
                }
            }

            FOMs = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                if ((NSIGs_cumulative[i] + NBKGs_cumulative[i]) < MyEPSILON) FOMs[i] = 0.0;
                else {
                    FOMs[i] = (NSIGs_cumulative[i] / NSIG_initial) / (alpha / 2.0 + std::sqrt(NBKGs_cumulative[i]));
                }
            }

            // Store FOMs with their corresponding indices
            std::vector<std::pair<double, int>> FOM_with_index;
            for (int i = 0; i < NBin; ++i) {
                FOM_with_index.push_back(std::make_pair(FOMs[i], i));
            }

            // sort
            std::sort(FOM_with_index.begin(), FOM_with_index.end(),
                [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
                    return a.first > b.first;
                });

            // get maximum index, and minimum FOM
            int MaximumIndex = FOM_with_index[0].second;
            double MinimumFOM = FOM_with_index[FOM_with_index.size() - 1].first;

            // remove the data whose FOM cut is higher than the maximized point
            FOM_with_index.erase(
                std::remove_if(FOM_with_index.begin(), FOM_with_index.end(),
                    [MaximumIndex](const std::pair<double, int>& p) {
                        return p.second > MaximumIndex;
                    }),
                FOM_with_index.end()
            );

            // If `rank` is too high or maximized point is close to 0, this can happend
            if (rank >= (int)FOM_with_index.size()) {
                printf("You try to find too far from maximized point. Just smallest value is set.\n");
                rank = FOM_with_index.size() - 1;
            }

            int OptimizedIndex = FOM_with_index[rank].second;
            double OptimizedFOM = FOM_with_index[rank].first;

            // print result
            printf("PunziFOM scan result for %s:\n", equation.c_str());
            printf("try to find %d-th highest value\n", rank);
            printf("Optimized FOM value: %lf\n", OptimizedFOM);
            printf("Cut value: %lf\n", Cuts[OptimizedIndex]);
            printf("NSIG: %lf\n", NSIGs_cumulative[OptimizedIndex]);
            printf("NBKG: %lf\n", NBKGs_cumulative[OptimizedIndex]);

            output_handle->clear();

            output_handle->push_back((double)rank);
            output_handle->push_back(OptimizedFOM);
            output_handle->push_back(Cuts[OptimizedIndex]);
            output_handle->push_back(NSIGs_cumulative[OptimizedIndex]);
            output_handle->push_back(NBKGs_cumulative[OptimizedIndex]);

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
            free(NSIGs_cumulative);
            free(NBKGs);
            free(NBKGs_cumulative);
            free(FOMs);

            delete c_temp;
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class Draw2DPunziFOM : public Module {
    private:
        /*
         * usage:
         * scan_condition: equation, min, max, bin
         */
        std::vector<std::tuple<const char*, double, double, int>> scan_conditions;
        std::vector<std::vector<Token>> postfix_exprs;

        /*
         * When preselection_equation_x is satisfied, equation_x is used to calculate PunziFOM.
         * On the other hand, when preselection_equation_y is satisfied, equation_y is used to calculate PunziFOM.
         * If both conditions are satisfied, both equation_x and equation_y are used to calculate PunziFOM.
         * If none of them are satisfied, that event is not considered for PunziFOM.
         * This option is useful when you want to add two separate region with different cut variables
         */
        std::string preselection_equation_x;
        std::string preselection_replaced_expr_x;
        std::vector<Token> postfix_expr_x;

        std::string preselection_equation_y;
        std::string preselection_replaced_expr_y;
        std::vector<Token> postfix_expr_y;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // For the O(1) look-up
        std::unordered_set<std::string> Signal_label_set;
        std::unordered_set<std::string> Background_label_set;

        // FOM range/bin
        int NBin_x;
        int NBin_y;
        double MIN_x;
        double MIN_y;
        double MAX_x;
        double MAX_y;

        double** Cuts_x;
        double** Cuts_y;
        double** NSIGs;
        double** NSIGs_cumulative;
        double** NBKGs;
        double** NBKGs_cumulative;
        double** FOMs;

        // vars for PunziFOM
        double NSIG_initial;
        double alpha;

        std::shared_ptr<std::vector<double>> output_handle;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string png_name;

        double MyEPSILON;
    public:
        Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, double NSIG_initial_, double alpha_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), scan_conditions(scan_conditions_), preselection_equation_x("1"), preselection_equation_y("1"), NSIG_initial(NSIG_initial_), alpha(alpha_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }
        Draw2DPunziFOM(std::vector<std::tuple<const char*, double, double, int>> scan_conditions_, const char* preselection_x_, const char* preselection_y_, double NSIG_initial_, double alpha_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), scan_conditions(scan_conditions_), preselection_equation_x(preselection_x_), preselection_equation_y(preselection_y_), NSIG_initial(NSIG_initial_), alpha(alpha_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~Draw2DPunziFOM() {}

        void Start() {
            // change variable name into placeholder
            for (std::vector<std::tuple<const char*, double, double, int>>::const_iterator iter = scan_conditions.begin(); iter != scan_conditions.end(); ++iter) {
                const char* equation = std::get<0>(*iter);

                std::string replaced_expr = replaceInternalValues(std::string(equation), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                std::vector<Token> postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);
                postfix_exprs.push_back(postfix_expr);
            }
            preselection_replaced_expr_x = replaceInternalValues(preselection_equation_x, internal_value);
            preselection_replaced_expr_y = replaceInternalValues(preselection_equation_y, internal_value);
            preselection_replaced_expr_x = replaceVariables(preselection_replaced_expr_x, &variable_names);
            preselection_replaced_expr_y = replaceVariables(preselection_replaced_expr_y, &variable_names);
            postfix_expr_x = PostfixExpression(preselection_replaced_expr_x, &VariableTypes);
            postfix_expr_y = PostfixExpression(preselection_replaced_expr_y, &VariableTypes);

            if (scan_conditions.size() != 2) {
                printf("Draw2DPunziFOM requires 2 element. Currently there are %d element(s)\n", scan_conditions.size());
                exit(1);
            }

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // Convert from vector to set
            Signal_label_set.clear();
            Signal_label_set.insert(Signal_label_list.begin(), Signal_label_list.end());
            Background_label_set.clear();
            Background_label_set.insert(Background_label_list.begin(), Background_label_list.end());

            // copy information
            NBin_x = std::get<3>(scan_conditions.at(0));
            NBin_y = std::get<3>(scan_conditions.at(1));
            MIN_x = std::get<1>(scan_conditions.at(0));
            MIN_y = std::get<1>(scan_conditions.at(1));
            MAX_x = std::get<2>(scan_conditions.at(0));
            MAX_y = std::get<2>(scan_conditions.at(1));

            // malloc history
            Cuts_x = (double**)malloc(sizeof(double*) * NBin_x);
            Cuts_y = (double**)malloc(sizeof(double*) * NBin_x);
            NSIGs = (double**)malloc(sizeof(double*) * NBin_x);
            NSIGs_cumulative = (double**)malloc(sizeof(double*) * NBin_x);
            NBKGs = (double**)malloc(sizeof(double*) * NBin_x);
            NBKGs_cumulative = (double**)malloc(sizeof(double*) * NBin_x);
            for (int i = 0; i < NBin_x; i++) {
                Cuts_x[i] = (double*)malloc(sizeof(double) * NBin_y);
                Cuts_y[i] = (double*)malloc(sizeof(double) * NBin_y);
                NSIGs[i] = (double*)malloc(sizeof(double) * NBin_y);
                NSIGs_cumulative[i] = (double*)malloc(sizeof(double) * NBin_y);
                NBKGs[i] = (double*)malloc(sizeof(double) * NBin_y);
                NBKGs_cumulative[i] = (double*)malloc(sizeof(double) * NBin_y);
            }

            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    Cuts_x[i][j] = 0.0;
                    Cuts_y[i][j] = 0.0;
                    NSIGs[i][j] = 0.0;
                    NSIGs_cumulative[i][j] = 0.0;
                    NBKGs[i][j] = 0.0;
                    NBKGs_cumulative[i][j] = 0.0;
                }
            }

            // initialize cuts
            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    double variable_value_x = MIN_x + ((double)i) * (MAX_x - MIN_x) / (NBin_x - 1);
                    double variable_value_y = MIN_y + ((double)j) * (MAX_y - MIN_y) / (NBin_y - 1);
                    Cuts_x[i][j] = variable_value_x;
                    Cuts_y[i][j] = variable_value_y;
                }
            }
        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result_preselection_x = EvaluatePostfixExpression(postfix_expr_x, iter->variable, &VariableTypes);
                double result_preselection_y = EvaluatePostfixExpression(postfix_expr_y, iter->variable, &VariableTypes);
                double result_x = EvaluatePostfixExpression(postfix_exprs.at(0), iter->variable, &VariableTypes);
                double result_y = EvaluatePostfixExpression(postfix_exprs.at(1), iter->variable, &VariableTypes);

                int first_bin_x = -1;
                if (result_x < MIN_x) first_bin_x = -1;
                else if (result_x >= MAX_x) first_bin_x = NBin_x - 1;
                else first_bin_x = std::min(NBin_x - 1, int(std::floor((result_x - MIN_x) / ((MAX_x - MIN_x) / (NBin_x - 1)))));

                int first_bin_y = -1;
                if (result_y < MIN_y) first_bin_y = -1;
                else if (result_y >= MAX_y) first_bin_y = NBin_y - 1;
                else first_bin_y = std::min(NBin_y - 1, int(std::floor((result_y - MIN_y) / ((MAX_y - MIN_y) / (NBin_y - 1)))));

                if ((result_preselection_x > 0.5) && (result_preselection_y > 0.5)) {
                    if ((first_bin_x >= 0) && (first_bin_y >= 0)) {
                        if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[first_bin_x][first_bin_y] = NSIGs[first_bin_x][first_bin_y] + totalweight;
                        if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[first_bin_x][first_bin_y] = NBKGs[first_bin_x][first_bin_y] + totalweight;
                    }
                }
                else if ((result_preselection_x > 0.5) && (result_preselection_y < 0.5)) {
                    if (first_bin_x >= 0) {
                        if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[first_bin_x][NBin_y - 1] = NSIGs[first_bin_x][NBin_y - 1] + totalweight;
                        if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[first_bin_x][NBin_y - 1] = NBKGs[first_bin_x][NBin_y - 1] + totalweight;
                    }
                }
                else if ((result_preselection_x < 0.5) && (result_preselection_y > 0.5)) {
                    if (first_bin_y >= 0) {
                        if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[NBin_x - 1][first_bin_y] = NSIGs[NBin_x - 1][first_bin_y] + totalweight;
                        if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[NBin_x - 1][first_bin_y] = NBKGs[NBin_x - 1][first_bin_y] + totalweight;
                    }
                }

                ++iter;
            }

            return 1;
        }

        void End() {

            // calculate cumulative sum
            for (int i = NBin_x - 1; i >= 0; i--) {
                for (int j = NBin_y - 1; j >= 0; j--) {
                    if ((i == (NBin_x - 1)) && (j == (NBin_y - 1))) {
                        NSIGs_cumulative[i][j] = NSIGs[i][j];
                        NBKGs_cumulative[i][j] = NBKGs[i][j];
                    }
                    else if ((i == (NBin_x - 1)) && (j != (NBin_y - 1))) {
                        NSIGs_cumulative[i][j] = NSIGs_cumulative[i][j + 1] + NSIGs[i][j];
                        NBKGs_cumulative[i][j] = NBKGs_cumulative[i][j + 1] + NBKGs[i][j];
                    }
                    else if ((i != (NBin_x - 1)) && (j == (NBin_y - 1))) {
                        NSIGs_cumulative[i][j] = NSIGs_cumulative[i + 1][j] + NSIGs[i][j];
                        NBKGs_cumulative[i][j] = NBKGs_cumulative[i + 1][j] + NBKGs[i][j];
                    }
                    else {
                        NSIGs_cumulative[i][j] = NSIGs_cumulative[i + 1][j] + NSIGs_cumulative[i][j + 1] - NSIGs_cumulative[i + 1][j + 1] + NSIGs[i][j];
                        NBKGs_cumulative[i][j] = NBKGs_cumulative[i + 1][j] + NBKGs_cumulative[i][j + 1] - NBKGs_cumulative[i + 1][j + 1] + NBKGs[i][j];
                    }
                }
            }

            FOMs = (double**)malloc(sizeof(double*) * NBin_x);
            for (int i = 0; i < NBin_x; i++) {
                FOMs[i] = (double*)malloc(sizeof(double) * NBin_y);
            }
            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    if ((NSIGs_cumulative[i][j] + NBKGs_cumulative[i][j]) < MyEPSILON) FOMs[i][j] = 0.0;
                    else {
                        FOMs[i][j] = (NSIGs_cumulative[i][j] / NSIG_initial) / (alpha / 2.0 + std::sqrt(NBKGs_cumulative[i][j]));
                    }
                }
            }

            double MinimumFOM = std::numeric_limits<double>::max();
            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    if (MinimumFOM > FOMs[i][j]) MinimumFOM = FOMs[i][j];
                }
            }

            double MaximumFOM = -std::numeric_limits<double>::max();
            int MaximumIndex_x = -1;
            int MaximumIndex_y = -1;
            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    if (MaximumFOM < FOMs[i][j]) {
                        MaximumFOM = FOMs[i][j];
                        MaximumIndex_x = i;
                        MaximumIndex_y = j;
                    }
                }
            }

            // print result
            printf("FOM scan result for %s,%s:\n", std::get<0>(scan_conditions.at(0)), std::get<0>(scan_conditions.at(1)));
            printf("Maximum FOM value: %lf\n", MaximumFOM);
            printf("Cut value: %lf,%lf\n", Cuts_x[MaximumIndex_x][MaximumIndex_y], Cuts_y[MaximumIndex_x][MaximumIndex_y]);
            printf("NSIG: %lf\n", NSIGs_cumulative[MaximumIndex_x][MaximumIndex_y]);
            printf("NBKG: %lf\n", NBKGs_cumulative[MaximumIndex_x][MaximumIndex_y]);

            output_handle->clear();

            output_handle->push_back(MaximumFOM);
            output_handle->push_back(Cuts_x[MaximumIndex_x][MaximumIndex_y]);
            output_handle->push_back(Cuts_y[MaximumIndex_x][MaximumIndex_y]);
            output_handle->push_back(NSIGs_cumulative[MaximumIndex_x][MaximumIndex_y]);
            output_handle->push_back(NBKGs_cumulative[MaximumIndex_x][MaximumIndex_y]);

            // draw FOM plot
            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();

            TH2D* th2 = new TH2D("th2", (";" + std::string(std::get<0>(scan_conditions.at(0))) + " cut;" + std::string(std::get<0>(scan_conditions.at(1))) + " cut;Punzi FOM").c_str(), NBin_x, MIN_x - (0.5 * (MAX_x - MIN_x) / (NBin_x - 1)), MAX_x + (0.5 * (MAX_x - MIN_x) / (NBin_x - 1)), NBin_y, MIN_y - (0.5 * (MAX_y - MIN_y) / (NBin_y - 1)), MAX_y + (0.5 * (MAX_y - MIN_y) / (NBin_y - 1)));
            for (int i = 0; i < NBin_x; i++) {
                for (int j = 0; j < NBin_y; j++) {
                    th2->SetBinContent(i + 1, j + 1, FOMs[i][j]);
                }
            }
            th2->Draw("COLZ");

            c_temp->SaveAs(png_name.c_str());

            for (int i = 0; i < NBin_x; i++) {
                free(Cuts_x[i]);
                free(Cuts_y[i]);
                free(NSIGs[i]);
                free(NSIGs_cumulative[i]);
                free(NBKGs[i]);
                free(NBKGs_cumulative[i]);
            }
            free(Cuts_x);
            free(Cuts_y);
            free(NSIGs);
            free(NSIGs_cumulative);
            free(NBKGs);
            free(NBKGs_cumulative);
            for (int i = 0; i < NBin_x; i++) free(FOMs[i]);
            free(FOMs);

            delete c_temp;
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            for (const auto& scan_condition : scan_conditions) {
                result.merge(GetVariablesFromExpression(std::string(std::get<0>(scan_condition)), variable_names));
            }

            result.merge(GetVariablesFromExpression(preselection_equation_x, variable_names));
            result.merge(GetVariablesFromExpression(preselection_equation_y, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class CalculateAUC : public Module {
    private:
        std::string equation;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // For the O(1) look-up
        std::unordered_set<std::string> Signal_label_set;
        std::unordered_set<std::string> Background_label_set;

        // FOM range/bin
        int NBin;
        double MIN;
        double MAX;

        double* Cuts;
        double* NSIGs;
        double* NSIGs_cumulative;
        double* NBKGs;
        double* NBKGs_cumulative;

        double NSIGs_total;
        double NBKGs_total;

        std::shared_ptr<double> output_handle;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string output_name;
        std::string write_option;

        double MyEPSILON;
    public:
        CalculateAUC(const char* equation_, double MIN_, double MAX_, const char* output_name_, const char* write_option_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::shared_ptr<double> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), output_name(output_name_), write_option(write_option_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // just 100
            NBin = 100;

            NSIGs_total = 0;
            NBKGs_total = 0;
        }

        ~CalculateAUC() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);

            if (Signal_label_list.size() == 0) {
                printf("signal should be defined. Use `SetSignal`\n");
                exit(1);
            }
            else if (Background_label_list.size() == 0) {
                printf("background should be defined. Use `SetBackground`\n");
                exit(1);
            }

            // Convert from vector to set
            Signal_label_set.clear();
            Signal_label_set.insert(Signal_label_list.begin(), Signal_label_list.end());
            Background_label_set.clear();
            Background_label_set.insert(Background_label_list.begin(), Background_label_list.end());

            // malloc history
            Cuts = (double*)malloc(sizeof(double) * NBin);
            NSIGs = (double*)malloc(sizeof(double) * NBin);
            NSIGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            NBKGs = (double*)malloc(sizeof(double) * NBin);
            NBKGs_cumulative = (double*)malloc(sizeof(double) * NBin);
            for (int i = 0; i < NBin; i++) {
                Cuts[i] = 0.0;
                NSIGs[i] = 0.0;
                NSIGs_cumulative[i] = 0.0;
                NBKGs[i] = 0.0;
                NBKGs_cumulative[i] = 0.0;
            }

            // initialize cuts
            for (int i = 0; i < NBin; i++) {
                double variable_value = MIN + ((double)i) * (MAX - MIN) / NBin;
                Cuts[i] = variable_value;
            }

            // check write option
            if (write_option == "w") {}
            else if (write_option == "a") {}
            else {
                printf("[CalculateAUC] write option should be one of `w` or `a`\n");
                exit(1);
            }
        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                int first_bin = -1;
                if (result < MIN) first_bin = -1;
                else if (result >= MAX) first_bin = NBin - 1;
                else first_bin = std::min(NBin - 1, int(std::floor((result - MIN) / ((MAX - MIN) / NBin))));
                if (first_bin >= 0) {
                    if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs[first_bin] = NSIGs[first_bin] + totalweight;
                    if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs[first_bin] = NBKGs[first_bin] + totalweight;
                }

                ++iter;
            }

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                if (Signal_label_set.find(iter->label) != Signal_label_set.end()) NSIGs_total = NSIGs_total + totalweight;
                if (Background_label_set.find(iter->label) != Background_label_set.end()) NBKGs_total = NBKGs_total + totalweight;

                ++iter;
            }

            return 1;
        }

        void End() {

            // calculate cumulative sum
            for (int i = NBin - 1; i >= 0; i--) {
                if (i == (NBin - 1)) {
                    NSIGs_cumulative[i] = NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs[i];
                }
                else {
                    NSIGs_cumulative[i] = NSIGs_cumulative[i + 1] + NSIGs[i];
                    NBKGs_cumulative[i] = NBKGs_cumulative[i + 1] + NBKGs[i];
                }
            }

            // get AUC
            double AUC = 0;
            for (int i = 0; i < NBin; i++) {
                if (i != (NBin - 1)) {
                    double del_FPR = (NBKGs_cumulative[i] / NBKGs_total) - (NBKGs_cumulative[i + 1] / NBKGs_total);
                    double avg_TPR = ((NSIGs_cumulative[i + 1] / NSIGs_total) + (NSIGs_cumulative[i] / NSIGs_total)) / 2.0;
                    AUC = AUC + del_FPR * avg_TPR;
                }
                else {
                    double del_FPR = (NBKGs_cumulative[i] / NBKGs_total) - 0.0;
                    double avg_TPR = ((NSIGs_cumulative[i] / NSIGs_total) + 0.0) / 2.0;
                    AUC = AUC + del_FPR * avg_TPR;
                }
            }

            // print AUC
            FILE* fp = fopen(output_name.c_str(), write_option.c_str());
            fprintf(fp, "%lf ", AUC);
            fclose(fp);

            (*output_handle) = AUC;

            free(Cuts);
            free(NSIGs);
            free(NSIGs_cumulative);
            free(NBKGs);
            free(NBKGs_cumulative);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;
        std::string expression;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

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
        DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), stack_title(stack_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(false), LogScale(false), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), stack_title(stack_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), stack_title(stack_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(false), LogScale(false), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        DrawStack(const char* expression_, const char* stack_title_, const char* png_name_, bool normalized_, bool LogScale_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string> data_label_list_, std::vector<std::string> MC_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression(expression_), stack_title(stack_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), normalized(normalized_), LogScale(LogScale_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), data_label_list(data_label_list_), MC_label_list(MC_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

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

            // remove duplicated labels
            removeDuplicates(Signal_label_list);
            removeDuplicates(Background_label_list);
            removeDuplicates(data_label_list);
            removeDuplicates(MC_label_list);

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
            replaced_expr = replaceInternalValues(expression, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);

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

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);
                if ( (std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) != stack_label_list.end()) || (std::find(hist_label_list.begin(), hist_label_list.end(), iter->label) != hist_label_list.end())) {

                    if (stack_hist == nullptr) {
                        x_variable.push_back(result);
                        weight.push_back(totalweight);
                        label.push_back(iter->label);
                    }
                    else {
                        if (std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) != stack_label_list.end()) {
                            int label_index = std::find(stack_label_list.begin(), stack_label_list.end(), iter->label) - stack_label_list.begin();
                            stack_hist[label_index]->Fill(result, totalweight);
                            stack_error->Fill(result, totalweight);
                        }
                        else if (std::find(hist_label_list.begin(), hist_label_list.end(), iter->label) != hist_label_list.end()) {
                            hist->Fill(result, totalweight);
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
                legend->AddEntry(hist, Signal_label_list.at(0).c_str(), "f");
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

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(expression, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class FastBDTTrain : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::string Signal_equation;
        std::string Signal_replaced_expr;
        std::vector<Token> Signal_postfix_expr;

        std::string Background_equation;
        std::string Background_replaced_expr;
        std::vector<Token> Background_postfix_expr;

        std::vector<std::string> Signal_label_list;
        std::vector<std::string> Background_label_list;

        // For the O(1) look-up
        std::unordered_set<std::string> Signal_label_set;
        std::unordered_set<std::string> Background_label_set;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::map<std::string, double> hyperparameters;

        // input variables
        std::vector<std::vector<float>> InputVariables;
        std::vector<float>* InputVariable;
        std::vector<bool> IsItSignal;
        std::vector<float> weight;

        std::string path;
        std::string output_name;

        // FBDT class
        FastBDT::Classifier classifier;

        /* 
         * balanced_weight option :
         * if it is turn ON, the number of signal is reweighted to the number of background
         */
        bool balanced_weight;

    public:
        FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, const char* path_, const char* output_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(input_variables_), Signal_equation(Signal_preselection_), Background_equation(Background_preselection_), hyperparameters(hyperparameters_), balanced_weight(false), path(path_), output_name(output_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
        }

        FastBDTTrain(std::vector<std::string> input_variables_, const char* Signal_preselection_, const char* Background_preselection_, std::map<std::string, double> hyperparameters_, bool balanced_weight_, const char* path_, const char* output_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(input_variables_), Signal_equation(Signal_preselection_), Background_equation(Background_preselection_), hyperparameters(hyperparameters_), balanced_weight(balanced_weight_), path(path_), output_name(output_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
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

            // Convert from vector to set
            Signal_label_set.clear();
            Signal_label_set.insert(Signal_label_list.begin(), Signal_label_list.end());
            Background_label_set.clear();
            Background_label_set.insert(Background_label_list.begin(), Background_label_list.end());

            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }
            Signal_replaced_expr = replaceInternalValues(Signal_equation, internal_value);
            Background_replaced_expr = replaceInternalValues(Background_equation, internal_value);
            Signal_replaced_expr = replaceVariables(Signal_replaced_expr, &variable_names);
            Background_replaced_expr = replaceVariables(Background_replaced_expr, &variable_names);
            Signal_postfix_expr = PostfixExpression(Signal_replaced_expr, &VariableTypes);
            Background_postfix_expr = PostfixExpression(Background_replaced_expr, &VariableTypes);

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
            std::vector<unsigned int> binning(postfix_exprs.size(), static_cast<unsigned int>(hyperparameters["Binning"]));
            classifier.SetBinning(binning);

            // malloc input variables
            InputVariable = new std::vector<float>[postfix_exprs.size()];
        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                // care about preselection first
                double preselection_result = -1;

                if (Signal_label_set.find(iter->label) != Signal_label_set.end()) {
                    if (Signal_replaced_expr == "") preselection_result = 1;
                    else {
                        preselection_result = EvaluatePostfixExpression(Signal_postfix_expr, iter->variable, &VariableTypes);
                    }
                }
                else if (Background_label_set.find(iter->label) != Background_label_set.end()) {
                    if (Background_replaced_expr == "") preselection_result = 1;
                    else {
                        preselection_result = EvaluatePostfixExpression(Background_postfix_expr, iter->variable, &VariableTypes);
                    }
                }
                else {
                    preselection_result = -1; // label is not registered. Do not use this data
                }

                if (preselection_result > 0.5) { // put input variables
                    for (int i = 0; i < postfix_exprs.size(); i++) {
                        double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                        InputVariable[i].push_back(result);
                    }

                    // put answer
                    if (Signal_label_set.find(iter->label) != Signal_label_set.end()) IsItSignal.push_back(true);
                    else if (Background_label_set.find(iter->label) != Background_label_set.end()) IsItSignal.push_back(false);

                    // put weight
                    weight.push_back(static_cast<float>(totalweight));
                }

                ++iter;
            }

            return 1;
        }

        void End() override {
            // reweight, if balanced_weight == true
            if (balanced_weight) {
                double sum_bkgs = 0.0;
                double sum_signal = 0.0;

                for (int i = 0; i < weight.size(); i++) {
                    if (IsItSignal.at(i)) sum_signal = sum_signal + weight.at(i);
                    else sum_bkgs = sum_bkgs + weight.at(i);
                }

                if (sum_signal == 0) {
                    printf("[FastBDTTrain] there is zero signal\n");
                    exit(1);
                }

                double reweight_factor = sum_bkgs / sum_signal;

                for (int i = 0; i < weight.size(); i++) {
                    if (IsItSignal.at(i)) weight.at(i) = weight.at(i) * reweight_factor;
                }
            }

            // fill
            for (int i = 0; i < postfix_exprs.size(); i++) {
                InputVariables.push_back(InputVariable[i]);
            }

            // fit
            classifier.fit(InputVariables, IsItSignal, weight);

            // free memory
            delete[] InputVariable;

            // save model
            std::fstream out_stream;
            if (output_name.empty()) out_stream.open((path + "/" + std::to_string(hyperparameters["NTrees"]) + "_" + std::to_string(hyperparameters["Depth"]) + "_" + std::to_string(hyperparameters["Shrinkage"]) + "_" + std::to_string(hyperparameters["Subsample"]) + "_" + std::to_string(hyperparameters["Binning"]) + ".weightfile").c_str(), std::ios_base::out | std::ios_base::trunc);
            else out_stream.open((path + "/" + output_name).c_str(), std::ios_base::out | std::ios_base::trunc);
            out_stream << classifier << std::endl;
            out_stream.close();
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));
            result.merge(GetVariablesFromExpression(Signal_equation, variable_names));
            result.merge(GetVariablesFromExpression(Background_equation, variable_names));
            for (const std::vector<std::size_t> variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }

        bool BlocksDownstream() const override {
            return true;
        }
    };

    class FastBDTApplication : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        // FBDT class
        std::string classifier_path;
        FastBDT::Classifier classifier;

        std::string branch_name;

    public:
        FastBDTApplication(std::vector<std::string> input_variables_, const char* classifier_path_, const char* branch_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(input_variables_), classifier_path(classifier_path_), branch_name(branch_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
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

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                std::vector<float> inputs;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    inputs.push_back(result);
                }

                float Output_FBDT = classifier.predict(inputs);
                iter->variable.push_back(static_cast<float>(Output_FBDT));

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return GetVariablesFromExpressions(equations, variable_names);
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
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        // the number of split and which one do you want to select?
        int split_num;
        int selected_index;

    public:
        RandomEventSelection(int split_num_, int selected_index_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), split_num(split_num_), selected_index(selected_index_), Event_variable_list(Event_variable_list_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

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

        int Process(std::deque<Data>* data) override {

            // Convert the string to a size_t hash value
            std::hash<std::string> hasher;
            size_t hashValue;
            if (data->size() > 0) hashValue = hasher(data->at(0).filename);
            else hashValue = 42;

            // Initialize the random number generator with the hash value
            std::mt19937 rng(static_cast<unsigned int>(hashValue));
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            // It is temporary data to save Data before/after selection is done.
            std::deque<Data> temp_data;
            std::deque<Data> temp_data_after_selection;

            // initialization flag previous event variable
            bool ItIsTheFirstData = true; // we erase data from std::deque<Data>. we should avoid the comparison with data->begin()
            std::vector<std::variant<int, unsigned int, float, double, std::string*>> previous_event_variable = temp_event_variable;

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); iter++) {
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
            data->clear();
            data->swap(temp_data_after_selection);

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return GetVariablesFromExpressions(Event_variable_list, variable_names);
        }
    };

    class DefineNewVariable : public Module {
    private:
        std::string equation;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        // FBDT class
        std::string new_variable_name;

    public:
        DefineNewVariable(const char* equation_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equation(equation_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, variable_names_);
            postfix_expr = PostfixExpression(replaced_expr, VariableTypes_);

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

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                iter->variable.push_back(static_cast<double>(result));

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            // new variable is not included
            return GetVariablesFromExpression(equation, variable_names);
        }
    };

    class RemoveVariable : public Module {
    private:
        std::vector<std::string> removed_variable_names;
        std::vector<int> removed_index;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        RemoveVariable(std::vector<std::string> removed_variable_names_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), removed_variable_names(removed_variable_names_) {

            // remove from internal value
            for (std::string removed_variable_name : removed_variable_names) {
                auto iter = internal_value_->find(removed_variable_name);
                if (iter != internal_value_->end()) internal_value_->erase(iter);
            }

            // get index
            for (std::string removed_variable_name : removed_variable_names) {
                std::vector<std::string>::iterator iter = std::find(variable_names_->begin(), variable_names_->end(), removed_variable_name);

                if (iter != variable_names_->end()) {
                    removed_index.push_back(iter - variable_names_->begin());
                }
            }

            // sort by decreasing order
            std::sort(removed_index.begin(), removed_index.end(), std::greater<int>());

            // copy variable list first, but it will not be used
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);
            eventweights = (*eventweights_);
            variable_indices_list = (*variable_indices_list_);

            // remove variable
            for (int idx : removed_index) {
                variable_names_->erase(variable_names_->begin() + idx);
                VariableTypes_->erase(VariableTypes_->begin() + idx);

                // calculate index for event weight again
                for (std::size_t i = 0; i < variable_indices_list_->size(); i++) {
                    for (std::size_t j = 0; j < variable_indices_list_->at(i).size(); j++) {
                        std::size_t temp_index = variable_indices_list_->at(i).at(j);
                        if (temp_index == idx) {
                            printf("[RemoveVariable] variable used in weight is tried to be removed\n");
                            exit(1);
                        }
                        if (temp_index > idx) variable_indices_list_->at(i).at(j) = temp_index - 1;
                    }
                }
            }
        }

        ~RemoveVariable() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                for (int idx : removed_index) {
                    iter->variable.erase(iter->variable.begin() + idx);
                }

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class ConditionalPairDefineNewVariable : public Module {
    private:
        std::map<std::string, std::string> condition_equation__criteria_equation_list;
        std::vector<std::pair<std::vector<Token>, std::vector<Token>>> condition_postfix_expr__criteria_postfix_expr_list;

        int condition_order; // start from 0. 0 means highest

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        // FBDT class
        std::string new_variable_name;

    public:
        ConditionalPairDefineNewVariable(std::map<std::string, std::string> condition_equation__criteria_equation_list_, int condition_order_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), condition_equation__criteria_equation_list(condition_equation__criteria_equation_list_), condition_order(condition_order_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (std::map<std::string, std::string>::iterator iter_eq = condition_equation__criteria_equation_list.begin(); iter_eq != condition_equation__criteria_equation_list.end(); ++iter_eq) {
                std::string condition_replaced_expr = replaceInternalValues(iter_eq->first, internal_value);
                condition_replaced_expr = replaceVariables(condition_replaced_expr, variable_names_);
                std::string criteria_replaced_expr = replaceInternalValues(iter_eq->second, internal_value);
                criteria_replaced_expr = replaceVariables(criteria_replaced_expr, variable_names_);

                condition_postfix_expr__criteria_postfix_expr_list.push_back(std::make_pair(PostfixExpression(condition_replaced_expr, VariableTypes_), PostfixExpression(criteria_replaced_expr, VariableTypes_)));
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

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double condition_result = -1;
                std::vector<double> condition_results;
                double criteria_result = std::numeric_limits<double>::max();
                std::vector<std::vector<Token>> criteria_postfix_exprs;

                for (std::vector<std::pair<std::vector<Token>, std::vector<Token>>>::iterator iter_eq = condition_postfix_expr__criteria_postfix_expr_list.begin(); iter_eq != condition_postfix_expr__criteria_postfix_expr_list.end(); ++iter_eq) {
                    double temp_ = EvaluatePostfixExpression(iter_eq->first, iter->variable, &VariableTypes);
                    condition_results.push_back(temp_);
                    criteria_postfix_exprs.push_back(iter_eq->second);
                }

                std::vector<double> temp_condition_results = condition_results;
                std::nth_element(temp_condition_results.begin(), temp_condition_results.begin() + condition_order, temp_condition_results.end(), std::greater<double>());

                // The n-th largest value
                condition_result = temp_condition_results.at(condition_order);

                // Find the original index of the n-th largest value
                std::vector<double>::iterator iter_condition_results = std::find(condition_results.begin(), condition_results.end(), condition_result);
                std::size_t index = std::distance(condition_results.begin(), iter_condition_results);

                criteria_result = EvaluatePostfixExpression(criteria_postfix_exprs.at(index), iter->variable, &VariableTypes);

                iter->variable.push_back(static_cast<double>(criteria_result));

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            for (const auto& [condition_equation, criteria_equation] : condition_equation__criteria_equation_list) {
                result.merge(GetVariablesFromExpression(condition_equation, variable_names));
                result.merge(GetVariablesFromExpression(criteria_equation, variable_names));
            }

            return result;
        }
    };

    class GetAverage : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string new_variable_name;

    public:
        GetAverage(std::vector<std::string> equations_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(equations_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[GetAverage] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~GetAverage() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double avg = 0;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    avg = avg + result;
                }
                avg = avg / postfix_exprs.size();

                iter->variable.push_back(avg);

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            return result;
        }
    };

    class GetStdDev : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        std::string new_variable_name;

    public:
        GetStdDev(std::vector<std::string> equations_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(equations_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[GetStdDev] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~GetStdDev() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                double avg = 0;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    avg = avg + result;
                }
                avg = avg / postfix_exprs.size();

                double std = 0;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    std = std + (result - avg) * (result - avg);
                }
                std = std / postfix_exprs.size();
                std = std::sqrt(std);

                iter->variable.push_back(std);

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            return result;
        }
    };

    class GetDiff : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        int order;

        std::string new_variable_name;

    public:
        GetDiff(std::vector<std::string> equations_, int order_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(equations_), order(order_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[GetDiff] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            int N_comb = postfix_exprs.size() * (postfix_exprs.size() - 1) / 2;
            if ((order < 0) || (order > (N_comb - 1))) {
                printf("[GetDiff] order should be within [%d,%d]\n", 0, N_comb - 1);
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~GetDiff() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                std::vector<double> inputs;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    inputs.push_back(result);
                }

                std::vector<double> Diffs;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result_i = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    for (int j = i + 1; j < postfix_exprs.size(); j++) {
                        double result_j = EvaluatePostfixExpression(postfix_exprs.at(j), iter->variable, &VariableTypes);
                        Diffs.push_back(std::abs(result_i - result_j));
                    }
                }

                std::sort(Diffs.begin(), Diffs.end(), std::greater<double>());

                iter->variable.push_back(Diffs.at(order));

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            return result;
        }
    };

    class GetAdd : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        int order;

        std::string new_variable_name;

    public:
        GetAdd(std::vector<std::string> equations_, int order_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), equations(equations_), order(order_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[GetAdd] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            int N_comb = postfix_exprs.size() * (postfix_exprs.size() - 1) / 2;
            if ((order < 0) || (order > (N_comb - 1))) {
                printf("[GetAdd] order should be within [%d,%d]\n", 0, N_comb - 1);
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~GetAdd() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                std::vector<double> inputs;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    inputs.push_back(result);
                }

                std::vector<double> Adds;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result_i = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    for (int j = i + 1; j < postfix_exprs.size(); j++) {
                        double result_j = EvaluatePostfixExpression(postfix_exprs.at(j), iter->variable, &VariableTypes);
                        Adds.push_back(result_i + result_j);
                    }
                }

                std::sort(Adds.begin(), Adds.end(), std::greater<double>());

                iter->variable.push_back(Adds.at(order));

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            return result;
        }
    };

    class GetRandom : public Module {
    private:
        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;

        std::string new_variable_name;

    public:
        GetRandom(std::vector<std::string> equations_, const char* new_variable_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_) : Module(), equations(equations_), new_variable_name(new_variable_name_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_) {
            // change variable name into placeholder
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceVariables(equations.at(i), variable_names_);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, VariableTypes_));
            }

            // check there is the same branch name or not
            if (std::find(variable_names_->begin(), variable_names_->end(), new_variable_name) != variable_names_->end()) {
                printf("[GetRandom] there is already %s variable\n", new_variable_name.c_str());
                exit(1);
            }

            // copy variable list first, because we use it inside the module
            variable_names = (*variable_names_);
            VariableTypes = (*VariableTypes_);

            // add variable
            variable_names_->push_back(new_variable_name);
            VariableTypes_->push_back("Double_t");
        }

        ~GetRandom() {}

        void Start() {

        }

        int Process(std::deque<Data>* data) {

            // Convert the string to a size_t hash value
            std::hash<std::string> hasher;
            size_t hashValue;
            if (data->size() > 0) hashValue = hasher(data->at(0).filename);
            else hashValue = 42;

            // Initialize the random number generator with the hash value
            std::mt19937 rng(static_cast<unsigned int>(hashValue));
            std::uniform_int_distribution<int> dist(0, postfix_exprs.size() - 1);

            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {

                std::vector<double> inputs;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    inputs.push_back(result);
                }

                iter->variable.push_back(inputs.at(dist(rng)));

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
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillDataSet(RooDataSet* dataset_, std::vector<RooRealVar*> realvars_, std::vector<std::string> equations_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), dataset(dataset_), realvars(realvars_), equations(equations_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillDataSet() {}
        void Start() {
            for (int i = 0; i < equations.size(); i++) {
                std::string equation = equations.at(i);
                std::string replaced_expr = replaceInternalValues(equation, internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }

        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                for (int i = 0; i < postfix_exprs.size(); i++) {
                    std::vector<Token> postfix_expr = postfix_exprs.at(i);
                    double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);
                    *(realvars.at(i)) = result;
                }

                RooArgSet temp_;
                for (int i = 0; i < postfix_exprs.size(); i++) temp_.add(*(realvars.at(i)));

                dataset->add(temp_, totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
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
        std::vector<Token> postfix_expr_x;

        std::string equation_y;
        std::string replaced_expr_y;
        std::vector<Token> postfix_expr_y;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillTProfile(TProfile* tprofile_, std::string equation_x_, std::string equation_y_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), tprofile(tprofile_), equation_x(equation_x_), equation_y(equation_y_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillTProfile() {}
        void Start() {
            replaced_expr_x = replaceInternalValues(equation_x, internal_value);
            replaced_expr_y = replaceInternalValues(equation_y, internal_value);
            replaced_expr_x = replaceVariables(replaced_expr_x, &variable_names);
            replaced_expr_y = replaceVariables(replaced_expr_y, &variable_names);
            postfix_expr_x = PostfixExpression(replaced_expr_x, &VariableTypes);
            postfix_expr_y = PostfixExpression(replaced_expr_y, &VariableTypes);
        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result_x = EvaluatePostfixExpression(postfix_expr_x, iter->variable, &VariableTypes);
                double result_y = EvaluatePostfixExpression(postfix_expr_y, iter->variable, &VariableTypes);

                tprofile->Fill(result_x, result_y, totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation_x, variable_names));
            result.merge(GetVariablesFromExpression(equation_y, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class FillTH1D : public Module {
        /*
        * This module is used to fill TH1D
        */
    private:

        TH1D* th1d;

        std::string equation;
        std::string replaced_expr;
        std::vector<Token> postfix_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillTH1D(TH1D* th1d_, std::string equation_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), th1d(th1d_), equation(equation_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillTH1D() {}
        void Start() {
            replaced_expr = replaceInternalValues(equation, internal_value);
            replaced_expr = replaceVariables(replaced_expr, &variable_names);
            postfix_expr = PostfixExpression(replaced_expr, &VariableTypes);
        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr, iter->variable, &VariableTypes);

                th1d->Fill(result, totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class FillCustomizedTH1D : public Module {
        /*
        * This module is used to fill TH1D with customized function
        */
    private:

        TH1D* th1d;
        double (*custom_function)(std::vector<double>);

        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillCustomizedTH1D(TH1D* th1d_, std::vector<std::string> equations_, double (*custom_function_)(std::vector<double>), std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), th1d(th1d_), equations(equations_), custom_function(custom_function_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillCustomizedTH1D() {}
        void Start() {
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }
        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                std::vector<double> results;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    results.push_back(result);
                }

                double filled_value = custom_function(results);
                if(std::isnan(filled_value) == false) th1d->Fill(custom_function(results), totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class FillTH2D : public Module {
        /*
        * This module is used to fill TH2D
        */
    private:

        TH2D* th2d;

        std::string x_expression;
        std::string x_replaced_expr;
        std::vector<Token> x_postfix_expr;
        std::string y_expression;
        std::string y_replaced_expr;
        std::vector<Token> y_postfix_expr;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillTH2D(TH2D* th2d_, const char* x_expression_, const char* y_expression_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), th2d(th2d_), x_expression(x_expression_), y_expression(y_expression_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillTH2D() {}
        void Start() {
            x_replaced_expr = replaceInternalValues(x_expression, internal_value);
            y_replaced_expr = replaceInternalValues(y_expression, internal_value);
            x_replaced_expr = replaceVariables(x_replaced_expr, &variable_names);
            y_replaced_expr = replaceVariables(y_replaced_expr, &variable_names);
            x_postfix_expr = PostfixExpression(x_replaced_expr, &VariableTypes);
            y_postfix_expr = PostfixExpression(y_replaced_expr, &VariableTypes);
        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double x_result = EvaluatePostfixExpression(x_postfix_expr, iter->variable, &VariableTypes);
                double y_result = EvaluatePostfixExpression(y_postfix_expr, iter->variable, &VariableTypes);

                th2d->Fill(x_result, y_result, totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(x_expression, variable_names));
            result.merge(GetVariablesFromExpression(y_expression, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class FillCustomizedTH2D : public Module {
        /*
        * This module is used to fill TH2D with customized function
        */
    private:

        TH2D* th2d;
        double (*x_custom_function)(std::vector<double>);
        double (*y_custom_function)(std::vector<double>);

        std::vector<std::string> equations;
        std::vector<std::vector<Token>> postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        FillCustomizedTH2D(TH2D* th2d_, std::vector<std::string> equations_, double (*x_custom_function_)(std::vector<double>), double (*y_custom_function_)(std::vector<double>), std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), th2d(th2d_), equations(equations_), x_custom_function(x_custom_function_), y_custom_function(y_custom_function_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~FillCustomizedTH2D() {}
        void Start() {
            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }
        }
        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                std::vector<double> results;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    results.push_back(result);
                }

                double filled_value_x = x_custom_function(results);
                double filled_value_y = y_custom_function(results);
                if ((std::isnan(filled_value_x) == false) && (std::isnan(filled_value_y) == false)) th2d->Fill(filled_value_x, filled_value_y, totalweight);

                ++iter;
            }
            return 1;
        }
        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class PrintEvent : public Module {
    private:
        std::vector<std::string> printed_values;
        std::vector<std::vector<Token>> postfix_exprs;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        PrintEvent(std::vector<std::string> printed_values_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), printed_values(printed_values_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ~PrintEvent() {}

        void Start() {
            // change variable name into placeholder
            for (int i = 0; i < printed_values.size(); i++) {
                std::string replaced_expr = replaceInternalValues(printed_values.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                std::vector<double> results;

                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    results.push_back(result);
                }

                printf("===================================\n");
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    printf("%s: %lf\n", printed_values.at(i).c_str(), results.at(i));
                }
                printf("===================================\n");

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(printed_values, variable_names));

            return result;
        }
    };

    class ABCDmethod : public Module {
    private:

        // N_A = N_B * (N_C/N_D)

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

        TH1D* th1d_ABCD;
        std::string expression_A;
        std::string replaced_expr_A;
        std::vector<Token> postfix_expr_A;
        std::string expression_B;
        std::string replaced_expr_B;
        std::vector<Token> postfix_expr_B;
        std::string expression_C;
        std::string replaced_expr_C;
        std::vector<Token> postfix_expr_C;
        std::string expression_D;
        std::string replaced_expr_D;
        std::vector<Token> postfix_expr_D;

        TH1D* th1d_ABCD_validation;
        std::string expression_Aprime;
        std::string replaced_expr_Aprime;
        std::vector<Token> postfix_expr_Aprime;
        std::string expression_Bprime;
        std::string replaced_expr_Bprime;
        std::vector<Token> postfix_expr_Bprime;
        std::string expression_Cprime;
        std::string replaced_expr_Cprime;
        std::vector<Token> postfix_expr_Cprime;
        std::string expression_Dprime;
        std::string replaced_expr_Dprime;
        std::vector<Token> postfix_expr_Dprime;

        bool WeightSumError;
        bool validation;

        std::shared_ptr<std::vector<double>> output_handle;

    public:
        ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, bool WeightSumError_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression_A(region_A_), expression_B(region_B_), expression_C(region_C_), expression_D(region_D_), expression_Aprime(""), expression_Bprime(""), expression_Cprime(""), expression_Dprime(""), validation(false), WeightSumError(WeightSumError_), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}
        ABCDmethod(const char* region_A_, const char* region_B_, const char* region_C_, const char* region_D_, const char* region_Aprime_, const char* region_Bprime_, const char* region_Cprime_, const char* region_Dprime_, bool WeightSumError_, std::shared_ptr<std::vector<double>> output_handle_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), expression_A(region_A_), expression_B(region_B_), expression_C(region_C_), expression_D(region_D_), expression_Aprime(region_Aprime_), expression_Bprime(region_Bprime_), expression_Cprime(region_Cprime_), expression_Dprime(region_Dprime_), WeightSumError(WeightSumError_), validation(true), output_handle(output_handle_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_) {}

        ~ABCDmethod() {}

        void Start() override {
            replaced_expr_A = replaceInternalValues(expression_A, internal_value);
            replaced_expr_B = replaceInternalValues(expression_B, internal_value);
            replaced_expr_C = replaceInternalValues(expression_C, internal_value);
            replaced_expr_D = replaceInternalValues(expression_D, internal_value);
            replaced_expr_A = replaceVariables(replaced_expr_A, &variable_names);
            replaced_expr_B = replaceVariables(replaced_expr_B, &variable_names);
            replaced_expr_C = replaceVariables(replaced_expr_C, &variable_names);
            replaced_expr_D = replaceVariables(replaced_expr_D, &variable_names);
            postfix_expr_A = PostfixExpression(replaced_expr_A, &VariableTypes);
            postfix_expr_B = PostfixExpression(replaced_expr_B, &VariableTypes);
            postfix_expr_C = PostfixExpression(replaced_expr_C, &VariableTypes);
            postfix_expr_D = PostfixExpression(replaced_expr_D, &VariableTypes);

            if (validation) {
                replaced_expr_Aprime = replaceInternalValues(expression_Aprime, internal_value);
                replaced_expr_Bprime = replaceInternalValues(expression_Bprime, internal_value);
                replaced_expr_Cprime = replaceInternalValues(expression_Cprime, internal_value);
                replaced_expr_Dprime = replaceInternalValues(expression_Dprime, internal_value);
                replaced_expr_Aprime = replaceVariables(replaced_expr_Aprime, &variable_names);
                replaced_expr_Bprime = replaceVariables(replaced_expr_Bprime, &variable_names);
                replaced_expr_Cprime = replaceVariables(replaced_expr_Cprime, &variable_names);
                replaced_expr_Dprime = replaceVariables(replaced_expr_Dprime, &variable_names);
                postfix_expr_Aprime = PostfixExpression(replaced_expr_Aprime, &VariableTypes);
                postfix_expr_Bprime = PostfixExpression(replaced_expr_Bprime, &VariableTypes);
                postfix_expr_Cprime = PostfixExpression(replaced_expr_Cprime, &VariableTypes);
                postfix_expr_Dprime = PostfixExpression(replaced_expr_Dprime, &VariableTypes);
            }

            // create histogram
            std::string hist_name = generateRandomString(12);
            th1d_ABCD = new TH1D(hist_name.c_str(), "ABCD", 4, 0, 4);

            if (validation) {
                hist_name = generateRandomString(12);
                th1d_ABCD_validation = new TH1D(hist_name.c_str(), "ABCD", 4, 0, 4);
            }
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result = EvaluatePostfixExpression(postfix_expr_A, iter->variable, &VariableTypes);
                if (result > 0.5) th1d_ABCD->Fill(0.5, totalweight);

                result = EvaluatePostfixExpression(postfix_expr_B, iter->variable, &VariableTypes);
                if (result > 0.5) th1d_ABCD->Fill(1.5, totalweight);

                result = EvaluatePostfixExpression(postfix_expr_C, iter->variable, &VariableTypes);
                if (result > 0.5) th1d_ABCD->Fill(2.5, totalweight);

                result = EvaluatePostfixExpression(postfix_expr_D, iter->variable, &VariableTypes);
                if (result > 0.5) th1d_ABCD->Fill(3.5, totalweight);

                if (validation) {
                    result = EvaluatePostfixExpression(postfix_expr_Aprime, iter->variable, &VariableTypes);
                    if (result > 0.5) th1d_ABCD_validation->Fill(0.5, totalweight);

                    result = EvaluatePostfixExpression(postfix_expr_Bprime, iter->variable, &VariableTypes);
                    if (result > 0.5) th1d_ABCD_validation->Fill(1.5, totalweight);

                    result = EvaluatePostfixExpression(postfix_expr_Cprime, iter->variable, &VariableTypes);
                    if (result > 0.5) th1d_ABCD_validation->Fill(2.5, totalweight);

                    result = EvaluatePostfixExpression(postfix_expr_Dprime, iter->variable, &VariableTypes);
                    if (result > 0.5) th1d_ABCD_validation->Fill(3.5, totalweight);
                }

                ++iter;
            }
            return 1;
        }

        void End() override {

            double N_A = th1d_ABCD->GetBinContent(1);
            double N_B = th1d_ABCD->GetBinContent(2);
            double N_C = th1d_ABCD->GetBinContent(3);
            double N_D = th1d_ABCD->GetBinContent(4);

            double N_A_err = 0;
            double N_B_err = 0;
            double N_C_err = 0;
            double N_D_err = 0;

            if (WeightSumError) {
                N_A_err = th1d_ABCD->GetBinError(1);
                N_B_err = th1d_ABCD->GetBinError(2);
                N_C_err = th1d_ABCD->GetBinError(3);
                N_D_err = th1d_ABCD->GetBinError(4);
            }
            else {
                N_A_err = std::sqrt(N_A);
                N_B_err = std::sqrt(N_B);
                N_C_err = std::sqrt(N_C);
                N_D_err = std::sqrt(N_D);
            }

            if (N_D == 0) {
                printf("[ABCDmethod] N_D is 0\n");
                exit(1);
            }

            printf("N_A = %lf+-%lf\n", N_A, N_A_err);
            printf("N_B = %lf+-%lf\n", N_B, N_B_err);
            printf("N_C = %lf+-%lf\n", N_C, N_C_err);
            printf("N_D = %lf+-%lf\n", N_D, N_D_err);
            printf("estimated N_A = %lf+-%lf\n", N_B * (N_C / N_D), N_A * std::sqrt((N_B_err / N_B) * (N_B_err / N_B) + (N_C_err / N_C) * (N_C_err / N_C) + (N_D_err / N_D) * (N_D_err / N_D)));

            output_handle->clear();

            output_handle->push_back(N_A);
            output_handle->push_back(N_A_err);
            output_handle->push_back(N_B);
            output_handle->push_back(N_B_err);
            output_handle->push_back(N_C);
            output_handle->push_back(N_C_err);
            output_handle->push_back(N_D);
            output_handle->push_back(N_D_err);
            output_handle->push_back(N_B * (N_C / N_D));
            output_handle->push_back(N_A * std::sqrt((N_B_err / N_B) * (N_B_err / N_B) + (N_C_err / N_C) * (N_C_err / N_C) + (N_D_err / N_D) * (N_D_err / N_D)));

            if (validation) {
                double N_Aprime = th1d_ABCD_validation->GetBinContent(1);
                double N_Bprime = th1d_ABCD_validation->GetBinContent(2);
                double N_Cprime = th1d_ABCD_validation->GetBinContent(3);
                double N_Dprime = th1d_ABCD_validation->GetBinContent(4);

                double N_Aprime_err = 0;
                double N_Bprime_err = 0;
                double N_Cprime_err = 0;
                double N_Dprime_err = 0;

                if (WeightSumError) {
                    N_Aprime_err = th1d_ABCD_validation->GetBinError(1);
                    N_Bprime_err = th1d_ABCD_validation->GetBinError(2);
                    N_Cprime_err = th1d_ABCD_validation->GetBinError(3);
                    N_Dprime_err = th1d_ABCD_validation->GetBinError(4);
                }
                else {
                    N_Aprime_err = std::sqrt(N_Aprime);
                    N_Bprime_err = std::sqrt(N_Bprime);
                    N_Cprime_err = std::sqrt(N_Cprime);
                    N_Dprime_err = std::sqrt(N_Dprime);
                }

                if (N_Dprime == 0) {
                    printf("[ABCDmethod] N_Dprime is 0\n");
                    exit(1);
                }

                printf("N_A' = %lf+-%lf\n", N_Aprime, N_Aprime_err);
                printf("N_B' = %lf+-%lf\n", N_Bprime, N_Bprime_err);
                printf("N_C' = %lf+-%lf\n", N_Cprime, N_Cprime_err);
                printf("N_D' = %lf+-%lf\n", N_Dprime, N_Dprime_err);
                printf("estimated N_A' = %lf+-%lf\n", N_Bprime * (N_Cprime / N_Dprime), N_Aprime * std::sqrt((N_Bprime_err / N_Bprime) * (N_Bprime_err / N_Bprime) + (N_Cprime_err / N_Cprime) * (N_Cprime_err / N_Cprime) + (N_Dprime_err / N_Dprime) * (N_Dprime_err / N_Dprime)));

                output_handle->push_back(N_Aprime);
                output_handle->push_back(N_Aprime_err);
                output_handle->push_back(N_Bprime);
                output_handle->push_back(N_Bprime_err);
                output_handle->push_back(N_Cprime);
                output_handle->push_back(N_Cprime_err);
                output_handle->push_back(N_Dprime);
                output_handle->push_back(N_Dprime_err);
                output_handle->push_back(N_Bprime * (N_Cprime / N_Dprime));
                output_handle->push_back(N_Aprime * std::sqrt((N_Bprime_err / N_Bprime) * (N_Bprime_err / N_Bprime) + (N_Cprime_err / N_Cprime) * (N_Cprime_err / N_Cprime) + (N_Dprime_err / N_Dprime) * (N_Dprime_err / N_Dprime)));
            }

            delete th1d_ABCD;
            if (validation) { delete th1d_ABCD_validation; }
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(expression_A, variable_names));
            result.merge(GetVariablesFromExpression(expression_B, variable_names));
            result.merge(GetVariablesFromExpression(expression_C, variable_names));
            result.merge(GetVariablesFromExpression(expression_D, variable_names));

            if (validation) {
                result.merge(GetVariablesFromExpression(expression_Aprime, variable_names));
                result.merge(GetVariablesFromExpression(expression_Bprime, variable_names));
                result.merge(GetVariablesFromExpression(expression_Cprime, variable_names));
                result.merge(GetVariablesFromExpression(expression_Dprime, variable_names));
            }

            for (const std::vector<std::size_t>& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }

    };

    class AddWeight : public Module {
    private:
        std::string weight_name;
        // weight internal variable name -> Data variable name
        std::vector<std::pair<std::string, std::string>> variable_name_map;
        std::vector<std::size_t> variable_indices;

        bool DataStructureDefined;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        AddWeight(const char* weight_name_, const std::vector<std::pair<std::string, std::string>> variable_name_map_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_) : Module(), weight_name(weight_name_), variable_name_map(variable_name_map_), DataStructureDefined(*DataStructureDefined_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), internal_value(internal_value_) {
            EventWeight* eventweight = EventWeights::GetWeight(weight_name);

            eventweights_->push_back(eventweight);
            if(DataStructureDefined){
                std::vector<std::size_t> variable_indices;
                std::vector<std::string> variable_names_eventweight = eventweight->GetVarNames();

                for(int i = 0; i < variable_names_eventweight.size(); i++){
                    std::string variable_name_eventweight = variable_names_eventweight.at(i);
                    bool IsFound = false;
                    std::string variable_name_Data;
                    for(int j = 0; j < variable_name_map_.size(); j++){
                        if(variable_name_eventweight == variable_name_map_.at(j).first){
                            variable_name_Data = variable_name_map_.at(j).second;
                            IsFound = true;
                            break;
                        }
                    }
                    if(!IsFound){
                        printf("[AddWeight] Variable %s is not found in eventweight %s\n", variable_name_eventweight.c_str(), weight_name.c_str());
                        exit(1);
                    }
                    else{
                        std::vector<std::string>::iterator iter = std::find(variable_names.begin(), variable_names.end(), variable_name_Data);
                        if (iter != variable_names.end()) {
                            variable_indices.push_back(iter - variable_names.begin());
                        }
                        else {
                            printf("[AddWeight] Variable %s is not found in file\n", variable_name_Data.c_str());
                            exit(1);
                        }
                    }
                }

                variable_indices_list_->push_back(variable_indices);
            }
            else {
                printf("[AddWeight] ROOT file structure is unknown. Please load ROOT files before calling `AddWeight`\n");
                exit(1);
            }

            // copy event weights
            eventweights = (*eventweights_);
            variable_indices_list = (*variable_indices_list_);
        }
        ~AddWeight() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineObservable : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        std::string title;
        double minimum;
        double maximum;
        std::string unit;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineObservable(const std::string& id_, const std::string& title_, double minimum_, double maximum_, const std::string& unit_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), title(title_), minimum(minimum_), maximum(maximum_), unit(unit_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineObservable() {}

        void Start() {
            fitmanager->DefineObservable(id, title, minimum, maximum, unit);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineAndFillDataSet : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        std::vector<std::string> observable_ids;
        std::vector<std::string> equations;
        std::string category_id;
        std::vector<std::pair<std::string, std::string>> state_conditions;
        RooCategory* category = nullptr;
        std::vector<std::vector<Token>> postfix_exprs;
        std::vector<RooRealVar*> roorealvars;
        RooAbsData* roodata;
        std::vector<std::vector<Token>> state_condition_postfix_exprs;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), observable_ids(observable_ids_), equations(expressions_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {
            if (observable_ids.size() != expressions.size()) {
                printf("[DefineAndFillDataSet] The number of observable ids and expressions should be the same\n");
                exit(1);
            }
        }
        DefineAndFillDataSet(const std::string& id_, const std::vector<std::string> observable_ids_, const std::vector<std::string> expressions_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_conditions_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), observable_ids(observable_ids_), equations(expressions_), category_id(category_id_), state_conditions(state_conditions_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {
            if (observable_ids.size() != expressions.size()) {
                printf("[DefineAndFillDataSet] The number of observable ids and expressions should be the same\n");
                exit(1);
            }
        }
        ~DefineAndFillDataSet() {}

        void Start() {
            std::vector<std::string> dataset_variable_ids = observable_ids;
            if (!category_id.empty()) dataset_variable_ids.push_back(category_id);

            fitmanager->DefineDataSet(id, dataset_variable_ids);
            roodata = fitmanager->GetData(id);
            for (const std::string& observable_id : observable_ids) {
                RooRealVar* temp_roorealvar = fitmanager->GetRooRealVar(observable_id);
                roorealvars.push_back(temp_roorealvar);
            }
            if (!category_id.empty()) category = fitmanager->GetRooCategory(category_id);

            for (int i = 0; i < equations.size(); i++) {
                std::string replaced_expr = replaceInternalValues(equations.at(i), internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }

            for (const auto& [state, condition] : state_conditions) {
                std::string replaced_expr = replaceInternalValues(condition, internal_value);
                replaced_expr = replaceVariables(replaced_expr, &variable_names);
                state_condition_postfix_exprs.push_back(PostfixExpression(replaced_expr, &VariableTypes));
            }
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                std::vector<double> results;
                for (int i = 0; i < postfix_exprs.size(); i++) {
                    double result = EvaluatePostfixExpression(postfix_exprs.at(i), iter->variable, &VariableTypes);
                    results.push_back(result);
                }

                // set values
                for (int i = 0; i < results.size(); i++) {
                    roorealvars.at(i)->setVal(results.at(i));
                }

                // set category
                if (category != nullptr) {
                    int matched_state = -1;
                    for (int i = 0; i < state_condition_postfix_exprs.size(); i++) {
                        double result = EvaluatePostfixExpression(state_condition_postfix_exprs.at(i), iter->variable, &VariableTypes);
                        if (result > 0.5) {
                            if (matched_state != -1) {
                                printf("[DefineAndFillDataSet] event matches multiple category states.\n");
                                exit(1);
                            }
                            matched_state = i;
                        }
                    }

                    if (matched_state == -1) {
                        ++iter;
                        continue;
                    }

                    category->setLabel(state_conditions.at(matched_state).first.c_str());
                }

                // fill dataset
                RooArgSet row;
                for (RooRealVar* roorealvar : roorealvars) {
                    row.add(*roorealvar);
                }

                // fill category
                if (category != nullptr) row.add(*category);

                roodata->add(row, totalweight);

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpressions(equations, variable_names));
            for (const auto& [state, condition] : state_conditions) {
                result.merge(GetVariablesFromExpression(condition, variable_names));
            }

            for (const auto& variable_indices : variable_indices_list) {
                for (const std::size_t variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class DefineFitParameter : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        std::string title;
        double init_value;
        double minimum;
        double maximum;
        std::string unit;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), title(title_), init_value(init_value_), minimum(minimum_), maximum(maximum_), unit(unit_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineFitParameter() {}

        void Start() {
            fitmanager->DefineFitParameter(id, title, init_value, minimum, maximum, unit);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineConstantParameter : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        std::string title;
        double value;
        std::string unit;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), title(title_), value(value_), unit(unit_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineConstantParameter() {}

        void Start() {
            fitmanager->DefineConstantParameter(id, title, value, unit);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineCategory : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        std::string title;
        std::vector<std::string> states;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::string>& states_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), title(title_), states(states_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineCategory() {}

        void Start() {
            fitmanager->DefineCategory(id, title, states);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineAndFillProfile : public Module {
    private:
        FitManager* fitmanager;
        std::string profile_id;
        std::string title;
        int bins;
        double xmin;
        double xmax;
        double ymin;
        double ymax;
        std::string equation_x;
        std::string replaced_expr_x;
        std::vector<Token> postfix_expr_x;
        std::string equation_y;
        std::string replaced_expr_y;
        std::vector<Token> postfix_expr_y;
        TProfile* tprofile;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineAndFillProfile(const std::string& profile_id_, const std::string& title_, int bins_, double xmin_, double xmax_, double ymin_, double ymax_, std::string equation_x_, std::string equation_y_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), profile_id(profile_id_), title(title_), bins(bins_), xmin(xmin_), xmax(xmax_), ymin(ymin_), ymax(ymax_), equation_x(equation_x_), equation_y(equation_y_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineAndFillProfile() {}

        void Start() {
            replaced_expr_x = replaceInternalValues(equation_x, internal_value);
            replaced_expr_x = replaceVariables(replaced_expr_x, &variable_names);
            postfix_expr_x = PostfixExpression(replaced_expr_x, &VariableTypes);

            replaced_expr_y = replaceInternalValues(equation_y, internal_value);
            replaced_expr_y = replaceVariables(replaced_expr_y, &variable_names);
            postfix_expr_y = PostfixExpression(replaced_expr_y, &VariableTypes);

            fitmanager->DefineProfile(profile_id, title, bins, xmin, xmax, ymin, ymax);
            tprofile = fitmanager->GetProfile(profile_id);
        }

        int Process(std::deque<Data>* data) override {
            for (std::deque<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double totalweight = 1;
                for (int weightIdx = 0; weightIdx < eventweights.size(); weightIdx++) {
                    EventWeight* eventweight = eventweights.at(weightIdx);
                    const std::vector<std::size_t>& variable_indices = variable_indices_list.at(weightIdx);
                    totalweight = totalweight * eventweight->Evaluate(*iter, variable_indices);
                }

                double result_x = EvaluatePostfixExpression(postfix_expr_x, iter->variable, &VariableTypes);
                double result_y = EvaluatePostfixExpression(postfix_expr_y, iter->variable, &VariableTypes);

                tprofile->Fill(result_x, result_y, totalweight);

                ++iter;
            }

            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            std::set<std::string> result;

            result.merge(GetVariablesFromExpression(equation_x, variable_names));
            result.merge(GetVariablesFromExpression(equation_y, variable_names));

            for (const auto& variable_indices : variable_indices_list) {
                for (const std::size_t& variable_index : variable_indices) {
                    result.merge(GetVariablesFromExpression(variable_names.at(variable_index), variable_names));
                }
            }

            return result;
        }
    };

    class SetParameterConstant : public Module {
    private:
        FitManager* fitmanager;
        std::string id;
        bool constant;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        SetParameterConstant(const std::string& id_, bool constant_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), id(id_), constant(constant_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~SetParameterConstant() {}

        void Start() {
            fitmanager->SetParameterConstant(id, constant);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class SetRange : public Module {
    private:
        FitManager* fitmanager;
        std::string variable_id;
        std::string range_name;
        double minimum;
        double maximum;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), variable_id(variable_id_), range_name(range_name_), minimum(minimum_), maximum(maximum_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~SetRange() {}

        void Start() {
            fitmanager->SetRange(variable_id, range_name, minimum, maximum);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineModel : public Module {
    private:
        FitManager* fitmanager;
        std::string model_id;
        std::string model_type;
        std::vector<std::string> observable_ids;
        std::vector<std::string> parameter_ids;
        ModelOptions options;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), model_id(model_id_), model_type(model_type_), observable_ids(observable_ids_), parameter_ids(parameter_ids_), options(options_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineModel() {}

        void Start() {
            fitmanager->DefineModel(model_id, model_type, observable_ids, parameter_ids, options);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineAddModel : public Module {
    private:
        FitManager* fitmanager;
        std::string model_id;
        std::vector<std::string> pdf_ids;
        std::vector<std::string> coefficient_ids;
        bool recursive_fractions;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineAddModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, const std::vector<std::string>& coefficient_ids_, bool recursive_fractions_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), model_id(model_id_), pdf_ids(pdf_ids_), coefficient_ids(coefficient_ids_), recursive_fractions(recursive_fractions_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineAddModel() {}

        void Start() {
            fitmanager->DefineAddModel(model_id, pdf_ids, coefficient_ids, recursive_fractions);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineProductModel : public Module {
    private:
        FitManager* fitmanager;
        std::string model_id;
        std::vector<std::string> pdf_ids;
        double cutoff;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineProductModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, double cutoff_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), model_id(model_id_), pdf_ids(pdf_ids_), cutoff(cutoff_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineProductModel() {}

        void Start() {
            fitmanager->DefineProductModel(model_id, pdf_ids, cutoff);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineGenericModel : public Module {
    private:
        FitManager* fitmanager;
        std::string model_id;
        std::string expression;
        std::vector<std::string> argument_ids;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineGenericModel(const std::string& model_id_, const std::string& expression_, const std::vector<std::string>& argument_ids_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), model_id(model_id_), expression(expression_), argument_ids(argument_ids_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineGenericModel() {}

        void Start() {
            fitmanager->DefineGenericModel(model_id, expression, argument_ids);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineSimultaneousModel : public Module {
    private:
        FitManager* fitmanager;
        std::string model_id;
        std::string category_id;
        std::vector<std::pair<std::string, std::string>> state_pdf_ids;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineSimultaneousModel(const std::string& model_id_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_pdf_ids_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), model_id(model_id_), category_id(category_id_), state_pdf_ids(state_pdf_ids_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineSimultaneousModel() {}

        void Start() {
            fitmanager->DefineSimultaneousModel(model_id, category_id, state_pdf_ids);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class DefineTF1 : public Module {
    private:
        FitManager* fitmanager;
        std::string function_id;
        std::string formula;
        double xmin;
        double xmax;
        std::vector<TF1ParameterDefinition>& parameters;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        DefineTF1(const std::string& function_id_, const std::string& formula_, double xmin_, double xmax_, const std::vector<TF1ParameterDefinition>& parameters_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), function_id(function_id_), formula(formula_), xmin(xmin_), xmax(xmax_), parameters(parameters_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~DefineTF1() {}

        void Start() {
            fitmanager->DefineTF1(function_id, formula, xmin, xmax, parameters);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class Fit : public Module {
    private:
        FitManager* fitmanager;
        std::string fit_id;
        std::string dataset_id;
        std::string model_id;
        FitOptions options;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        Fit(const std::string& fit_id_, const std::string dataset_id_, const std::string& model_id_, const FitOptions& options_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), fit_id(fit_id_), dataset_id(dataset_id_), model_id(model_id_), options(options_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~Fit() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->Fit(fit_id, dataset_id, model_id, options);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }

        bool BlocksDownstream() const override {
            return true;
        }
    };

    class PlotFit : public Module {
    private:
        FitManager* fitmanager;
        std::string fit_id;
        std::string observable_id;
        std::string plot_name;
        std::string category_state;
        FitPlotOptions options;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const FitPlotOptions& options_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), fit_id(fit_id_), observable_id(observable_id_), plot_name(plot_name_), options(options_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        PlotFit(const std::string& fit_id_, const std::string& observable_id_, const std::string& plot_name_, const std::string& category_state_, const FitPlotOptions& options_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), fit_id(fit_id_), observable_id(observable_id_), plot_name(plot_name_), category_state(category_state_), options(options_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~PlotFit() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            if(category_state.empty()) fitmanager->PlotFit(fit_id, observable_id, plot_name, options);
            else fitmanager->PlotFit(fit_id, observable_id, plot_name, category_state, options);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class ExportFitResult : public Module {
    private:
        FitManager* fitmanager;
        std::string filename;
        std::vector<std::string> fit_ids;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        ExportFitResult(const std::string& filename_, const std::vector<std::string>& fit_ids_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), filename(filename_), fit_ids(fit_ids_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~ExportFitResult() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->ExportFitResult(filename, fit_ids);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class CreateNLL : public Module {
    private:
        FitManager* fitmanager;
        std::string nll_id;
        std::string dataset_id;
        std::string model_id;
        FitOptions options;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        CreateNLL(const std::string& nll_id_, const std::string& dataset_id_, const std::string& model_id_, const FitOptions& options_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), nll_id(nll_id_), dataset_id(dataset_id_), model_id(model_id_), options(options_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~CreateNLL() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->CreateNLL(nll_id, dataset_id, model_id, options);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class PlotNLL : public Module {
    private:
        FitManager* fitmanager;
        std::string nll_id;
        std::string parameter_id;
        std::string plot_name;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        PlotNLL(const std::string& nll_id_, const std::string& parameter_id_, const std::string& plot_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), nll_id(nll_id_), parameter_id(parameter_id_), plot_name(plot_name_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~PlotNLL() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->PlotNLL(nll_id, parameter_id, plot_name);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class PlotProfileNLL : public Module {
    private:
        FitManager* fitmanager;
        std::string nll_id;
        std::string poi_id;
        std::string plot_name;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        PlotProfileNLL(const std::string& nll_id_, const std::string& poi_id_, const std::string& plot_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), nll_id(nll_id_), poi_id(poi_id_), plot_name(plot_name_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~PlotProfileNLL() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->PlotProfileNLL(nll_id, poi_id, plot_name);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class SaveWorkspace : public Module {
    private:
        FitManager* fitmanager;
        std::string filename;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        SaveWorkspace(const std::string& filename_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), filename(filename_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~SaveWorkspace() {}

        void Start() {}

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {
            fitmanager->SaveWorkspace(filename);
        }

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

    class LoadWorkspace : public Module {
    private:
        FitManager* fitmanager;
        std::string filename;
        std::string workspace_name;

        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;
        std::vector<EventWeight*> eventweights;
        std::vector<std::vector<std::size_t>> variable_indices_list;
        std::map<std::string, double> internal_value;

    public:
        LoadWorkspace(const std::string& filename_, const std::string& workspace_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_, std::vector<EventWeight*>* eventweights_, std::vector<std::vector<std::size_t>>* variable_indices_list_, std::map<std::string, double>* internal_value_, FitManager* fitmanager_) : Module(), filename(filename_), workspace_name(workspace_name_), variable_names(*variable_names_), VariableTypes(*VariableTypes_), eventweights(*eventweights_), variable_indices_list(*variable_indices_list_), internal_value(*internal_value_), fitmanager(fitmanager_) {}
        ~LoadWorkspace() {}

        void Start() {
            fitmanager->LoadWorkspace(filename, workspace_name);
        }

        int Process(std::deque<Data>* data) override {
            return 1;
        }

        void End() override {}

        std::optional<std::set<std::string>> RequiredVariables() const override {
            return std::set<std::string>{};
        }
    };

}

#endif 

