#ifndef DATA_H
#define DATA_H

#include <variant>
#include <vector>

typedef struct data {
    std::vector<std::variant<int, unsigned int, float, double>> variable;
} Data;

#endif 