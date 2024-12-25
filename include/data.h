#ifndef DATA_H
#define DATA_H

#include <variant>
#include <vector>
#include <string>

typedef struct data {
    std::vector<std::variant<int, unsigned int, float, double, std::string*>> variable;
    std::string label;
    std::string filename;
} Data;

#endif 