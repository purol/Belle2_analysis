#ifndef STRING_EQUATION_H
#define STRING_EQUATION_H

#include <string>
#include <stack>
#include <sstream>

enum class OpType {
    Value,      // Literal number (e.g., 3.14)
    Variable,   // Variable Index
    Add, Sub, Mul, Div, Pow,
    GT, LT, GE, LE, EQ, NE,
    And, Or,
    UnaryMinus, UnaryPlus,
    Openparenthesis, Closeparenthesis
};

struct Token {
    OpType type;
    double value; // Used if type == Value
    int index;    // Used if type == Variable
};

int precedence(OpType op) {
    switch (op) {
    case OpType::Or: return 1;
    case OpType::And: return 2;
    case OpType::EQ: case OpType::NE: return 3;
    case OpType::LT: case OpType::LE: case OpType::GT: case OpType::GE: return 4;
    case OpType::Add: case OpType::Sub: return 5;
    case OpType::Mul: case OpType::Div: return 6;
    case OpType::Pow: return 7;
    case OpType::UnaryMinus: case OpType::UnaryPlus: return 8;
    default: return 0;
    }
}

double applyOp(double a, double b, const OpType op) {
    switch (op) {
    case OpType::Add: return (a + b);
    case OpType::Sub: return (a - b);
    case OpType::Mul: return (a * b);
    case OpType::Div: return (a / b);
    case OpType::Pow: return std::pow(a, b);
    case OpType::LT: {
        if (a < b) return 1.0;
        else return 0.0;
    }
    case OpType::GT: {
        if (a > b) return 1.0;
        else return 0.0;
    }
    case OpType::LE: {
        if (a <= b) return 1.0;
        else return 0.0;
    }
    case OpType::GE: {
        if (a >= b) return 1.0;
        else return 0.0;
    }
    case OpType::EQ: {
        if (a == b) return 1.0;
        else return 0.0;
    }
    case OpType::NE: {
        if (a != b) return 1.0;
        else return 0.0;
    }
    case OpType::And: {
        if ((a != 0) && (b != 0)) return 1.0;
        else return 0.0;
    }
    case OpType::Or: {
        if ((a != 0) || (b != 0)) return 1.0;
        else return 0.0;
    }
    default: {
        printf("[applyOp] unknown operator\n");
        exit(1);
        return 1;
    }
    }
}

double applyOp(double a, const OpType op) {

    switch (op) {
    case OpType::UnaryMinus: return -a;
    case OpType::UnaryPlus: return a;
    default: {
        printf("[applyOp] unknown operator\n");
        exit(1);
        return 1;
    }
    }
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

std::vector<Token> PostfixExpression(const std::string& replaced_expr_, const std::vector<std::string>* VariableTypes_) {
    std::istringstream iss(replaced_expr_);
    std::vector<Token> output;
    std::stack<OpType> ops;

    // previous token is needed to check unary operator
    char previous_token = '\0';
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
            output.push_back({ OpType::Value, value, -1});

        }
        else if (token == '\x01') { // it is placeholder

            int index;
            iss >> index;

            if (VariableTypes_->at(index) == "Double_t") {}
            else if (VariableTypes_->at(index) == "Int_t") {}
            else if (VariableTypes_->at(index) == "UInt_t") {}
            else if (VariableTypes_->at(index) == "Float_t") {}
            else if (VariableTypes_->at(index) == "string") {
                printf("[evaluateExpression] string variable cannot be used in equations\n");
                exit(1);
            }
            else {
                printf("unexpected data type\n");
                exit(1);
            }

            output.push_back({ OpType::Variable, -1, index });

            iss >> token;

            if (token != '\x02') {
                printf("placeholder is wrong\n");
                exit(1);
            }

        }
        else if (token == '(') {
            ops.push(OpType::Openparenthesis);
        }
        else if (token == ')') {
            while (!ops.empty() && ops.top() != OpType::Openparenthesis) {
                output.push_back({ ops.top(), -1, -1 });
                ops.pop();

                if (ops.empty()) {
                    printf("cannot find `(`\n");
                    exit(1);
                }
            }
            ops.pop();
        }
        else if ((std::string("+-*/^<>=!&|") + std::string("\x03") + std::string("\x04")).find(token) != std::string::npos) {
            OpType current_op;

            switch (token) {
            case '+': current_op = OpType::Add; break;
            case '-': current_op = OpType::Sub; break;
            case '*': current_op = OpType::Mul; break;
            case '/': current_op = OpType::Div; break;
            case '^': current_op = OpType::Pow; break;
            case '<':
            {
                char next_token;
                iss >> next_token;
                if (next_token != '=') {
                    current_op = OpType::LT;
                    iss.putback(next_token);
                }
                else current_op = OpType::LE;
                break;
            }
            case '>':
            {
                char next_token;
                iss >> next_token;
                if (next_token != '=') {
                    current_op = OpType::GT;
                    iss.putback(next_token);
                }
                else current_op = OpType::GE;
                break;
            }
            case '=':
            {
                char next_token;
                iss >> next_token;
                if (next_token != '=') {
                    printf("[evaluateExpression] unknown operator: %c\n", token);
                    exit(1);
                }
                else current_op = OpType::EQ;
                break;
            }
            case '!':
            {
                char next_token;
                iss >> next_token;
                if (next_token != '=') {
                    printf("[evaluateExpression] unknown operator: %c\n", token);
                    exit(1);
                }
                else current_op = OpType::NE;
                break;
            }
            case '&':
            {
                char next_token;
                iss >> next_token;
                if (next_token != token) {
                    printf("[evaluateExpression] unknown operator: %c\n", token);
                    exit(1);
                }
                else current_op = OpType::And;
                break;
            }
            case '|':
            {
                char next_token;
                iss >> next_token;
                if (next_token != token) {
                    printf("[evaluateExpression] unknown operator: %c\n", token);
                    exit(1);
                }
                else current_op = OpType::Or;
                break;
            }
            case '\x03': current_op = OpType::UnaryMinus; break;
            case '\x04': current_op = OpType::UnaryPlus; break;
            }

            while (!ops.empty() && (precedence(ops.top()) >= precedence(current_op)) && (ops.top() != OpType::Openparenthesis)) {

                // unary is right-associative, do not pop
                if (((current_op == OpType::UnaryMinus) || (current_op == OpType::UnaryPlus)) && (precedence(ops.top()) == precedence(current_op))) break;

                // power is right-associative, do not pop
                if ((current_op == OpType::Pow) && (precedence(ops.top()) == precedence(current_op))) break;

                output.push_back({ ops.top(), -1, -1 });
                ops.pop();
            }
            ops.push(current_op);
        }
        else {
            printf("unknown token: %c\n", token);
            exit(1);
        }

        previous_token = token;
    }

    while (!ops.empty()) {
        output.push_back({ ops.top(), -1, -1 });
        ops.pop();
    }

    return output;
}

double EvaluatePostfixExpression(const std::vector<Token>& postfix_expr_, const std::vector<std::variant<int, unsigned int, float, double, std::string*>>& variables_, const std::vector<std::string>* VariableTypes_) {
    std::stack<double> values;

    for (int i = 0; i < postfix_expr_.size(); i++) {
        Token temp_token = postfix_expr_.at(i);

        if (temp_token.type == OpType::Value) {
            values.push(temp_token.value);
        }
        else if (temp_token.type == OpType::Variable) {
            int index = temp_token.index;

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
        }
        else if ((temp_token.type == OpType::UnaryMinus) || (temp_token.type == OpType::UnaryPlus)) {
            if (values.size() == 0) {
                printf("[EvaluatePostfixExpression] there is no number when unary operator comes\n");
                exit(1);
            }
            double a = values.top(); values.pop();
            values.push(applyOp(a, temp_token.type));
        }
        else {
            if (values.size() < 2) {
                printf("[EvaluatePostfixExpression] there is only %d number when binary operator comes\n", values.size());
                exit(1);
            }
            double b = values.top(); values.pop();
            double a = values.top(); values.pop();
            values.push(applyOp(a, b, temp_token.type));
        }
    }

    if (values.size() != 1) {
        printf("[EvaluatePostfixExpression] size of values is %d\n", values.size());
        exit(1);
    }

    return values.top();

}

#endif 

