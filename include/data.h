#ifndef DATA_H
#define DATA_H

#include <variant>
#include <vector>
#include <string>
#include <memory>

typedef struct data {
    std::vector<std::variant<int, unsigned int, float, double, std::string*>> variable;
    std::string label;
    std::string filename;

    std::vector<std::shared_ptr<std::string>> string_storage;

    void PushString(const std::string& value) {
        // deep copy std::string
        auto ptr = std::make_shared<std::string>(value);

        variable.push_back(ptr.get());
        string_storage.push_back(ptr);
    }
} Data;

#endif 