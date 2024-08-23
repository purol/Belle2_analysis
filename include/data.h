#ifndef DATA_H
#define DATA_H

#include <vector>

typedef struct data {
    std::vector<int> variable_int;
    std::vector<unsigned int> variable_unsigned_int;
    std::vector<float> variable_float;
    std::vector<double> variable_double;
} Data;

#endif 