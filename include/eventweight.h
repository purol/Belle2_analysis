#ifndef EVENTWEIGHT_H
#define EVENTWEIGHT_H

#include "data.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <random>

class EventWeight {
private:
    static std::mt19937 gen(std::random_device{}());
    std::normal_distribution<double> dist{0.0, 1.0};

    bool constWeight;
    static constexpr double DEFAULT_VALUE = 1.0;

    std::vector<std::string> variable_names;
    std::vector<std::string> variable_index; // index in Data structure
    std::vector<std::vector<double>> variable_min; // variable_min[bin range][variable]
    std::vector<std::vector<double>> variable_max; // variable_min[bin range][variable]
    std::vector<double> nominal_weight_value;
    std::vector<double> fluctuated_weight_value;
    std::vector<std::vector<double>> fluctuation_up; // Absolute uncertainty of weight. fluctuation_up[fluctuation type][fluctuation value]
    std::vector<std::vector<double>> fluctuation_down; // Absolute uncertainty of weight. fluctuation_down[fluctuation type][fluctuation value]
    std::vector<bool> correlated; // correlation among bins

    bool IsItAdditive;

    /* 
    example table:
    p_min p_max theta_min thate_max dataMCratio dataMCratio_up1 dataMCratio_down1
    0.0 1.0 0.7 0.8 1.00 0.01 0.11
    1.0 2.0 0.7 0.8 0.93 0.02 0.12
    0.0 1.0 0.8 0.9 0.99 0.03 0.13
    1.0 2.0 0.8 0.9 1.10 0.04 0.14

    =>

    variable_names = {p, theta}
    variable_min = { {0.0, 0.7}, {1.0, 0.7}, {0.0, 0.8}, {1.0, 0.8} }
    variable_max = { {1.0, 0.8}, {2.0, 0.8}, {1.0, 0.9}, {2.0, 0.9} }
    nominal_weight_value = { 1.00, 0.93, 0.99, 1.10 }
    fluctuation_up = { {0.01, 0.02, 0.03, 0.04} }
    fluctuation_down = { {0.11, 0.12, 0.13, 0.14} }
    correlated = { false }
    */

public:
    EventWeight(const double constWeight_);
    EventWeight(const char* CSV_name_, std::vector<std::string> bin_names_, std::vector<std::string> weight_name_, std::vector<std::string> fluctuation_up_names_, std::vector<std::string> fluctuation_down_names_, std::vector<bool> correlated_);

    void NameToIndex(std::vector<std::string> variable_names_);
    double Evaluate(const Data& data_);
    void Fluctuate();
    void ResetToNominal();
};

EventWeight::EventWeight(const double constWeight_) : gen(rd()), constWeight(true) {
    nominal_weight_value.push_back(constWeight_);
    fluctuated_weight_value.push_back(constWeight_);
}

void EventWeight::NameToIndex(std::vector<std::string> variable_names_){
    for(int i = 0; i < variable_names.size(); i++){
        std::string variable_name = variable_names.at(i);
        std::vector<std::string>::iterator it = std::find(variable_names_.begin(), variable_names_.end(), variable_name);
        if (it != variable_names_.end()) {
            int index = std::distance(variable_names_.begin(), it);
            variable_index.push_back(index);
        }
        else{
            printf("[EventWeight] Cannot find variable %s\n", variable_name.c_str());
            exit(1);
        }
    }
}

double EventWeight::Evaluate(const Data& data_){
    if(constWeight){
        return fluctuated_weight_value.at(0);
    }
    else{
        for(int i = 0; i < fluctuated_weight_value.size(); i++){
            bool IsThisBin = true;
            for(int j = 0; j < variable_index.size(); j++){
                index = variable_index.at(j);
                value_min = variable_min.at(i).at(j);
                value_max = variable_max.at(i).at(j);
                value = std::get<double>(data_.variable.at(index));

                if((value < value_min) || (value >= value_max)){
                    IsThisBin = false;
                    break;
                }
            }

            if(IsThisBins) return fluctuated_weight_value.at(i);
        }
        return DEFAULT_VALUE;
    }
}

void EventWeight::Fluctuate(){
    if(constWeight) return;
    else{
        // to do
    }
}

void EventWeight::ResetToNominal(){
    for(int i = 0; i < nominal_weight_value.size(); i++){
        fluctuated_weight_value.at(i) = nominal_weight_value.at(i);
    }
}

#endif 

