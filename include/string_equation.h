#ifndef STRING_EQUATION_H
#define STRING_EQUATION_H

#include <string>
#include <stack>
#include <sstream>

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
        printf("[applyOp] unknown operator: %s\n", op.c_str());
        exit(1);
        return 1;
    }
}

double applyOp(double a, const std::string& op) {
    if (op == "\x03") return -a;
    else if (op == "\x04") return a;
    else {
        printf("[applyOp] unknown operator: %s\n", op.c_str());
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
            else if (VariableTypes_->at(index) == "string") {
                printf("[evaluateExpression] string variable cannot be used in equations\n");
                exit(1);
            }
            else {
                printf("unexpected data type\n");
                exit(1);
            }

            iss >> token;

            if (token != '\x02') {
                printf("placeholder is wrong\n");
                exit(1);
            }

        }
        else if (token == '(') {
            ops.push(std::string(1, token));
        }
        else if (token == ')') {
            while (!ops.empty() && ops.top() != '(') {
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
                    printf("[evaluateExpression] unknown operator: %c\n", token);
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
                    printf("[evaluateExpression] unknown operator: %c\n", token);
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
    if ((expression.find(std::string("\x01")) != std::string::npos) || (expression.find(std::string("\x02")) != std::string::npos) || (expression.find(std::string("\x03")) != std::string::npos) || (expression.find(std::string("\x04")) != std::string::npos)) {
        printf("In the equation expression, Ascii 01, 02, 03, 04 are included. It is not feasible\n");
        exit(1);
    }

    std::string replaced_expr = expression;

    // change string variables to int index. We do not replace them as variable value to save computing time
    for (int i = 0; i < var_name->size(); i++) {
        std::string::size_type pos = 0;
        while ((pos = replaced_expr.find(var_name->at(i), pos)) != std::string::npos) {
            // accidentally, variable names can be overlapped (ex. Btag_M and Btag_Mbc, missingMomentumOfEventCMS and missingMomentumOfEventCMS_theta). If next and previous char is alphabet or underbar just skip it.
            // to do: how about number? Mbc3 and Mbc...
            if (pos != 0) {
                if (std::isalpha(replaced_expr.at(pos - 1)) || (replaced_expr.at(pos - 1) == '_')) {
                    pos = pos + var_name->at(i).length();
                    continue;
                }
            }
            if ((pos + var_name->at(i).length()) != replaced_expr.length()) {
                if (std::isalpha(replaced_expr.at(pos + var_name->at(i).length())) || (replaced_expr.at(pos + var_name->at(i).length()) == '_')) {
                    pos = pos + var_name->at(i).length();
                    continue;
                }
            }

            // placeholder is "\x01" and "\x02"
            replaced_expr.replace(pos, var_name->at(i).length(), "\x01" + std::to_string(i) + "\x02");
            pos += ("\x01" + std::to_string(i) + "\x02").length();
        }
    }

    return replaced_expr;
}

#endif 

