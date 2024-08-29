#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>
#include <algorithm>
#include <limits>

#include "data.h"
#include "string_equation.h"
#include "base.h"

#include <TGraph.h>

double reserve_function() {
    return 1.0;
}

namespace Module {

    class Module {
    public:
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
        std::vector<std::variant<int, unsigned int, float, double>> temp_variable;

        bool* DataStructureDefined;
        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
    public:
        Load(const char* dirname_, const char* including_string_, const char* label_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), dirname(dirname_), label(label_), DataStructureDefined(DataStructureDefined_), variable_names(variable_names_), VariableTypes(VariableTypes_) {
            // load file list and initialize entry counter
            load_files(dirname.c_str(), &filename, including_string_);
            Nentry = filename.size();
            Currententry = 0;

            // check data structure
            for (int i = 0; i < Nentry; i++) {
                TFile* input_file = new TFile((dirname + std::string("/") + filename.at(i)).c_str(), "read");

                // read tree
                TTree* temp_tree = (TTree*)input_file->Get(TREE);

                // read list of branches
                TObjArray* temp_branchList = temp_tree->GetListOfBranches();

                // read/check name of branches and their type
                if ((*DataStructureDefined) == false) {
                    for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                        const char* temp_branch_name = temp_branchList->At(j)->GetName();
                        const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();

                        variable_names->push_back(temp_branch_name);
                        VariableTypes->push_back(std::string(TypeName));
                    }
                    (*DataStructureDefined) = true;
                }
                else {
                    for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                        const char* temp_branch_name = temp_branchList->At(j)->GetName();
                        const char* TypeName = temp_tree->FindLeaf(temp_branch_name)->GetTypeName();

                        if (variable_names->at(j) != std::string(temp_branch_name)) {
                            printf("variable name is different: %s %s\n", variable_names->at(j).c_str(), temp_branch_name);
                            exit(1);
                        }
                        else if (VariableTypes->at(j) != std::string(TypeName)) {
                            printf("type is different: %s %s\n", VariableTypes->at(j).c_str(), TypeName);
                            exit(1);
                        }
                    }
                }

                input_file->Close();
            }
        }
        ~Load() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes->size(); i++) {
                if (strcmp(VariableTypes->at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
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
            TTree* temp_tree = (TTree*)input_file->Get(TREE);

            // set branch addresses
            for (int j = 0; j < temp_tree->GetNbranches(); j++) {
                if (strcmp(VariableTypes->at(j).c_str(), "Double_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names->at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "Int_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names->at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "UInt_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names->at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "Float_t") == 0) {
                    temp_tree->SetBranchAddress(variable_names->at(j).c_str(), &std::get<float>(temp_variable.at(j)));
                }
            }

            // fill Data vector
            for (unsigned int j = 0; j < temp_tree->GetEntries(); j++) {
                temp_tree->GetEntry(j);

                Data temp = { temp_variable, label, filename.at(Currententry) };
                data->push_back(temp);
            }

            input_file->Close();
            Currententry++;
            return 0;
        }

        void End() override {}
    };

    class Cut : public Module {
    private:
        std::string cut_string;
        std::string replaced_expr;
        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;

    public:
        Cut(const char* cut_string_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), cut_string(cut_string_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}
        ~Cut() {}

        void Start() {
            replaced_expr = replaceVariables(cut_string, variable_names);
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                if (result < 0.5) data->erase(iter);
                else ++iter;
            }

            return 1;
        }

        void End() override {}
    };

    class PrintInformation : public Module {
    private:
        std::string print_string;
        double Ncandidate;
    public:
        PrintInformation(const char* print_string_) : Module(), print_string(print_string_), Ncandidate(0){}
        ~PrintInformation() {}

        void Start() override {}

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                Ncandidate = Ncandidate + reserve_function();
                ++iter;
            }

            return 1;
        }

        void End() override {
            printf("%s\n", print_string.c_str());
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

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
        std::string expression;
        std::string replaced_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> weight;
    public:
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}
        DrawTH1D(const char* expression_, const char* hist_title_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), hist_title(hist_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~DrawTH1D() {
            delete hist;
        }

        void Start() override {
            // change variable name into placeholder
            replaced_expr = replaceVariables(expression, variable_names);
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                x_variable.push_back(result);
                weight.push_back(reserve_function());

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
            std::string hist_name = generateRandomString(12);
            hist = new TH1D(hist_name.c_str(), hist_title.c_str(), nbins, x_low, x_high);

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                hist->Fill(x_variable.at(i), weight.at(i));
            }

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            weight.clear();

            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            hist->SetStats(false);
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

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
        std::string x_expression;
        std::string x_replaced_expr;
        std::string y_expression;
        std::string y_replaced_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> y_variable;
        std::vector<double> weight;
    public:
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(x_nbins_), x_low(x_low_), x_high(x_high_), y_nbins(y_nbins_), y_low(y_low_), y_high(y_high_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), x_expression(x_expression_), y_expression(y_expression_), hist_title(hist_title_), x_nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), y_nbins(50), y_low(std::numeric_limits<double>::max()), y_high(std::numeric_limits<double>::max()), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~DrawTH2D() {
            delete hist;
        }

        void Start() override {
            // change variable name into placeholder
            x_replaced_expr = replaceVariables(x_expression, variable_names);
            y_replaced_expr = replaceVariables(y_expression, variable_names);
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double x_result = evaluateExpression(x_replaced_expr, iter->variable, VariableTypes);
                double y_result = evaluateExpression(y_replaced_expr, iter->variable, VariableTypes);
                x_variable.push_back(x_result);
                y_variable.push_back(y_result);
                weight.push_back(reserve_function());

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
            std::string hist_name = generateRandomString(12);
            hist = new TH2D(hist_name.c_str(), hist_title.c_str(), x_nbins, x_low, x_high, y_nbins, y_low, y_high);

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                hist->Fill(x_variable.at(i), y_variable.at(i), weight.at(i));
            }

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            y_variable.clear();
            weight.clear();

            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            hist->SetStats(false);
            hist->Draw("COLZ");
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
        std::vector<std::variant<int, unsigned int, float, double>> temp_variable;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
    public:
        PrintSeparateRootFile(const char* path_, const char* prefix_, const char* suffix_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), path(path_), prefix(prefix_), suffix(suffix_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~PrintSeparateRootFile() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes->size(); i++) {
                if (strcmp(VariableTypes->at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
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
                    temp_tree = new TTree(TREE, "");

                    // set Branch
                    for (int j = 0; j < VariableTypes->size(); j++) {
                        if (strcmp(VariableTypes->at(j).c_str(), "Double_t") == 0) {
                            temp_tree->Branch(variable_names->at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes->at(j).c_str(), "Int_t") == 0) {
                            temp_tree->Branch(variable_names->at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes->at(j).c_str(), "UInt_t") == 0) {
                            temp_tree->Branch(variable_names->at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                        }
                        else if (strcmp(VariableTypes->at(j).c_str(), "Float_t") == 0) {
                            temp_tree->Branch(variable_names->at(j).c_str(), &std::get<float>(temp_variable.at(j)));
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
        std::vector<std::variant<int, unsigned int, float, double>> temp_variable;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
    public:
        PrintRootFile(const char* output_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), output_name(output_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~PrintRootFile() {}

        void Start() override {
            // fill `temp_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < VariableTypes->size(); i++) {
                if (strcmp(VariableTypes->at(i).c_str(), "Double_t") == 0) {
                    temp_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Int_t") == 0) {
                    temp_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "UInt_t") == 0) {
                    temp_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes->at(i).c_str(), "Float_t") == 0) {
                    temp_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                    exit(1);
                }
            }

            temp_file = new TFile(output_name.c_str(), "recreate");
            temp_file->cd();
            temp_tree = new TTree(TREE, "");

            // set Branch
            for (int j = 0; j < VariableTypes->size(); j++) {
                if (strcmp(VariableTypes->at(j).c_str(), "Double_t") == 0) {
                    temp_tree->Branch(variable_names->at(j).c_str(), &std::get<double>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "Int_t") == 0) {
                    temp_tree->Branch(variable_names->at(j).c_str(), &std::get<int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "UInt_t") == 0) {
                    temp_tree->Branch(variable_names->at(j).c_str(), &std::get<unsigned int>(temp_variable.at(j)));
                }
                else if (strcmp(VariableTypes->at(j).c_str(), "Float_t") == 0) {
                    temp_tree->Branch(variable_names->at(j).c_str(), &std::get<float>(temp_variable.at(j)));
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
        std::vector<std::variant<int, unsigned int, float, double>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        std::string replaced_expr;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;

        static char to_upper(char c) {
            return std::toupper(static_cast<unsigned char>(c));
        }
    public:
        BCS(const char* equation_, const char* criteria_, const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), criteria(criteria_), Event_variable_list(Event_variable_list_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}
        
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
                int event_variable_index = std::find(variable_names->begin(), variable_names->end(), Event_variable_list.at(i)) - variable_names->begin();

                if (event_variable_index == variable_names->size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                    exit(1);
                }
            }

            replaced_expr = replaceVariables(equation, variable_names);
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
            int selected_index = -1;

            // initialize previous event variable
            std::vector<std::variant<int, unsigned int, float, double>> previous_event_variable = temp_event_variable;
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = event_variable_index_list.at(i);

                if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Double_t") == 0) {
                    previous_event_variable.at(i) = -std::numeric_limits<double>::max();
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Int_t") == 0) {
                    previous_event_variable.at(i) = -std::numeric_limits<int>::max();
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "UInt_t") == 0) {
                    previous_event_variable.at(i) = std::numeric_limits<unsigned int>::max();
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Float_t") == 0) {
                    previous_event_variable.at(i) = -std::numeric_limits<float>::max();
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                    exit(1);
                }
            }

            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                        exit(1);
                    }
                }

                // if event variable changes, do BCS
                if (previous_event_variable != temp_event_variable) {
                    if (selected_index != -1) {
                        Data temp = temp_data.at(selected_index);
                        temp_data_after_BCS.push_back(temp);
                        temp_data.clear();

                        // reset extreme value/index
                        if (criteria == "HIGHEST") extreme_value = -std::numeric_limits<double>::max();
                        else if (criteria == "LOWEST") extreme_value = std::numeric_limits<double>::max();
                        else {
                            printf("criteria for BCS should be `highest` or `lowest`\n");
                            exit(1);
                        }
                        selected_index = -1;
                    }
                }

                // get BCS variable
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                
                // check the BCS criteria
                if (criteria == "HIGHEST") {
                    if (result > extreme_value) {
                        extreme_value = result;
                        selected_index = temp_data.size();
                    }
                }
                else if (criteria == "LOWEST") {
                    if (result < extreme_value) {
                        extreme_value = result;
                        selected_index = temp_data.size();
                    }
                }

                // get Data
                temp_data.push_back(*iter);
                data->erase(iter);

                previous_event_variable = temp_event_variable;

            }

            // do BCS for the final dataset
            if (selected_index != -1) {
                Data temp = temp_data.at(selected_index);
                temp_data_after_BCS.push_back(temp);
                temp_data.clear();

                // reset extreme value/index
                if (criteria == "HIGHEST") extreme_value = -std::numeric_limits<double>::max();
                else if (criteria == "LOWEST") extreme_value = std::numeric_limits<double>::max();
                else {
                    printf("criteria for BCS should be `highest` or `lowest`\n");
                    exit(1);
                }
                selected_index = -1;
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
        std::vector<std::variant<int, unsigned int, float, double>> temp_event_variable;

        // index of event variables in `variable_names`
        std::vector<int> event_variable_index_list;

        // event variable history
        std::vector<std::vector<std::variant<int, unsigned int, float, double>>> history_event_variable;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;

    public:
        IsBCSValid(const std::vector<std::string> Event_variable_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), Event_variable_list(Event_variable_list_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~IsBCSValid() {}

        void Start() override {
            // exception handling
            if (Event_variable_list.size() == 0) {
                printf("event variable for IsBCSValid should exist.\n");
                exit(1);
            }

            // fill `temp_event_variable` by dummy value. It is to set variable type beforehand.
            for (int i = 0; i < Event_variable_list.size(); i++) {
                int event_variable_index = std::find(variable_names->begin(), variable_names->end(), Event_variable_list.at(i)) - variable_names->begin();

                if (event_variable_index == variable_names->size()) {
                    printf("cannot find variable: %s\n", Event_variable_list.at(i).c_str());
                    exit(1);
                }

                event_variable_index_list.push_back(event_variable_index);

                if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Double_t") == 0) {
                    temp_event_variable.push_back(static_cast<double>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Int_t") == 0) {
                    temp_event_variable.push_back(static_cast<int>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "UInt_t") == 0) {
                    temp_event_variable.push_back(static_cast<unsigned int>(0.0));
                }
                else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Float_t") == 0) {
                    temp_event_variable.push_back(static_cast<float>(0.0));
                }
                else {
                    printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                    exit(1);
                }
            }
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                // get event variable
                for (int i = 0; i < Event_variable_list.size(); i++) {
                    int event_variable_index = event_variable_index_list.at(i);

                    if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Double_t") == 0) {
                        temp_event_variable.at(i) = std::get<double>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Int_t") == 0) {
                        temp_event_variable.at(i) = std::get<int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "UInt_t") == 0) {
                        temp_event_variable.at(i) = std::get<unsigned int>(iter->variable.at(event_variable_index));
                    }
                    else if (strcmp(VariableTypes->at(event_variable_index).c_str(), "Float_t") == 0) {
                        temp_event_variable.at(i) = std::get<float>(iter->variable.at(event_variable_index));
                    }
                    else {
                        printf("unexpected data type: %s\n", VariableTypes->at(i).c_str());
                        exit(1);
                    }
                }

                if (std::find(history_event_variable.begin(), history_event_variable.end(), temp_event_variable) == history_event_variable.end()) {
                    history_event_variable.push_back(temp_event_variable);
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

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;

        std::string png_name;

        double MyEPSILON;
    public:
        DrawFOM(const char* equation_, double MIN_, double MAX_, const char* png_name_, std::vector<std::string> Signal_label_list_, std::vector<std::string> Background_label_list_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), equation(equation_), MIN(MIN_), MAX(MAX_), png_name(png_name_), Signal_label_list(Signal_label_list_), Background_label_list(Background_label_list_), variable_names(variable_names_), VariableTypes(VariableTypes_) {
            // just 50
            NBin = 50;

            // just 0.000001
            MyEPSILON = 0.000001;
        }

        ~DrawFOM() {}

        void Start() {
            // change variable name into placeholder
            replaced_expr = replaceVariables(equation, variable_names);

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
                    double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                    if (result > variable_value) DoesItPassCriteria = true;
                    else DoesItPassCriteria = false;

                    if (DoesItPassCriteria) {
                        if (std::find(Signal_label_list.begin(), Signal_label_list.end(), iter->label) != Signal_label_list.end()) NSIGs[i] = NSIGs[i] + reserve_function();
                        if (std::find(Background_label_list.begin(), Background_label_list.end(), iter->label) != Background_label_list.end()) NBKGs[i] = NBKGs[i] + reserve_function();
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

    class DrawStack : public Module {
    private:
        THStack* stack;
        std::string stack_title;
        int nbins;
        double x_low;
        double x_high;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
        std::string expression;
        std::string replaced_expr;

        std::string png_name;

        std::vector<double> x_variable;
        std::vector<double> weight;
        std::vector<std::string> label;

        std::vector<std::string> selected_label;
    public:
        DrawStack(const char* expression_, const char* stack_title_, int nbins_, double x_low_, double x_high_, std::vector<std::string> selected_label_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(nbins_), x_low(x_low_), x_high(x_high_), selected_label(selected_label_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}
        DrawStack(const char* expression_, const char* stack_title_, std::vector<std::string> selected_label_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), stack_title(stack_title_), nbins(50), x_low(std::numeric_limits<double>::max()), x_high(std::numeric_limits<double>::max()), selected_label(selected_label_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_) {}

        ~DrawStack() {
            delete stack;
        }

        void Start() override {
            // change variable name into placeholder
            replaced_expr = replaceVariables(expression, variable_names);
        }

        int Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                if (std::find(selected_label.begin(), selected_label.end(), iter->label) != selected_label.end()) {
                    x_variable.push_back(result);
                    weight.push_back(reserve_function());
                    label.push_back(iter->label);
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

            // create histogram
            TH1D** temp_hist;
            temp_hist = (TH1D**)malloc(sizeof(TH1D*) * selected_label.size());
            for (int i = 0; i < selected_label.size(); i++) {
                std::string hist_name = generateRandomString(12);
                temp_hist[i] = new TH1D(hist_name.c_str(), stack_title.c_str(), nbins, x_low, x_high);
            }

            // fill histogram
            for (int i = 0; i < weight.size(); i++) {
                int label_index = std::find(selected_label.begin(), selected_label.end(), label.at(i)) - selected_label.begin();
                temp_hist[label_index]->Fill(x_variable.at(i), weight.at(i));
            }

            // clear vector. Maybe not needed but to save memory...
            x_variable.clear();
            weight.clear();
            label.clear();

            // stack histogram
            for (int i = 0; i < selected_label.size(); i++) {
                stack->Add(temp_hist[i]);
            }

            // set palette
            gStyle->SetPalette(kPastel);

            // set maximum
            double ymax = -std::numeric_limits<double>::max();
            for (int i = 0; i < selected_label.size(); i++) {
                if (ymax < temp_hist[i]->GetMaximum()) ymax = temp_hist[i]->GetMaximum();
            }
            stack->SetMaximum(ymax * 1.1);

            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            stack->Draw("pfc Hist");
            c_temp->SaveAs(png_name.c_str());
            delete c_temp;

            delete[] temp_hist;
        }
    };

}

#endif 

