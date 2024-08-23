#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>

#include "data.h"

class Module {
public:
    Module() {}
    virtual ~Module() {}
    virtual std::vector<Data>* Process() = 0;
    virtual void Print() = 0;
};

class Cut : public Module {
private:
    std::string cut_string;
public:
    Cut() : Module(const char* cut_string_) : cut_string(cut_string_) {}
    ~Cut() {}

    std::vector<Data>* Process() override {
        printf("cut\n");
    }

    void Print() override {}
};

#endif 