#ifndef MODULE_H
#define MODULE_H

#include <vector>
#include <string>

#include "data.h"

namespace Module {

    class Module {
    public:
        Module() {}
        virtual ~Module() {}
        virtual std::vector<Data>* Process(std::vector<Data>* data) = 0;
        virtual void Print() = 0;
    };

    class Cut : public Module {
    private:
        std::string cut_string;
    public:
        Cut(const char* cut_string_) : Module(), cut_string(cut_string_) {}
        ~Cut() {}

        std::vector<Data>* Process(std::vector<Data>* data) override {
            printf("cut\n");
            std::vector<Data>* temp;
            return temp;
        }

        void Print() override {}
    };

}

#endif 