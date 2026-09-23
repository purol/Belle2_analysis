#ifndef DATA_H
#define DATA_H

#include <variant>
#include <vector>
#include <string>
#include <memory>

#include "variable_data.h"

typedef struct data {
    VariableData variable;
    std::string label;
    std::string filename;

    std::vector<std::shared_ptr<std::string>> string_storage;

    data() = default;

    explicit data(const std::shared_ptr<VariableSchema>& schema) : variable(schema) {}

    void PushString(const std::string& value) {
        // deep copy std::string
        auto ptr = std::make_shared<std::string>(value);

        variable.push_back(ptr.get());
        string_storage.push_back(ptr);
    }
} Data;

#endif 