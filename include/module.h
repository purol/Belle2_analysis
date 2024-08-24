#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>
#include <stack>
#include <sstream>

#include "data.h"

namespace Module {

    class Module {
    public:
        Module() {}
        virtual ~Module() {}
        virtual void Process(std::vector<Data>* data) = 0;
        virtual void Print() = 0;
    };

    class Cut : public Module {
    private:
        std::string cut_string;
        std::string replaced_expr;
        std::vector<std::string> variable_names;
        std::vector<std::string> VariableTypes;

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

        int precedence(const std::string& op) {
            if (op == "||") return 1;
            if (op == "&&") return 2;
            if (op == "==" || op == "!=") return 3;
            if (op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
            if (op == "+" || op == "-") return 5;
            if (op == "*" || op == "/") return 6;
            if (op == "^") return 7;
            return 0;
        }

        double evaluateExpression(const std::string& replaced_expr_, const std::vector<std::variant<int, unsigned int, float, double>> variables_, const std::vector<std::string> VariableTypes_) {
            std::istringstream iss(replaced_expr_);
            std::stack<double> values;
            std::stack<std::string> ops;

            char token;
            while (iss >> token) {
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
                    int index;
                    iss >> index;

                    if (VariableTypes_.at(index) == "Double_t") {
                        values.push((double)std::get<double>(variables_.at(index)));
                    }
                    else if (VariableTypes_.at(index) == "Int_t") {
                        values.push((double)std::get<int>(variables_.at(index)));
                    }
                    else if (VariableTypes_.at(index) == "UInt_t") {
                        values.push((double)std::get<unsigned int>(variables_.at(index)));
                    }
                    else if (VariableTypes_.at(index) == "Float_t") {
                        values.push((double)std::get<float>(variables_.at(index)));
                    }
                    else {
                        printf("unexpected data type\n");
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
                else if (std::string("+-*/^<>=!&|").find(token) != std::string::npos) {
                    std::string temp_operator;

                    switch (token) {
                    case '+':
                    case '-':
                    case '*':
                    case '/':
                    case '^':
                        temp_operator = std::string(1, token);
                        break;
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
                    }

                    while (!ops.empty() && (precedence(ops.top()) >= precedence(temp_operator))) {
                        double b = values.top(); values.pop();
                        double a = values.top(); values.pop();
                        std::string op = ops.top(); ops.pop();
                        values.push(applyOp(a, b, op));
                    }
                    ops.push(temp_operator);
                }
                else {
                    printf("unknown token: %c\n", token);
                    exit(1);
                }
            }

            while (!ops.empty()) {
                if (values.size() < 2) {
                    printf("equation expression is wrong\n");
                    exit(1);
                }

                double b = values.top(); values.pop();
                double a = values.top(); values.pop();
                std::string op = ops.top(); ops.pop();
                values.push(applyOp(a, b, op));
            }

            if (values.size() != 1) {
                printf("equation expression is wrong\n");
                exit(1);
            }

            return values.top();
        }

        std::string replaceVariables(const std::string& expression, const std::vector<std::string> var_name) {

            std::string replaced_expr = expression;

            // change string variables to int index. We do not replace them as variable value to save computing time
            for (int i = 0; i < var_name.size(); i++) {
                std::string::size_type pos = 0;
                while ((pos = replaced_expr.find(var_name.at(i), pos)) != std::string::npos) {
                    replaced_expr.replace(pos, var_name.at(i).length(), std::to_string(i));
                    pos += std::to_string(i).length();
                }
            }

            return replaced_expr;
        }

    public:
        Cut(const char* cut_string_, std::vector<std::string>* variable_names_, std::vector<std::string>* VariableTypes_) : Module(), cut_string(cut_string_), variable_names(*variable_names_), VariableTypes(*VariableTypes_) {
            replaced_expr = replaceVariables(cut_string, variable_names);
        }
        ~Cut() {}

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                double result = evaluateExpression(replaced_expr, iter->variable, VariableTypes);
                if (result < 0.5) data->erase(iter);
                else ++iter;
            }
        }

        void Print() override {}
    };

    class PrintInformation : public Module {
    private:
        std::string print_string;
        double Ncandidate;
    public:
        PrintInformation(const char* print_string_) : Module(), print_string(print_string_), Ncandidate(0){}
        ~PrintInformation() {}

        void Process(std::vector<Data>* data) override {
            for (std::vector<Data>::iterator iter = data->begin(); iter != data->end(); ) {
                Ncandidate = Ncandidate + 1.0;
                ++iter;
            }
        }

        void Print() override {
            printf("%s\n", print_string.c_str());
            printf("Number of candidate: %lf\n", Ncandidate);
        }
    };

}

#endif 
