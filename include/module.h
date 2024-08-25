#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>
#include <stack>
#include <sstream>
#include <algorithm>

#include "data.h"

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

        double applyOp(double a, double b, const std::string& op) {
            if (op == "+") return (a + b);
            else if (op == "-") return (a - b);
            else if (op == "*") return (a * b);
            else if (op == "/") return (a / b);
            else if (op == "^") return std::pow(a, b);
            else if (op == "<") {
                if (a < b) return 1.0;
                else return 0.0;
            }
            else if (op == ">") {
                if (a > b) return 1.0;
                else return 0.0;
            }
            else if (op == "<=") {
                if (a <= b) return 1.0;
                else return 0.0;
            }
            else if (op == ">=") {
                if (a >= b) return 1.0;
                else return 0.0;
            }
            else if (op == "==") {
                if (a == b) return 1.0;
                else return 0.0;
            }
            else if (op == "!=") {
                if (a != b) return 1.0;
                else return 0.0;
            }
            else if (op == "&&") {
                if ((a != 0) && (b != 0)) return 1.0;
                else return 0.0;
            }
            else if (op == "||") {
                if ((a != 0) || (b != 0)) return 1.0;
                else return 0.0;
            }
            else {
                printf("unknown operator: %s\n", op.c_str());
                exit(1);
                return 1;
            }
        }

        double applyOp(double a, const std::string& op) {
            if (op == "\x03") return -a;
            else if (op == "\x04") return a;
            else {
                printf("unknown operator: %s\n", op.c_str());
                exit(1);
                return 1;
            }
        }

        int precedence(const std::string& op) {
            if (op == "||") return 1;
            if (op == "&&") return 2;
            if (op == "==" || op == "!=") return 3;
            if (op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
            if (op == "+" || op == "-") return 5;
            if (op == "*" || op == "/") return 6;
            if (op == "^") return 7;
            if (op == "\x03") return 8; // minus unary operator
            if (op == "\x04") return 9; // plus unary operator
            return 0;
        }

        double evaluateExpression(const std::string& replaced_expr_, const std::vector<std::variant<int, unsigned int, float, double>> variables_, const std::vector<std::string>* VariableTypes_) {
            std::istringstream iss(replaced_expr_);
            std::stack<double> values;
            std::stack<std::string> ops;

            // previous token is needed to check unary operator
            char previous_token;
            char token;
            while (iss >> token) {
                // check it is unary operator or not
                if (token == '-') {
                    if (std::isdigit(previous_token) || (previous_token == ')') || (previous_token == '\x02')) {} // it is subtraction
                    else token = '\x03'; // it is unary
                }
                else if (token == '+') {
                    if (std::isdigit(previous_token) || (previous_token == ')') || (previous_token == '\x02')) {} // it is addition
                    else token = '\x04'; // it is unary
                }

                if (std::isdigit(token) || (token == '.')) { // it is number
                    if (token == '.') {
                        char next_token;
                        iss >> next_token;
                        if (std::isdigit(next_token)) {
                            iss.putback(next_token);
                        }
                        else {
                            printf("unexpected `.` character\n");
                            exit(1);
                        }
                    }

                    iss.putback(token);
                    double value;
                    iss >> value;
                    values.push(value);

                }
                else if (token == '\x01') { // it is placeholder

                    int index;
                    iss >> index;

                    if (VariableTypes_->at(index) == "Double_t") {
                        values.push((double)std::get<double>(variables_.at(index)));
                    }
                    else if (VariableTypes_->at(index) == "Int_t") {
                        values.push((double)std::get<int>(variables_.at(index)));
                    }
                    else if (VariableTypes_->at(index) == "UInt_t") {
                        values.push((double)std::get<unsigned int>(variables_.at(index)));
                    }
                    else if (VariableTypes_->at(index) == "Float_t") {
                        values.push((double)std::get<float>(variables_.at(index)));
                    }
                    else {
                        printf("unexpected data type\n");
                        exit(1);
                    }

                    char next_token;
                    iss >> next_token;

                    if (next_token != '\x02') {
                        printf("placeholder is wrong\n");
                        exit(1);
                    }

                }
                else if (token == '(') {
                    ops.push(std::string(1, token));
                }
                else if (token == ')') {
                    while (!ops.empty() && ops.top() != '(') {
                        double b = values.top(); values.pop();
                        double a = values.top(); values.pop();
                        std::string op = ops.top(); ops.pop();
                        values.push(applyOp(a, b, op));

                        if (ops.empty()) {
                            printf("cannot find `(`\n");
                            exit(1);
                        }
                    }
                    ops.pop();
                }
                else if ((std::string("+-*/^<>=!&|") + std::string("\x03") + std::string("\x04")).find(token) != std::string::npos) {
                    std::string temp_operator;

                    switch (token) {
                    case '+':
                    case '-':
                    case '*':
                    case '/':
                    case '^':
                    {
                        temp_operator = std::string(1, token);
                        break;
                    }
                    case '<':
                    case '>':
                    {
                        char next_token;
                        iss >> next_token;
                        if (next_token != '=') {
                            temp_operator = std::string(1, token);
                            iss.putback(next_token);
                        }
                        else {
                            temp_operator = std::string(1, token) + std::string(1, next_token);
                        }
                        break;
                    }
                    case '=':
                    case '!':
                    {
                        char next_token;
                        iss >> next_token;
                        if (next_token != '=') {
                            printf("unknown operator: %c\n", token);
                            exit(1);
                        }
                        else {
                            temp_operator = std::string(1, token) + std::string(1, next_token);
                        }
                        break;
                    }
                    case '&':
                    case '|':
                    {
                        char next_token;
                        iss >> next_token;
                        if (next_token != token) {
                            printf("unknown operator: %c\n", token);
                            exit(1);
                        }
                        else {
                            temp_operator = std::string(1, token) + std::string(1, next_token);
                        }
                        break;
                    }
                    case '\x03':
                    case '\x04':
                    {
                        temp_operator = std::string(1, token);
                        break;
                    }
                    }

                    while (!ops.empty() && (precedence(ops.top()) >= precedence(temp_operator))) {
                        if ((ops.top() != '\x03') && (ops.top() != '\x04')) {
                            double b = values.top(); values.pop();
                            double a = values.top(); values.pop();
                            std::string op = ops.top(); ops.pop();
                            values.push(applyOp(a, b, op));
                        }
                        else {
                            double a = values.top(); values.pop();
                            std::string op = ops.top(); ops.pop();
                            values.push(applyOp(a, op));
                        }
                    }
                    ops.push(temp_operator);
                }
                else {
                    printf("unknown token: %c\n", token);
                    exit(1);
                }

                previous_token = token;
            }

            while (!ops.empty()) {
                if ((ops.top() != '\x03') && (ops.top() != '\x04')) {
                    if (values.size() < 2) {
                        printf("equation expression is wrong. Even though there is operator, the remaining number is not enough.\n");
                        exit(1);
                    }

                    double b = values.top(); values.pop();
                    double a = values.top(); values.pop();
                    std::string op = ops.top(); ops.pop();
                    values.push(applyOp(a, b, op));
                }
                else {
                    if (values.size() < 1) {
                        printf("equation expression is wrong. Even though there is operator, the remaining number is not enough.\n");
                        exit(1);
                    }

                    double a = values.top(); values.pop();
                    std::string op = ops.top(); ops.pop();
                    values.push(applyOp(a, op));
                }
            }

            if (values.size() != 1) {
                printf("equation expression is wrong. The number of remaining number is not equal to 1.\n");
                exit(1);
            }

            return values.top();
        }

        std::string replaceVariables(const std::string& expression, const std::vector<std::string>* var_name) {

            // placeholder is "\x01" and "\x02", which is hard to be typed by user... but maybe user can type...
            // therefore, I want to check the equation beforehand
            // also, "\x03" and "\x04" are used for unary operator
            if ((expression.find(std::string("\x01")) != std::string::npos) || (expression.find(std::string("\x02")) != std::string::npos) || (expression.find(std::string("\x03")) != std::string::npos) || (expression.find(std::string("\x04")) != std::string::npos)){
                printf("In the equation expression, Ascii 01, 02, 03, 04 are included. It is not feasible\n");
                exit(1);
            }

            std::string replaced_expr = expression;

            // change string variables to int index. We do not replace them as variable value to save computing time
            for (int i = 0; i < var_name->size(); i++) {
                std::string::size_type pos = 0;
                while ((pos = replaced_expr.find(var_name->at(i), pos)) != std::string::npos) {
                    // placeholder is "\x01" and "\x02"
                    replaced_expr.replace(pos, var_name->at(i).length(), "\x01" + std::to_string(i) + "\x02");
                    pos += ("\x01" + std::to_string(i) + "\x02").length();
                }
            }

            return replaced_expr;
        }

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
        std::string variable_name;
        int variable_index;

        std::string png_name;
    public:
        DrawTH1D(const char* hist_name_, const char* hist_title_, const char* variable_name_, int nbins_, double x_low_, double x_high_, const char* png_name_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), variable_name(variable_name_), nbins(nbins_), x_low(x_low_), x_high(x_high_), png_name(png_name_), variable_names(variable_names_), VariableTypes(VariableTypes_)
        {
            hist = new TH1D(hist_name_, hist_title_, nbins, x_low, x_high);
        }
        ~DrawTH1D() {
            delete hist;
        }

        void Start() override {
            // get variable index
            std::vector<std::string>::iterator iter = variable_names->find(variable_names->begin(), variable_names->end(), variable_name);

            if (iter != variable_names->end()) {
                variable_index = iter - variable_names->begin();
            }
            else {
                printf("cannot find variable: %s\n", variable_name.c_str());
                exit(1);
            }
        }

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                if (VariableTypes->at(variable_index) == "Double_t") {
                    hist->Fill((double)std::get<double>(iter->variable.at(variable_index)));
                }
                else if (VariableTypes->at(variable_index) == "Int_t") {
                    hist->Fill((double)std::get<int>(iter->variable.at(variable_index)));
                }
                else if (VariableTypes->at(variable_index) == "UInt_t") {
                    hist->Fill((double)std::get<unsigned int>(iter->variable.at(variable_index)));
                }
                else if (VariableTypes->at(variable_index) == "Float_t") {
                    hist->Fill((double)std::get<float>(iter->variable.at(variable_index)));
                }
                else {
                    printf("unexpected data type\n");
                    exit(1);
                }

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

}

#endif 

