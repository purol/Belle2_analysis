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
        * `Start` function is called just after the data structure is determined. It is called only one time.
        */
        virtual void Start() = 0;
        /*
        * `Process` function is called every time for each ROOT file.
        */
        virtual void Process(std::vector<Data>* data) = 0;
        /*
        * `End` function is called after all ROOT files are read. It is called only once.
        */
        virtual void End() = 0;
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

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                if (result < 0.5) data->erase(iter);
                else ++iter;
            }
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

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                Ncandidate = Ncandidate + 1.0;
                ++iter;
            }
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

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                hist->Fill(result);
                ++iter;
            }
        }

        TH1D* SaveTH1D() {
            return hist;
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

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double x_result = evaluateExpression(x_replaced_expr, iter->variable, VariableTypes);
                double y_result = evaluateExpression(y_replaced_expr, iter->variable, VariableTypes);
                hist->Fill(x_result, y_result);
                ++iter;
            }
        }

        TH2D* SaveTH2D() {
            return hist;
        }

        void End() override {
            TCanvas* c_temp = new TCanvas("c", "", 800, 800); c_temp->cd();
            hist->SetStats(false);
            hist->Draw("COLZ");
            c_temp->SaveAs(png_name.c_str());
            delete c_temp;
        }

    };

}

#endif 

