#ifndef EVENTWEIGHT_H
#define EVENTWEIGHT_H

#include "data.h"
#include "base.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <random>
#include <algorithm>
#include <map>
#include <cstdlib>

struct WeightAxis {
    std::string name;
    std::string min_column;
    std::string max_column;
};

struct WeightUncertainty {
    std::string uncertainty_name;
    std::string up_column;
    std::string down_column;
    bool bins_correlated;
};

struct WeightRange{
    double min;
    double max;
};

struct WeightVariation{
    std::string uncertainty_name;
    double up_value; // Absolute uncertainty of weight. Should be positive.
    double down_value; // Absolute uncertainty of weight. Should be positive.
};

struct WeightBin {
    std::vector<WeightRange> ranges;
    double weight;
    std::vector<WeightVariation> uncertainties = {};
};

struct WeightUncertaintyInformation {
    std::string uncertainty_name;
    bool bins_correlated;
};

class EventWeight {
private:
    inline static std::mt19937 gen{ std::random_device{}() };
    std::normal_distribution<double> normal_dist{0.0, 1.0};

    bool constWeight;
    static constexpr double DEFAULT_VALUE = 1.0;
    bool ignoreOutOfRange;

    std::vector<std::string> variable_names; // name of variable used in EventWeight class
    std::vector<std::vector<double>> variable_min; // variable_min[bin range][variable]
    std::vector<std::vector<double>> variable_max; // variable_max[bin range][variable]
    std::vector<double> nominal_weight_value;
    std::vector<double> fluctuated_weight_value;
    std::map<std::string, int> uncertainty_name_to_index;
    std::vector<std::vector<double>> fluctuation_up; // Absolute uncertainty of weight. Should be positive. fluctuation_up[fluctuation type][fluctuation value]
    std::vector<std::vector<double>> fluctuation_down; // Absolute uncertainty of weight. Should be positive. fluctuation_down[fluctuation type][fluctuation value]
    std::vector<bool> correlated; // correlation among bins

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

    void ApplyFluctuate(const std::string& uncertainty_name_);

public:
    EventWeight(const double constWeight_);
    EventWeight(const std::string& CSV_file_, const std::vector<WeightAxis>& axis_columns_, const std::string& weight_column_, const std::vector<WeightUncertainty>& weight_unc_columns_, bool ignoreOutOfRange_);
    EventWeight(const std::string& CSV_file_, const std::vector<WeightAxis>& axis_columns_, const std::string& weight_column_, bool ignoreOutOfRange_);
    //EventWeight(const std::vector<std::string>& variable_names_, const std::vector<std::vector<double>>& variable_min_, const std::vector<std::vector<double>>& variable_max_, const std::vector<double>& nominal_weight_value_, const std::vector<std::vector<double>>& fluctuation_up_, const std::vector<std::vector<double>>& fluctuation_down_, const std::vector<bool>& correlated_, bool ignoreOutOfRange_);
    //EventWeight(const std::vector<std::string>& variable_names_, const std::vector<std::vector<double>>& variable_min_, const std::vector<std::vector<double>>& variable_max_, const std::vector<double>& nominal_weight_value_, bool ignoreOutOfRange_);

    EventWeight(const std::vector<std::string>& variable_names_, const std::vector<WeightBin>& weightbins_, bool ignoreOutOfRange_);
    EventWeight(const std::vector<std::string>& variable_names_, const std::vector<WeightBin>& weightbins_, const std::vector<WeightUncertaintyInformation>& uncertainty_information_, bool ignoreOutOfRange_);

    double Evaluate(const Data& data_, const std::vector<std::size_t>& variable_indices_) const;
    void Fluctuate();
    void Fluctuate(const std::string& uncertainty_name_);
    void Fluctuate(const std::vector<std::string>& uncertainty_names_);
    void ResetToNominal();
    const std::vector<std::string>& GetVarNames() const;
};

inline void EventWeight::ApplyFluctuate(const std::string& uncertainty_name_){
    if(uncertainty_name_to_index.find(uncertainty_name_) == uncertainty_name_to_index.end()){
        printf("[EventWeight::ApplyFluctuate] cannot find uncertainty %s\n", uncertainty_name_.c_str());
        exit(1);
    }

    int idx_fluc = uncertainty_name_to_index.find(uncertainty_name_)->second;

    if (correlated.at(idx_fluc)) {
        double alpha = normal_dist(gen);
        for (int i = 0; i < nominal_weight_value.size(); i++) {
            if (alpha >= 0) fluctuated_weight_value.at(i) = fluctuated_weight_value.at(i) + alpha * fluctuation_up.at(idx_fluc).at(i);
            else fluctuated_weight_value.at(i) = fluctuated_weight_value.at(i) + alpha * fluctuation_down.at(idx_fluc).at(i);
        }
    }
    else {
        for (int i = 0; i < nominal_weight_value.size(); i++) {
            double alpha = normal_dist(gen);
            if (alpha >= 0) fluctuated_weight_value.at(i) = fluctuated_weight_value.at(i) + alpha * fluctuation_up.at(idx_fluc).at(i);
            else fluctuated_weight_value.at(i) = fluctuated_weight_value.at(i) + alpha * fluctuation_down.at(idx_fluc).at(i);
        }
    }
}

inline EventWeight::EventWeight(const double constWeight_) : ignoreOutOfRange(false), constWeight(true) {
    nominal_weight_value.push_back(constWeight_);
    fluctuated_weight_value.push_back(constWeight_);
}

inline EventWeight::EventWeight(const std::string& CSV_file_, const std::vector<WeightAxis>& axis_columns_, const std::string& weight_column_, const std::vector<WeightUncertainty>& weight_unc_columns_, bool ignoreOutOfRange_) : ignoreOutOfRange(ignoreOutOfRange_), constWeight(false) {
    // usage example: EventWeight("./muonid.csv", {{"momentum", "p_min","p_max"}, {"angle", "theta_min","theta_max"}}, "dataMCratio", {{"stat", "dataMCratio_stat_up", "dataMCratio_stat_down", false}, {"syst", "dataMCratio_sys_up", "dataMCratio_sys_down", true}}, true);

    std::vector<std::string> axis_min_column_names;
    std::vector<std::string> axis_max_column_names;
    std::string weight_column_name;
    std::vector<std::string> uncer_up_column_names;
    std::vector<std::string> uncer_down_column_names;

    for (int i = 0; i < axis_columns_.size(); i++) {
        axis_min_column_names.push_back(axis_columns_.at(i).min_column);
        axis_max_column_names.push_back(axis_columns_.at(i).max_column);
    }
    weight_column_name = weight_column_;
    for (int i = 0; i < weight_unc_columns_.size(); i++) {
        if(uncertainty_name_to_index.find(weight_unc_columns_.at(i).uncertainty_name) != uncertainty_name_to_index.end()){
            printf("[EventWeight::EventWeight] uncertainty name %s is duplicated.\n", weight_unc_columns_.at(i).uncertainty_name.c_str());
            exit(1);
        }
        uncertainty_name_to_index.insert(std::make_pair(weight_unc_columns_.at(i).uncertainty_name, i));
        uncer_up_column_names.push_back(weight_unc_columns_.at(i).up_column);
        uncer_down_column_names.push_back(weight_unc_columns_.at(i).down_column);
    }

    std::vector<std::string> all_column_names;
    all_column_names.insert(all_column_names.end(), axis_min_column_names.begin(), axis_min_column_names.end());
    all_column_names.insert(all_column_names.end(), axis_max_column_names.begin(), axis_max_column_names.end());
    all_column_names.push_back(weight_column_name);
    all_column_names.insert(all_column_names.end(), uncer_up_column_names.begin(), uncer_up_column_names.end());
    all_column_names.insert(all_column_names.end(), uncer_down_column_names.begin(), uncer_down_column_names.end());

    std::vector<std::vector<double>> column_values = ReadCSVDoubleColumns(CSV_file_, all_column_names);
    const int bin_size = column_values.at(0).size();
    const int axis_num = axis_min_column_names.size();
    const int uncer_num = uncer_up_column_names.size();

    fluctuation_up.resize(uncer_num);
    fluctuation_down.resize(uncer_num);

    for (int bin_index = 0; bin_index < bin_size; bin_index++) {
        std::vector<double> temp_vec;
        for (int axis_index = 0; axis_index < axis_num; axis_index++) {
            temp_vec.push_back(column_values.at(axis_index).at(bin_index));
        }
        variable_min.push_back(temp_vec);
        temp_vec.clear();

        for (int axis_index = axis_num; axis_index < (axis_num + axis_num); axis_index++) {
            temp_vec.push_back(column_values.at(axis_index).at(bin_index));
        }
        variable_max.push_back(temp_vec);
        temp_vec.clear();

        nominal_weight_value.push_back(column_values.at(axis_num + axis_num).at(bin_index));
        fluctuated_weight_value.push_back(column_values.at(axis_num + axis_num).at(bin_index));

        for (int axis_index = (axis_num + axis_num + 1); axis_index < (axis_num + axis_num + 1 + uncer_num); axis_index++) {
            fluctuation_up.at(axis_index - (axis_num + axis_num + 1)).push_back(column_values.at(axis_index).at(bin_index));
        }
        temp_vec.clear();

        for (int axis_index = (axis_num + axis_num + 1 + uncer_num); axis_index < (axis_num + axis_num + 1 + uncer_num + uncer_num); axis_index++) {
            fluctuation_down.at(axis_index - (axis_num + axis_num + 1 + uncer_num)).push_back(column_values.at(axis_index).at(bin_index));
        }
        temp_vec.clear();
    }

    for (int i = 0; i < axis_columns_.size(); i++) {
        variable_names.push_back(axis_columns_.at(i).name);
    }

    for (int i = 0; i < weight_unc_columns_.size(); i++) {
        correlated.push_back(weight_unc_columns_.at(i).bins_correlated);
    }
}

inline EventWeight::EventWeight(const std::string& CSV_file_, const std::vector<WeightAxis>& axis_columns_, const std::string& weight_column_, bool ignoreOutOfRange_) : ignoreOutOfRange(ignoreOutOfRange_), constWeight(false) {
    // usage example: EventWeight("./muonid.csv", {{"momentum", "p_min","p_max"}, {"angle", "theta_min","theta_max"}}, "dataMCratio");

    std::vector<std::string> axis_min_column_names;
    std::vector<std::string> axis_max_column_names;
    std::string weight_column_name;

    for (int i = 0; i < axis_columns_.size(); i++) {
        axis_min_column_names.push_back(axis_columns_.at(i).min_column);
        axis_max_column_names.push_back(axis_columns_.at(i).max_column);
    }
    weight_column_name = weight_column_;

    std::vector<std::string> all_column_names;
    all_column_names.insert(all_column_names.end(), axis_min_column_names.begin(), axis_min_column_names.end());
    all_column_names.insert(all_column_names.end(), axis_max_column_names.begin(), axis_max_column_names.end());
    all_column_names.push_back(weight_column_name);

    std::vector<std::vector<double>> column_values = ReadCSVDoubleColumns(CSV_file_, all_column_names);
    const int bin_size = column_values.at(0).size();
    const int axis_num = axis_min_column_names.size();

    for (int bin_index = 0; bin_index < bin_size; bin_index++) {
        std::vector<double> temp_vec;
        for (int axis_index = 0; axis_index < axis_num; axis_index++) {
            temp_vec.push_back(column_values.at(axis_index).at(bin_index));
        }
        variable_min.push_back(temp_vec);
        temp_vec.clear();

        for (int axis_index = axis_num; axis_index < (axis_num + axis_num); axis_index++) {
            temp_vec.push_back(column_values.at(axis_index).at(bin_index));
        }
        variable_max.push_back(temp_vec);
        temp_vec.clear();

        nominal_weight_value.push_back(column_values.at(axis_num + axis_num).at(bin_index));
        fluctuated_weight_value.push_back(column_values.at(axis_num + axis_num).at(bin_index));
    }

    for (int i = 0; i < axis_columns_.size(); i++) {
        variable_names.push_back(axis_columns_.at(i).name);
    }
}

//inline EventWeight::EventWeight(const std::vector<std::string>& variable_names_, const std::vector<std::vector<double>>& variable_min_, const std::vector<std::vector<double>>& variable_max_, const std::vector<double>& nominal_weight_value_, const std::vector<std::vector<double>>& fluctuation_up_, const std::vector<std::vector<double>>& fluctuation_down_, const std::vector<bool>& correlated_, bool ignoreOutOfRange_) : variable_names(variable_names_), variable_min(variable_min_), variable_max(variable_max_), nominal_weight_value(nominal_weight_value_), fluctuated_weight_value(nominal_weight_value_), fluctuation_up(fluctuation_up_), fluctuation_down(fluctuation_down_), correlated(correlated_), ignoreOutOfRange(ignoreOutOfRange_), constWeight(false) {}

//inline EventWeight::EventWeight(const std::vector<std::string>& variable_names_, const std::vector<std::vector<double>>& variable_min_, const std::vector<std::vector<double>>& variable_max_, const std::vector<double>& nominal_weight_value_, bool ignoreOutOfRange_) : variable_names(variable_names_), variable_min(variable_min_), variable_max(variable_max_), nominal_weight_value(nominal_weight_value_), fluctuated_weight_value(nominal_weight_value_), ignoreOutOfRange(ignoreOutOfRange_), constWeight(false) {}

inline EventWeight::EventWeight(const std::vector<std::string>& variable_names_, const std::vector<WeightBin>& weightbins_, bool ignoreOutOfRange_) : EventWeight(variable_names_, weightbins_, std::vector<WeightUncertaintyInformation>{}, ignoreOutOfRange_) {}

inline EventWeight::EventWeight(const std::vector<std::string>& variable_names_, const std::vector<WeightBin>& weightbins_, const std::vector<WeightUncertaintyInformation>& uncertainty_information_, bool ignoreOutOfRange_) : variable_names(variable_names_), ignoreOutOfRange(ignoreOutOfRange_), constWeight(false) {
    fluctuation_up.resize(uncertainty_information_.size());
    fluctuation_down.resize(uncertainty_information_.size());

    int temp_index = 0;
    for(const auto& uncertainty_info : uncertainty_information_) {
        if(uncertainty_name_to_index.find(uncertainty_info.uncertainty_name) != uncertainty_name_to_index.end()){
            printf("[EventWeight::EventWeight] uncertainty name %s is duplicated.\n", uncertainty_info.uncertainty_name.c_str());
            exit(1);
        }
        uncertainty_name_to_index.insert(std::make_pair(uncertainty_info.uncertainty_name, temp_index));
        correlated.push_back(uncertainty_info.bins_correlated);
        temp_index++;
    }

    for(const auto& weightbin : weightbins_) {
        if(weightbin.ranges.size() != variable_names_.size()){
            printf("[EventWeight::EventWeight] There are %zu variables are registered, while some bin requires %zu variable range(s).\n", variable_names_.size(), weightbin.ranges.size());
            exit(1);
        }

        nominal_weight_value.push_back(weightbin.weight);
        fluctuated_weight_value.push_back(weightbin.weight);

        std::vector<double> temp_min;
        std::vector<double> temp_max;
        for(const auto& weightrange : weightbin.ranges){
            temp_min.push_back(weightrange.min);
            temp_max.push_back(weightrange.max);
        }

        variable_min.push_back(temp_min);
        variable_max.push_back(temp_max);

        for(const auto& uncertainty : weightbin.uncertainties){
            if(uncertainty_name_to_index.find(uncertainty.uncertainty_name) == uncertainty_name_to_index.end()) {
                printf("[EventWeight::EventWeight] uncertainty %s cannot be found.\n", uncertainty.uncertainty_name.c_str());
                exit(1);
            }

            int idx_fluc = uncertainty_name_to_index.find(uncertainty.uncertainty_name)->second;

            fluctuation_up.at(idx_fluc).push_back(uncertainty.up_value);
            fluctuation_down.at(idx_fluc).push_back(uncertainty.down_value);
        }
    }

    for(int idx_fluc = 0; idx_fluc < fluctuation_up.size(); idx_fluc++) {
        if(fluctuation_up.at(idx_fluc).size() != nominal_weight_value.size()){
            printf("[EventWeight::EventWeight] uncertainty %s has %zu vlaue, while there are %zu bins.\n", uncertainty_information_.at(idx_fluc).uncertainty_name.c_str(), fluctuation_up.at(idx_fluc).size(), nominal_weight_value.size());
            exit(1);
        }
    }
}

inline double EventWeight::Evaluate(const Data& data_, const std::vector<std::size_t>& variable_indices_) const {
    if(constWeight){
        return fluctuated_weight_value.at(0);
    }
    else{
        if(variable_indices_.size() != variable_names.size()){
            printf("[EventWeight::Evaluate] Expected %zu input variables, but received %zu\n", variable_names.size(), variable_indices_.size());
            exit(1);
        }

        for(int i = 0; i < fluctuated_weight_value.size(); i++){
            bool IsThisBin = true;
            for(int j = 0; j < variable_indices_.size(); j++){
                std::size_t index = variable_indices_.at(j);
                double value_min = variable_min.at(i).at(j);
                double value_max = variable_max.at(i).at(j);
                double value;
                
                if (std::holds_alternative<int>(data_.variable.at(index))) value = static_cast<double>(std::get<int>(data_.variable.at(index)));
                else if (std::holds_alternative<unsigned int>(data_.variable.at(index))) value = static_cast<double>(std::get<unsigned int>(data_.variable.at(index)));
                else if (std::holds_alternative<float>(data_.variable.at(index))) value = static_cast<double>(std::get<float>(data_.variable.at(index)));
                else if (std::holds_alternative<double>(data_.variable.at(index))) value = std::get<double>(data_.variable.at(index));
                else {
                    printf("[EventWeight::Evaluate] unsupported type\n");
                    exit(1);
                }

                if((value < value_min) || (value >= value_max)){
                    IsThisBin = false;
                    break;
                }
            }

            if(IsThisBin) return fluctuated_weight_value.at(i);
        }
        if(ignoreOutOfRange) return DEFAULT_VALUE;
        else {
            printf("[EventWeight::Evaluate] cannot find proper bins\n");
            exit(1);
        }
    }
}

inline void EventWeight::Fluctuate(){
    ResetToNominal();
    if(fluctuation_up.empty()) {
        printf("[EventWeight::Fluctuate] There is no uncertainty for weight. Just ignore it.\n");
        return;
    }
    else {
        for(const auto& pair : uncertainty_name_to_index){
            const std::string uncertainty_name = pair.first;
            ApplyFluctuate(uncertainty_name);
        }
    }
}

inline void EventWeight::Fluctuate(const std::vector<std::string>& uncertainty_names_) {
    ResetToNominal();
    for(const std::string& uncertainty_name : uncertainty_names_) ApplyFluctuate(uncertainty_name);
}

inline void EventWeight::Fluctuate(const std::string& uncertainty_name_) {
    ResetToNominal();
    ApplyFluctuate(uncertainty_name_);
}

inline void EventWeight::ResetToNominal(){
    for(int i = 0; i < nominal_weight_value.size(); i++){
        fluctuated_weight_value.at(i) = nominal_weight_value.at(i);
    }
}

inline const std::vector<std::string>& EventWeight::GetVarNames() const {
    return variable_names;
}

class EventWeights{
private:
    EventWeights() = default;

    EventWeights(const EventWeights&) = delete;
    EventWeights& operator=(const EventWeights&) = delete;

    std::map<std::string, EventWeight> eventweight_map;

    static EventWeights& getInstance(){
        static EventWeights instance;
        return instance;
    }

    void InternalRegister(const std::string& weight_name_, const EventWeight& eventweight_);
    EventWeight* InternalGetWeight(const std::string& weight_name_);
public:
    static void Register(const std::string& weight_name_, const EventWeight& eventweight_);
    static EventWeight* GetWeight(const std::string& weight_name_);
    static void Fluctuate(const std::string& weight_name_);
    static void Fluctuate(const std::string& weight_name_, const std::string& uncertainty_name_);
    static void Fluctuate(const std::string& weight_name_, const std::vector<std::string>& uncertainty_names_);
    static void ResetToNominal(const std::string& weight_name_);
};

inline void EventWeights::InternalRegister(const std::string& weight_name_, const EventWeight& eventweight_){
    std::map<std::string, EventWeight>::iterator it = eventweight_map.find(weight_name_);

    if(it != eventweight_map.end()){
        printf("[EventWeights::InternalRegister] weight %s already exists\n", weight_name_.c_str());
        exit(1);
    }
    else eventweight_map.insert_or_assign(weight_name_, eventweight_); 
}

inline EventWeight* EventWeights::InternalGetWeight(const std::string& weight_name_){
    std::map<std::string, EventWeight>::iterator it = eventweight_map.find(weight_name_);

    if(it != eventweight_map.end()){
        return &(it->second);
    }
    else{
        printf("[EventWeights::InternalGetWeight] weight %s is not found\n", weight_name_.c_str());
        exit(1);
    }
}

inline void EventWeights::Register(const std::string& weight_name_, const EventWeight& eventweight_){
    getInstance().InternalRegister(weight_name_, eventweight_);
}

inline EventWeight* EventWeights::GetWeight(const std::string& weight_name_){
    return getInstance().InternalGetWeight(weight_name_);
}

inline void EventWeights::Fluctuate(const std::string& weight_name_) {
    getInstance().InternalGetWeight(weight_name_)->Fluctuate();
}

inline void EventWeights::Fluctuate(const std::string& weight_name_, const std::string& uncertainty_name_){
    getInstance().InternalGetWeight(weight_name_)->Fluctuate(uncertainty_name_);
}

inline void EventWeights::Fluctuate(const std::string& weight_name_, const std::vector<std::string>& uncertainty_names_){
    getInstance().InternalGetWeight(weight_name_)->Fluctuate(uncertainty_names_);
}

inline void EventWeights::ResetToNominal(const std::string& weight_name_) {
    getInstance().InternalGetWeight(weight_name_)->ResetToNominal();
}

#endif 

