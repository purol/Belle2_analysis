#ifndef FIT_MANAGER_H
#define FIT_MANAGER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

#include <RooDataSet.h>
#include <RooRealVar.h>
#include <RooArgSet.h>
#include <RooFitResult.h>
#include <RooAbsPdf.h>

struct DataSetResource {
	RooRealVar weight;
	std::string weight_id;

	std::vector<RooRealVar*> observables;
	std::vector<std::string> observables_id;

	RooDataSet dataset;
};

struct ModelResource{

	std::vector<RooRealVar*> observables;
	std::vector<std::string> observables_id;

	std::vector<RooRealVar*> firParameters;
	std::vector<std::string> firParameters_id;

	RooAbsPdf abspdf;
};

class FitManager {
private:
	std::unordered_map<std::string, RooRealVar> observables;
	std::unordered_map<std::string, RooRealVar> firParameters;
	std::unordered_map<std::string, DataSetResource> dataSets;
	std::unordered_map<std::string, ModelResource> models;
	std::unordered_map<std::string, FitResource> fits;
public:
	void DefineFitParameter(const std::string& parameter_id_, const std::string& title_, double minValue_, double maxValue_, const std::string& unit_ = "", bool setConst_ = false);
	void DefineObservable(const std::string& observable_id_, const std::string& title_, double minValue_, double maxValue_, const std::string& unit_ = "");
	void DefineDataSet(const std::string& dataset_id_, const std::string& title_, const std::vector<std::string>& observable_ids_);
	void DefineModel(const std::string& model_id_, const std::string& title_, const std::vector<std::string>& observable_ids_);

	RooRealVar* GetFitParameter(const std::string& parameter_id_);
	RooRealVar* GetObservable(const std::string& observable_id_);
	RooDataSet* GetDataSet(const std::string& dataset_id_);

	void Fit();
	void SaveFitPlot();
	void SaveFitResult();
};

inline void FitManager::DefineFitParameter(const std::string& parameter_id_, const std::string& title_, double minValue_, double maxValue_, const std::string& unit_ = ""){
	if(firParameters.find(parameter_id_) != firParameters.end()){
		printf("[FitManager::DefineFitParameter] There is already fit parameter %s\n", parameter_id_.c_str());
		exit(1);
	}

	RooRealVar fit_parameter_temp(parameter_id_.c_str(), title_.c_str(), minValue_, maxValue_, unit_.c_str());
	fitParameters.push_back(fit_parameter_temp);
}

inline void FitManager::DefineObservable(const std::string& observable_id_, const std::string& title_, double minValue_, double maxValue_, const std::string& unit_, bool setConst_ = false){
	if(observables.find(observable_id_) != observables.end()){
		printf("[FitManager::DefineObservable] There is already observable %s\n", observable_id_.c_str());
		exit(1);
	}

	RooRealVar observable_temp(observable_id_.c_str(), title_.c_str(), minValue_, maxValue_, unit_.c_str());
	observable_temp.setConstant(setConst_);
	observables.push_back(observable_temp);
}

inline void FitManager::DefineDataSet(const std::string& dataset_id_, const std::string& title_, const std::vector<std::string>& observable_ids_){
    if(dataSets.find(dataset_id_) != dataSets.end()){
        printf("[FitManager::DefineDataSet] There is already dataset %s\n", dataset_id_.c_str());
		exit(1);
	}

	std::string weight_id = dataset_id_ + "_weight";
	RooRealVar weight_temp(weight_id.c_str(), weight_id.c_str(), 0.0, 100.0);

	std::vector<RooRealVar*> observables_temp;
	RooArgSet argset_temp;
	for(const std::string& observable_id : observable_ids_){
		RooRealVar* observable_temp = GetObservable(observable_id);
		observables_temp.push_back(observable_temp);
		argset_temp.add(*observable_temp);
	}
	argset_temp.add(weight_temp);

	RooDataSet dataset_temp(dataset_id_.c_str(), title_.c_str(), argset_temp, RooFit::WeightVar(weight_id.c_str()));

	DataSetResource datasetresource_temp = {weight_temp, weight_id, observables_temp, observable_ids_, dataset_temp};
	dataSets.push_back(datasetresource_temp);
}

inline void FitManager::DefineModel(const std::string& model_id_, const std::string& title_, const std::vector<std::string>& observable_ids_){

}

inline RooRealVar* FitManager::GetFitParameter(const std::string& parameter_id_){
	if(firParameters.find(parameter_id_) == firParameters.end()){
        printf("[FitManager::GetFitParameter] Cannot find fit parameter %s\n", parameter_id_.c_str());
		exit(1);
	}

	return &firParameters[parameter_id_];
}

inline RooRealVar* FitManager::GetObservable(const std::string& observable_id_){
	if(observables.find(observable_id_) == observables.end()){
        printf("[FitManager::GetObservable] Cannot find observable %s\n", observable_id_.c_str());
		exit(1);
	}

	return &observables[observable_id_];
}

inline RooDataSet* FitManager::GetDataSet(const std::string& dataset_id_){
	if(dataSets.find(dataset_id_) == dataSets.end()){
        printf("[FitManager::GetDataSet] Cannot find dataset %s\n", dataset_id_.c_str());
		exit(1);
	}

	return &dataSets[dataset_id_];
}

#endif 

