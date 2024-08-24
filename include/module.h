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
        virtual void Process(std::vector<Data>* data) = 0;
        virtual void Print() = 0;
    };

    class Cut : public Module {
    private:
        std::string cut_string;
    public:
        Cut(const char* cut_string_) : Module(), cut_string(cut_string_) {}
        ~Cut() {}

        void Process(std::vector<Data>* data) override {
            printf("cut\n");
        }

        void Print() override {}
    };

    class PrintInformation : public Module {
    private:
        std::string print_string;
        double Ncandidate;
    public:
        PrintInformation(const char* print_string_) : Module(), print_string(print_string_), Nevt(0){}
        ~PrintInformation() {}

        void Process(std::vector<Data>* data) override {
            Ncandidate = Ncandidate + 1.0;
        }

        void Print() override {
            printf("%s", print_string.c_str());
            printf("Number of candidate: %lf\n", Ncandidate);
        }
    };

}

#endif 