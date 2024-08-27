#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>
#include <algorithm>

#include "data.h"
#include "string_equation.h"
#include "base.h"

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
        std::string category;

        // temporary variable to extract data from branch
        std::vector<std::variant<int, unsigned int, float, double>> temp_variable;

        bool* DataStructureDefined;
        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
    public:
        Load(const char* dirname_, const char* including_string_, const char* category_, bool* DataStructureDefined_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), dirname(dirname_), category(category_), DataStructureDefined(DataStructureDefined_), variable_names(variable_names_), VariableTypes(VariableTypes_) {
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

                Data temp = { temp_variable, category, filename.at(Currententry) };
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
                Ncandidate = Ncandidate + 1.0;
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
        int nbins;
        double x_low;
        double x_high;

        std::vector<std::string>* variable_names;
        std::vector<std::string>* VariableTypes;
        std::string expression;
        std::string replaced_expr;

        std::string png_name;
    public:
        DrawTH1D(const char* expression_, const char* hist_title_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), expression(expression_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_)
        {
            std::string hist_name = generateRandomString(12);
            hist = new TH1D(hist_name.c_str(), hist_title_, nbins, x_low, x_high);
        }
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
                hist->Fill(result);
                ++iter;
            }

            return 1;
        }

        void End() override {
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
    public:
        DrawTH2D(const char* x_expression_, const char* y_expression_, const char* hist_title_, int x_nbins_, double x_low_, double x_high_, int y_nbins_, double y_low_, double y_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), x_expression(x_expression_), y_expression(y_expression_), x_nbins(x_nbins_), x_low(x_low_), x_high(x_high_), y_nbins(y_nbins_), y_low(y_low_), y_high(y_high_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_)
        {
            std::string hist_name = generateRandomString(12);
            hist = new TH2D(hist_name.c_str(), hist_title_, x_nbins, x_low, x_high, y_nbins, y_low, y_high);
        }
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
                hist->Fill(x_result, y_result);
                ++iter;
            }

            return 1;
        }

        void End() override {
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

}

#endif 

