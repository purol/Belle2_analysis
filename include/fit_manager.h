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

class FitManager {
private:
	std::unordered_map<std::string, std::unique_ptr<RooRealVar>> observables;
	std::unordered_map<std::string, std::unique_ptr<RooRealVar>> firParameters;

	std::vector<RooRealVar> constantList;
	std::vector<RooDataSet> dataSetList;
	std::vector<RooAbsPdf> fitFunctionList;
public:
	void DefineFitParameter(const std::string& parameter_id, const std::string& title, double minValue, double maxValue, const std::string& unit = "");
	void DefineConstantParameter(const std::string& parameter_id, const std::string& title, double value, const std::string& unit = "");
	void DefineObservable(const std::string& observable_id, const std::string& title, double minimum, double maximum, const std::string& unit = "");
	void DefineDataSet();
	void Fit();
};

#endif 

