#ifndef FIT_MANAGER_H
#define FIT_MANAGER_H

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <optional>
#include <memory>

#include <RooAddPdf.h>
#include <RooProdPdf.h>
#include <RooGenericPdf.h>
#include <RooSimultaneous.h>

#include <RooAbsPdf.h>
#include <RooAbsArg.h>
#include <RooRealVar.h>
#include <RooCategory.h>
#include <RooArgList.h>
#include <RooDataSet.h>
#include <RooFitResult.h>
#include <RooWorkspace.h>
#include <RooAbsRealLValue.h>
#include <RooArgSet.h>
#include <RooPlot.h>
#include <RooFit.h>
#include <TTree.h>

#include <RooGaussian.h>
#include <RooBifurGauss.h>
#include <RooCrystalBall.h>
#include <RooJohnson.h>
#include <RooCBShape.h>
#include <RooArgusBG.h>
#include <RooPolynomial.h>
#include <RooExponential.h>
#include <RooChebychev.h>
#include <RooBernstein.h>
#include <RooBreitWigner.h>
#include <RooVoigtian.h>
#include <RooBukinPdf.h>
#include <RooNovosibirsk.h>

#include <TCanvas.h>
#include <TFile.h>

struct WorkingDataSet {
	std::vector<std::string> observable_ids;
	RooRealVar weight_variable;
	RooDataSet dataset;
};

struct ModelOptions {
	// RooPolynomial
	int lowest_order = 1;

	// RooJohnson. If not set, ROOT's default threshold is used.
	std::optional<double> johnson_mass_threshold;

	// RooExponential
	bool negateCoefficient = false;

	// RooVoigtian
	bool doFast = false;
};

struct ModelDefinition {
	std::vector<std::string> observable_ids;
	std::vector<std::string> parameter_ids;

	// Exact-size model: min_parameters == max_parameters.
	// Variadic model: max_parameters == std::nullopt
	std::size_t N_min_parameters = 0;
	std::optional<std::size_t> N_max_parameters;
};

struct FitOptions {
	std::string range;
	std::pair<std::string, std::string> Minimizer;
	std::vector<std::string> Minos;
	int strategy = 1;
	bool SumW2Error = false;
	bool extended = false;
	std::string Offset = "none";

	int print_level = 0;
};

class FitManager {
private:
    std::unique_ptr<RooWorkspace> created_workspace;
	std::string loaded_filename;
    std::unique_ptr<TFile> loaded_file;
    RooWorkspace* workspace = nullptr;

	std::unordered_map<std::string, WorkingDataSet> working_datasets;
	std::unordered_map<std::string, std::unique_ptr<RooAbsReal>> fit_nlls;

	void ImportChecked(const RooAbsArg& object_);
	void ImportChecked(const TObject& object_, const std::string& aliasName_);
	void ImportDataChecked(const RooAbsData& data_);
	void EnsureWorkspaceNameAvailable(const std::string& id_) const;
	void EnsureFitIdAvailable(const std::string& fit_id_) const;
	void EnsureDataInWorkspace(const std::string& dataset_id_);

	static const std::unordered_map<std::string, ModelDefinition>& ModelDefinitions();

	static void ValidateModelArguments(const std::string & model_type_, const ModelDefinition& definition_, std::size_t observable_count_, std::size_t parameter_count_);

public:
	explicit FitManager(const std::string& workspace_name_);
	explicit FitManager(const std::string& filename_, const std::string& workspace_name_);

	FitManager(const FitManager&) = delete;
	FitManager& operator=(const FitManager&) = delete;
	FitManager(FitManager&&) = delete;
	FitManager& operator=(FitManager&&) = delete;

	~FitManager() = default;

	void DefineObservable(const std::string& id_, const std::string& title_, double minimum_, double maximum_, const std::string& unit_ = "");
	void DefineDataSet(const std::string& id_, const std::vector<std::string> observable_ids_);
	void DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_ = "");
	void DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_ = "");
	void DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::string>& states_);

	void SetParameterConstant(const std::string& id_, bool constant_ = true);
	void SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_);

	RooAbsArg* GetRooAbsArg(const std::string& id_);
	const RooAbsArg* GetRooAbsArg(const std::string& id_) const;
	RooAbsReal* GetRooAbsReal(const std::string& id_);
	const RooAbsReal* GetRooAbsReal(const std::string& id_) const;
	RooRealVar* GetRooRealVar(const std::string& id_);
	const RooRealVar* GetRooRealVar(const std::string& id_) const;
	RooCategory* GetRooCategory(const std::string& id_);
	const RooCategory* GetRooCategory(const std::string& id_) const;
	RooAbsPdf* GetPdf(const std::string& id_);
	const RooAbsPdf* GetPdf(const std::string& id_) const;
	RooAbsData* GetData(const std::string& id_);
	const RooAbsData* GetData(const std::string& id_) const;
	RooFitResult* GetFitResult(const std::string& fit_id_);
	const RooFitResult* GetFitResult(const std::string& fit_id_) const;

	void FinalizeDataSet(const std::string& dataset_id_);
	void FinalizeAllDataSet();

	void DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options = {});
	void DefineAddModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, const std::vector<std::string>& coefficient_ids_, bool recursive_fractions_ = false);
	void DefineProductModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, double cutoff_ = 0.0);
	void DefineGenericModel(const std::string& model_id_, const std::string& expression_, const std::vector<std::string>& argument_ids_);
	void DefineSimultaneousModel(const std::string& model_id_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_pdf_ids_);

	void ImportRooAbsArg(const RooAbsArg& object_);
	void ImportPdf(const RooAbsPdf& pdf_);
	void ImportData(const RooAbsData& data);

	void Fit(const std::string& fit_id_, const std::string dataset_id_, const std::string& model_id_, const FitOptions& options_ = {});
	void CreateNLL(const std::string& nll_id_, const std::string& dataset_id_, const std::string& model_id_, const FitOptions& options_ = {});

	void PlotNLL(const std::string& nll_id_, const std::string& parameter_id_, const std::string& plot_name_);
	void PlotProfileNLL(const std::string& nll_id_, const std::string& poi_id_, const std::string& plot_name_);

	void SaveWorkspace(const std::string& filename_);
	void LoadWorkspace(const std::string& filename_, const std::string& workspace_name_);

	void ExportFitResult(const std::string& filename_, const std::vector<std::string>& fit_ids_);

};

inline FitManager::FitManager(const std::string& workspace_name_){
	created_workspace = std::make_unique<RooWorkspace>(workspace_name_.c_str(), workspace_name_.c_str());
	workspace = created_workspace.get();
}

inline FitManager::FitManager(const std::string& filename_, const std::string& workspace_name_){
	LoadWorkspace(filename_, workspace_name_);
}

inline void FitManager::ImportChecked(const RooAbsArg& object_) {
	EnsureWorkspaceNameAvailable(object_.GetName());

	if (workspace->import(object_)) {
		printf("[FitManager::ImportChecked] failed to import RooAbsArg %s", object_.GetName());
		exit(1);
	}
}

inline void FitManager::ImportChecked(const TObject& object_, const std::string& aliasName_) {
	EnsureWorkspaceNameAvailable(aliasName_);

	if (workspace->import(object_, aliasName_.c_str())) {
		printf("[FitManager::ImportChecked] failed to import RooAbsArg %s", object_.GetName());
		exit(1);
	}
}

inline void FitManager::ImportDataChecked(const RooAbsData& data_) {
	EnsureWorkspaceNameAvailable(data_.GetName());

	if (workspace->import(data_)) {
		printf("[FitManager::ImportDataChecked] failed to import dataset %s", data_.GetName());
		exit(1);
	}
}

inline void FitManager::EnsureWorkspaceNameAvailable(const std::string& id_) const {
	if (id_.empty()) {
		printf("[FitManager::EnsureWorkspaceNameAvailable] Object ID must not be empty.\n");
		exit(1);
	}

	if ((workspace->arg(id_.c_str()) != nullptr) || (workspace->data(id_.c_str()) != nullptr) || (workspace->genobj(id_.c_str()) != nullptr) || (working_datasets.find(id_) != working_datasets.end())) {
		printf("[FitManager::EnsureWorkspaceNameAvailable] Ojbect %s already exists.\n", id_.c_str());
		exit(1);
	}
}

inline void FitManager::EnsureFitIdAvailable(const std::string& fit_id_) const {
	if(fit_id_.empty()){
		printf("[FitManager::EnsureFitIdAvailable] fit ID must not be empty.\n");
		exit(1);
	}

	if(workspace->obj(fit_id_.c_str()) != nullptr){
		printf("[FitManager::EnsureFitIdAvailable] fit %s already exists.\n", fit_id_.c_str());
		exit(1);
	}
}

inline void FitManager::EnsureDataInWorkspace(const std::string& dataset_id_){
	if (workspace->data(dataset_id_.c_str()) != nullptr){
		return;
	}

	std::unordered_map<std::string, WorkingDataSet>::iterator it = working_datasets.find(dataset_id_);

	if(it != working_datasets.end()){
		FinalizeDataSet(dataset_id_);
		return;
	}

	printf("[FitManager::EnsureDataInWorkspace] dataset %s does not exists.\n", dataset_id_.c_str());
	exit(1);
}

inline const std::unordered_map<std::string, ModelDefinition>& FitManager::ModelDefinitions() {

	static const std::unordered_map<std::string, ModelDefinition> definitions = {
		{ // RooGaussian (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_mean, RooAbsReal &_sigma)
			"RooGaussian",
			{ {"x"}, {"mean", "sigma"}, 2, 2}
        },
		{ // RooBifurGauss (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_mean, RooAbsReal &_sigmaL, RooAbsReal &_sigmaR)
			"RooBifurGauss",
			{ {"x"}, {"mean", "sigmaL", "sigmaR"}, 3, 3}
		},
		{ // RooCrystalBall (const char *name, const char *title, RooAbsReal &x, RooAbsReal &x0, RooAbsReal &sigmaL, RooAbsReal &sigmaR, RooAbsReal &alphaL, RooAbsReal &nL, RooAbsReal &alphaR, RooAbsReal &nR)
			"RooCrystalBall",
			{ {"x"}, {"x0", "sigmaL", "sigmaR", "alphaL", "nL", "alphaR", "nR" }, 7, 7}
		},
		{ // RooJohnson (const char *name, const char *title, RooAbsReal &mass, RooAbsReal &mu, RooAbsReal &lambda, RooAbsReal &gamma, RooAbsReal &delta, double massThreshold=-std::numeric_limits< double >::max())
			"RooJohnson",
			{ {"x"}, {"mu", "lambda", "gamma", "delta"}, 4, 4}
		},
		{ // RooCBShape (const char *name, const char *title, RooAbsReal &_m, RooAbsReal &_m0, RooAbsReal &_sigma, RooAbsReal &_alpha, RooAbsReal &_n)
			"RooCBShape",
			{ {"x"}, {"m0", "sigma", "alpha", "n"}, 4, 4}
		},
		{ // RooArgusBG (const char *name, const char *title, RooAbsReal &_m, RooAbsReal &_m0, RooAbsReal &_c, RooAbsReal &_p)
			"RooArgusBG",
			{ {"x"}, {"m0", "c", "p"}, 3, 3}
		},
		{ // RooPolynomial (const char *name, const char *title, RooAbsReal &_x, const RooArgList &_coefList, Int_t lowestOrder=1)
			"RooPolynomial",
			{ {"x"}, {"coefficient"}, 0, std::nullopt}
		},
		{ // RooExponential (const char *name, const char *title, RooAbsReal &variable, RooAbsReal &coefficient, bool negateCoefficient=false)
			"RooExponential",
			{{"x"}, {"c"}, 1, 1}
		},
		{ // RooChebychev (const char *name, const char *title, RooAbsReal &_x, const RooArgList &_coefList)
			"RooChebychev",
			{{"x"}, {"coefficient"}, 0, std::nullopt}
		},
		{ // RooBernstein (const char *name, const char *title, RooAbsRealLValue &_x, const RooArgList &_coefList)
			"RooBernstein",
			{{"x"}, {"coefficient"}, 1, std::nullopt}
		},
		{ // RooBreitWigner (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_mean, RooAbsReal &_width)
			"RooBreitWigner",
			{{"x"}, {"mean", "width"}, 2, 2}
		},
		{ // RooVoigtian (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_mean, RooAbsReal &_width, RooAbsReal &_sigma, bool doFast=false)
			"RooVoigtian",
			{{"x"}, {"mean", "width", "sigma"}, 3, 3}
		},
		{ // RooBukinPdf (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_Xp, RooAbsReal &_sigp, RooAbsReal &_xi, RooAbsReal &_rho1, RooAbsReal &_rho2)
			"RooBukinPdf",
			{{"x"}, {"Xp", "sigp", "xi", "rho1", "rho2"}, 5, 5}
		},
		{ // RooNovosibirsk (const char *name, const char *title, RooAbsReal &_x, RooAbsReal &_peak, RooAbsReal &_width, RooAbsReal &_tail)
			"RooNovosibirsk",
			{{"x"}, {"peak", "width", "tail"}, 3, 3}
		}
	};

	return definitions;
}

inline void FitManager::ValidateModelArguments(const std::string & model_type_, const ModelDefinition& definition_, std::size_t observable_count_, std::size_t parameter_count_){
	if (observable_count_ != definition_.observable_ids.size()){
		printf("[FitManager::ValidateModelArguments] model type %s requires %zu observable(s), but %zu were supplied.\n", model_type_.c_str(), definition_.observable_ids.size(), observable_count_);
		exit(1);
	}

	if(parameter_count_ < definition_.N_min_parameters){
		printf("[FitManager::ValidateModelArguments] model type %s requires at least %zu parameter(s), but %zu were supplied.\n", model_type_.c_str(), definition_.N_min_parameters, parameter_count_);
		exit(1);
	}

		if((definition_.N_max_parameters.has_value()) && (parameter_count_ > definition_.N_max_parameters.value())){
		printf("[FitManager::ValidateModelArguments] model type %s requires at most %zu parameter(s), but %zu were supplied.\n", model_type_.c_str(), definition_.N_max_parameters.value(), parameter_count_);
		exit(1);
	}
}

inline void FitManager::CreateNLL(const std::string& nll_id_, const std::string& dataset_id_, const std::string& model_id_, const FitOptions& options_) {

	if(fit_nlls.find(nll_id_) != fit_nlls.end()){
		printf("[FitManager::CreateNLL] nll %s already exists.\n", nll_id_.c_str());
		exit(1);
	}

    if((options_.Offset != "none") && (options_.Offset != "initial") && (options_.Offset != "bin")){
		printf("[FitManager::Fit] offset must be none, initial, or bin.\n");
		exit(1);
	}

	EnsureDataInWorkspace(dataset_id_);

	RooAbsPdf* pdf = GetPdf(model_id_);
	RooAbsData* data = GetData(dataset_id_);

	RooCmdArg range_arg = options_.range.empty() ? RooCmdArg::none() : RooFit::Range(options_.range.c_str());

    RooAbsReal* nll = pdf->createNLL(
		*data,
		
		range_arg,

		RooFit::Extended(options_.extended),
		RooFit::Offset(options_.Offset)
	);

	fit_nlls.emplace(nll_id_, std::unique_ptr<RooAbsReal>(nll));

}

inline void FitManager::DefineObservable(const std::string& id_, const std::string& title_, double minimum_, double maximum_, const std::string& unit_) {
	if (minimum_ > maximum_) {
		printf("[FitManager::DefineObservable] minimum is larger than maximum\n");
		exit(1);
	}

	RooRealVar observable(id_.c_str(), title_.c_str(), minimum_, maximum_, unit_.c_str());

	ImportChecked(observable);
}

inline void FitManager::DefineDataSet(const std::string& id_, const std::vector<std::string> observable_ids_) {
	EnsureWorkspaceNameAvailable(id_);

	if (observable_ids_.empty()) {
		printf("[FitManager::DefineDataSet] At least one observable is required.\n");
		exit(1);
	}

	RooArgSet observables_in_dataset;

	for (const std::string& observable_id : observable_ids_) {
		RooAbsArg* observable = GetRooAbsArg(observable_id);
		observables_in_dataset.add(*observable);
	}

	const std::string weight_id = id_ + "__weight";
	RooRealVar weight_variable(weight_id.c_str(), "weight", 1.0, -100.0, 100.0);
	observables_in_dataset.add(weight_variable);

	RooDataSet dataset = RooDataSet(id_.c_str(), id_.c_str(), observables_in_dataset, RooFit::WeightVar(weight_id.c_str()));

	WorkingDataSet workingdataset = { observable_ids_, weight_variable, dataset };

	working_datasets.emplace(id_, std::move(workingdataset));
}

inline void FitManager::DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_) {
	if (minimum_ > maximum_) {
		printf("[FitManager::DefineFitParameter] minimum is larger than maximum\n");
		exit(1);
	}

	if ((init_value_ < minimum_) || (init_value_ > maximum_)) {
		printf("[FitManager::DefineFitParameter] Initial value is outside the allowed range\n");
		exit(1);
	}

	RooRealVar parameter(id_.c_str(), title_.c_str(), init_value_, minimum_, maximum_, unit_.c_str());

	ImportChecked(parameter);
}

inline void FitManager::DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_) {
	RooRealVar parameter(id_.c_str(), title_.c_str(), value_, unit_.c_str());

	parameter.setConstant(true);
	ImportChecked(parameter);
}

inline void FitManager::SetParameterConstant(const std::string& id_, bool constant_) {
	GetRooRealVar(id_)->setConstant(constant_);
}

inline void FitManager::SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_) {
	if (range_name_.empty()) {
		printf("[FitManager::SetRange] range name must not be empty\n");
		exit(1);
	}

	if (minimum_ > maximum_) {
		printf("[FitManager::SetRange] minimum must be smaller than maximum\n");
		exit(1);
	}

	GetRooRealVar(variable_id_)->setRange(range_name_.c_str(), minimum_, maximum_);
}

inline void FitManager::DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::string>& states_) {
	if (states_.empty()) {
		printf("[FitManager::DefineCategory] at least one state is required.\n");
		exit(1);
	}

	RooCategory category(id_.c_str(), title_.c_str());

	for (const std::string& state : states_) {
		if (state.empty()) {
			printf("[FitManager::DefineCategory] state label must not be empty.\n");
			exit(1);
		}

		if (category.defineType(state)) {
			printf("[FitManager::DefineCategory] failed to define state %s", state.c_str());
			exit(1);
		}
	}

	ImportChecked(category);
}

inline RooAbsArg* FitManager::GetRooAbsArg(const std::string& id_) {
	RooAbsArg* arg = workspace->arg(id_.c_str());

	if (!arg) {
		printf("[FitManager::GetRooAbsArg] RooAbsArg %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return arg;
}

inline const RooAbsArg* FitManager::GetRooAbsArg(const std::string& id_) const {
	RooAbsArg* arg = workspace->arg(id_.c_str());

	if (!arg) {
		printf("[FitManager::GetRooAbsArg] RooAbsArg %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return arg;
}

inline RooAbsReal* FitManager::GetRooAbsReal(const std::string& id_) {
	RooAbsReal* real = dynamic_cast<RooAbsReal*>(GetRooAbsArg(id_));

	if (!real) {
		printf("[FitManager::GetRooAbsReal] object %s is not a RooAbsReal.\n", id_.c_str());
		exit(1);
	}

	return real;
}

inline const RooAbsReal* FitManager::GetRooAbsReal(const std::string& id_) const {
	const RooAbsReal* real = dynamic_cast<const RooAbsReal*>(GetRooAbsArg(id_));

	if (!real) {
		printf("[FitManager::GetRooAbsReal] object %s is not a RooAbsReal.\n", id_.c_str());
		exit(1);
	}

	return real;
}

inline RooRealVar* FitManager::GetRooRealVar(const std::string& id_) {
	RooRealVar* variable = workspace->var(id_.c_str());

	if (!variable) {
		printf("[FitManager::GetRooRealVar] RooRealVar %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return variable;
}

const inline RooRealVar* FitManager::GetRooRealVar(const std::string& id_) const {
	RooRealVar* variable = workspace->var(id_.c_str());

	if (!variable) {
		printf("[FitManager::GetRooRealVar] RooRealVar %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return variable;
}

inline RooCategory* FitManager::GetRooCategory(const std::string& id_) {
	RooCategory* category = workspace->cat(id_.c_str());

	if (!category) {
		printf("[FitManager::GetRooCategory] RooCategory %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return category;
}

inline const RooCategory* FitManager::GetRooCategory(const std::string& id_) const {
	RooCategory* category = workspace->cat(id_.c_str());

	if (!category) {
		printf("[FitManager::GetRooCategory] RooCategory %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return category;
}

inline RooAbsPdf* FitManager::GetPdf(const std::string& id_) {
	RooAbsPdf* pdf = workspace->pdf(id_.c_str());

	if (!pdf) {
		printf("[FitManager::GetPdf] PDF %s does not exist\n", id_.c_str());
		exit(1);
	}

	return pdf;
}

inline const RooAbsPdf* FitManager::GetPdf(const std::string& id_) const {
	RooAbsPdf* pdf = workspace->pdf(id_.c_str());

	if (!pdf) {
		printf("[FitManager::GetPdf] PDF %s does not exist\n", id_.c_str());
		exit(1);
	}

	return pdf;
}

inline RooAbsData* FitManager::GetData(const std::string& id_){
	auto it = working_datasets.find(id_);

    if(it != working_datasets.end()){
		return &it->second.dataset;
	}

	RooAbsData* data = workspace->data(id_.c_str());

	if (!data){
		printf("[FitManager::GetData] data %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return data;
}

inline const RooAbsData* FitManager::GetData(const std::string& id_) const {
	auto it = working_datasets.find(id_);

    if(it != working_datasets.end()){
		return &it->second.dataset;
	}

	RooAbsData* data = workspace->data(id_.c_str());

	if (!data){
		printf("[FitManager::GetData] data %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return data;
}

inline RooFitResult* FitManager::GetFitResult(const std::string& fit_id_){
	RooFitResult* result = dynamic_cast<RooFitResult*>(workspace->obj(fit_id_.c_str()));

	if (!result){
		printf("[FitManager::GetFitResult] fit result %s does not exist in the workspace.\n", fit_id_.c_str());
		exit(1);
	}

	return result;
}

inline const RooFitResult* FitManager::GetFitResult(const std::string& fit_id_) const{
	RooFitResult* result = dynamic_cast<RooFitResult*>(workspace->obj(fit_id_.c_str()));

	if (!result){
		printf("[FitManager::GetFitResult] fit result %s does not exist in the workspace.\n", fit_id_.c_str());
		exit(1);
	}

	return result;
}

inline void FitManager::FinalizeDataSet(const std::string& dataset_id_) {
	std::unordered_map<std::string, WorkingDataSet>::iterator it = working_datasets.find(dataset_id_);

	if (it == working_datasets.end()) {
		printf("[FitManager::FinalizeDataSet] dataset %s does not exist.\n", dataset_id_.c_str());
		exit(1);
	}

	RooDataSet& dataset = it->dataset;

	if (workspace->data(dataset_id_.c_str()) != nullptr) {
		printf("[FitManager::FinalizeDataSet] dataset %s already exists in the workspace.\n", dataset_id_.c_str());
		exit(1);
	}

	if (workspace->import(dataset)) {
		printf("[FitManager::FinalizeDataSet] fail to import dataset %s in the workspace.\n", dataset_id_.c_str());
		exit(1);
	}

	working_datasets.erase(it);
}

inline void FitManager::FinalizeAllDataSet() {
	std::vector<std::string> dataset_ids;
	dataset_ids.reserve(working_datasets.size());

	for (const std::pair<std::string, WorkingDataSet>& dataset : working_datasets) {
		dataset_ids.push_back(dataset.first);
	}

	for (const std::string& dataset_id : dataset_ids) {
		FinalizeDataSet(dataset_id);
	}
}

inline void FitManager::DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options_) {
	EnsureWorkspaceNameAvailable(model_id_);

	auto it = ModelDefinitions().find(model_type_);

	if(it == ModelDefinitions().end()){
		printf("[FitManager::DefineModel] cannot find model type %s. Use ImportPdf() for an arbitrary RooAbsPdf.\n", model_type_.c_str());
		exit(1);
	}

	const ModelDefinition& definition = it->second;

    ValidateModelArguments(model_type_, definition, observable_ids_.size(), parameter_ids_.size());

	std::vector<RooAbsReal*> observables;
	observables.reserve(observable_ids_.size());

	for(const std::string& observable_id : observable_ids_){
		observables.push_back(GetRooAbsReal(observable_id));
	}

	std::vector<RooAbsReal*> parameters;
	parameters.reserve(parameter_ids_.size());

	for(const std::string& parameter_id : parameter_ids_){
		parameters.push_back(GetRooAbsReal(parameter_id));
	}

	std::unique_ptr<RooAbsPdf> pdf;

	if (model_type_ == "RooGaussian"){
		pdf = std::make_unique<RooGaussian>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1)
		);
	}
	else if(model_type_ == "RooBifurGauss"){
		pdf = std::make_unique<RooBifurGauss>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2)
		);
	}
	else if (model_type_ == "RooCrystalBall") {
		pdf = std::make_unique<RooCrystalBall>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2),
			*parameters.at(3),
			*parameters.at(4),
			*parameters.at(5),
			*parameters.at(6)
		);
	}
	else if (model_type_ == "RooJohnson") {
		if (options_.johnson_mass_threshold.has_value()) {
			pdf = std::make_unique<RooJohnson>(
				model_id_.c_str(),
				model_id_.c_str(),
				*observables.at(0),
				*parameters.at(0),
				*parameters.at(1),
				*parameters.at(2),
				*parameters.at(3),
				options_.johnson_mass_threshold.value()
			);
		}
		else {
			pdf = std::make_unique<RooJohnson>(
				model_id_.c_str(),
				model_id_.c_str(),
				*observables.at(0),
				*parameters.at(0),
				*parameters.at(1),
				*parameters.at(2),
				*parameters.at(3)
			);
		}
	}
	else if (model_type_ == "RooCBShape") {
		pdf = std::make_unique<RooCBShape>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2),
			*parameters.at(3)
		);
	}
	else if (model_type_ == "RooPolynomial") {
		RooArgList coefficients;

		for (RooAbsReal* parameter : parameters) {
			coefficients.add(*parameter);
		}

		pdf = std::make_unique<RooPolynomial>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			coefficients,
			options_.lowest_order
		);
	}
	else if (model_type_ == "RooExponential") {
		pdf = std::make_unique<RooExponential>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			options_.negateCoefficient
		);
	}
	else if (model_type_ == "RooChebychev") {
		RooArgList coefficients;

		for (RooAbsReal* parameter : parameters) {
			coefficients.add(*parameter);
		}

		pdf = std::make_unique<RooChebychev>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			coefficients
		);
	}
	else if (model_type_ == "RooBernstein") {
		RooArgList coefficients;

		for (RooAbsReal* parameter : parameters) {
			coefficients.add(*parameter);
		}

        auto* observable =
            dynamic_cast<RooAbsRealLValue*>(
               observables.at(0)
            );

        if (!observable) {
            printf(
                "[FitManager::DefineModel] "
                "RooBernstein requires a RooAbsRealLValue observable.\n"
            );
            exit(1);
        }

		pdf = std::make_unique<RooBernstein>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observable,
			coefficients
		);
	}
	else if (model_type_ == "RooBreitWigner") {
		pdf = std::make_unique<RooBreitWigner>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1)
		);
	}
	else if (model_type_ == "RooVoigtian") {
		pdf = std::make_unique<RooVoigtian>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2),
			options_.doFast
		);
	}
	else if (model_type_ == "RooBukinPdf") {
		pdf = std::make_unique<RooBukinPdf>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2),
			*parameters.at(3),
			*parameters.at(4)
		);
	}
	else if (model_type_ == "RooNovosibirsk") {
		pdf = std::make_unique<RooNovosibirsk>(
			model_id_.c_str(),
			model_id_.c_str(),
			*observables.at(0),
			*parameters.at(0),
			*parameters.at(1),
			*parameters.at(2)
		);
	}
    else if (model_type_ == "RooArgusBG") {
        pdf = std::make_unique<RooArgusBG>(
            model_id_.c_str(),
            model_id_.c_str(),
            *observables.at(0),
            *parameters.at(0),
            *parameters.at(1),
            *parameters.at(2)
        );
    }
	else {
		printf("[FitManager::DefineModel] model type %s is registered but has not builder.\n", model_type_.c_str());
		exit(1);
	}

	ImportPdf(*pdf);
}

inline void FitManager::DefineAddModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, const std::vector<std::string>& coefficient_ids_, bool recursive_fractions_) {
	/*
	* recursive_fraction = false:
	* f = c1*f1 + c2*f2 + (1-c1-c2)*f3
	* 
	* recursive_fraction = true:
	* f = c1*f1 + (1-c1)*c2*f2 + (1-c1)*(1-c2)*f3
	* 
	* NOTE: Each pdfs are assumed to be normalized. The normalization for the final PDF is not done.
	* NOTE: If Ncoeff == Npdf - 1, the coefficient for the last pdf is calculated from \sigma c = 1, so it is automatically normalized
	*/
	EnsureWorkspaceNameAvailable(model_id_);

	const std::size_t n_pdf = pdf_ids_.size();
	const std::size_t n_coef = coefficient_ids_.size();

	if (n_pdf < 1) {
		printf("[FitManager::DefineAddModel] at least one pdf is required.\n");
		exit(1);
	}

	if (!((n_coef == n_pdf) || (n_coef + 1 == n_pdf))) {
		printf("[FitManager::DefineAddModel] number of coefficients must be Npdf or Npdf - 1.\n");
		exit(1);
	}

	RooArgList pdfs;
	RooArgList coefficients;

	for (const std::string& pdf_id : pdf_ids_) {
		pdfs.add(*GetPdf(pdf_id));
	}

	for (const std::string& coefficient_id : coefficient_ids_) {
		coefficients.add(*GetRooAbsReal(coefficient_id));
	}

	RooAddPdf model(model_id_.c_str(), model_id_.c_str(), pdfs, coefficients, recursive_fractions_);

	ImportPdf(model);

}

inline void FitManager::DefineProductModel(const std::string& model_id_, const std::vector<std::string>& pdf_ids_, double cutoff_) {
	/*
	* cut_off: parameter for the optimization. If pdf value is smaller than cut_off, it just becomes zero
	* 
	* NOTE: If pdfs share the variables, the normalization is done.
	*/

	EnsureWorkspaceNameAvailable(model_id_);

	if (pdf_ids_.empty()) {
		printf("[FitManager::DefineProductModel] at least one pdf is required.\n");
		exit(1);
	}

	if (cutoff_ < 0.0) {
		printf("[FitManager::DefineProductModel] cutoff must be non-negative.\n");
		exit(1);
	}

	RooArgList pdfs;

	for (const std::string& pdf_id : pdf_ids_) {
		pdfs.add(*GetPdf(pdf_id));
	}

	RooProdPdf model(model_id_.c_str(), model_id_.c_str(), pdfs, cutoff_);

	ImportPdf(model);
}

inline void FitManager::DefineGenericModel(const std::string& model_id_, const std::string& expression_, const std::vector<std::string>& argument_ids_) {
	/*
	* example usage:
	* fit_manager.DefineGenericModel("my_gaussian", "exp(-0.5 * pow((@0 - @1) / @2, 2))", {"x", "mean", "sigma"} );
	* expression follows the valid TFormula
	* 
	* NOTE: normalization is automatically done
	*/

	EnsureWorkspaceNameAvailable(model_id_);

	if (expression_.empty()) {
		printf("[FitManager::DefineGenericModel] expression must not be empty.\n");
		exit(1);
	}

	if (argument_ids_.empty()) {
		printf("[FitManager::DefineGenericModel] at least one argument is required.\n");
		exit(1);
	}

	RooArgList arguments;

	for (const std::string& argument_id : argument_ids_) {
		arguments.add(*GetRooAbsArg(argument_id));
	}

	RooGenericPdf model(model_id_.c_str(), model_id_.c_str(), expression_.c_str(), arguments);

	ImportPdf(model);
}

inline void FitManager::DefineSimultaneousModel(const std::string& model_id_, const std::string& category_id_, const std::vector<std::pair<std::string, std::string>>& state_pdf_ids_) {
	/*
	* example usage:
	* fit_manager.DefineCategory( "channel", "channel", { {"electron", "muon"} } );
	* fit_manager.DefineModel( "electron_pdf", "RooGaussian", "M", {"mean", "sigma_e"} );
	* fit_manager.DefineModel( "muon_pdf", "RooGaussian", "M", {"mean", "sigma_mu"} );
	* 
	* fit_manager.DefineSimultaneousModel( "sim_pdf", "channel", { {"electron", "electron_pdf"}, {"muon", "muon_pdf"} } );
	*/

	EnsureWorkspaceNameAvailable(model_id_);

	if (state_pdf_ids_.empty()) {
		printf("[FitManager::DefineSimultaneousModel] at least one state/PDF pair is required.\n");
		exit(1);
	}

	RooCategory* category = GetRooCategory(category_id_);

	RooSimultaneous model(model_id_.c_str(), model_id_.c_str(), *category);

	for (const std::pair<std::string, std::string> state_pdf_id : state_pdf_ids_) {
		const std::string& state = state_pdf_id.first;
		const std::string& pdf_id = state_pdf_id.second;

		if (!category->hasLabel(state.c_str())) {
			printf("[FitManager::DefineSimultaneousModel] category %s has no state %s.\n", category_id_.c_str(), state.c_str());
			exit(1);
		}

		if (model.addPdf(*GetPdf(pdf_id), state.c_str())) {
			printf("[FitManager::DefineSimultaneousModel] fail to add PDF %s for state %s.\n", pdf_id.c_str(), state.c_str());
			exit(1);
		}
	}

	ImportPdf(model);
}

inline void FitManager::ImportRooAbsArg(const RooAbsArg& object_) {
	ImportChecked(object_);
}

inline void FitManager::ImportPdf(const RooAbsPdf& pdf_) {
	ImportChecked(pdf_);
}

inline void FitManager::ImportData(const RooAbsData& data_) {
	ImportDataChecked(data_);
}

inline void FitManager::Fit(const std::string& fit_id_, const std::string dataset_id_, const std::string& model_id_, const FitOptions& options_) {
	EnsureFitIdAvailable(fit_id_);
	EnsureDataInWorkspace(dataset_id_);

	if((options_.strategy < 0) || (options_.strategy > 2)){
		printf("[FitManager::Fit] strategy must be 0, 1, or 2.\n");
		exit(1);
	}

	if((options_.Offset != "none") && (options_.Offset != "initial") && (options_.Offset != "bin")){
		printf("[FitManager::Fit] offset must be none, initial, or bin.\n");
		exit(1);
	}

	RooAbsPdf* pdf = GetPdf(model_id_);
	RooAbsData* data = GetData(dataset_id_);

	RooCmdArg range_arg = options_.range.empty() ? RooCmdArg::none() : RooFit::Range(options_.range.c_str());
    RooCmdArg minimizer_arg = options_.Minimizer.first.empty() ? RooCmdArg::none() : RooFit::Minimizer( options_.Minimizer.first.c_str(), options_.Minimizer.second.empty() ? nullptr : options_.Minimizer.second.c_str() );

	RooArgSet minos_parameters;
    for (const std::string& parameter_id : options_.Minos) {
        minos_parameters.add(*GetRooAbsReal(parameter_id));
    }
	RooCmdArg minos_arg = options_.Minos.empty() ? RooCmdArg::none() : RooFit::Minos(minos_parameters);

    std::unique_ptr<RooFitResult> result(pdf->fitTo(
		    *data,

		    RooFit::Save(true),

		    range_arg,
		    minimizer_arg,
		    minos_arg,

		    RooFit::Strategy(options_.strategy),
            RooFit::SumW2Error(options_.SumW2Error),
            RooFit::Extended(options_.extended),
            RooFit::Offset(options_.Offset),
            RooFit::PrintLevel(options_.print_level)
	    )
	);

	ImportChecked(*result, fit_id_);
	
};

inline void FitManager::PlotNLL(const std::string& nll_id_, const std::string& parameter_id_, const std::string& plot_name_){
	/*
	* plot NLL with fixing parameters, except parameter_id_
	*/
	const std::unordered_map<std::string, std::unique_ptr<RooAbsReal>>::iterator it = fit_nlls.find(nll_id_);
	
	if (it == fit_nlls.end()){
		printf("[FitManager::PlotNLL] nll %s does not exist.\n", nll_id_.c_str());
		exit(1);
	}

	RooAbsReal* nll = it->second.get();

	RooRealVar* parameter = GetRooRealVar(parameter_id_);

	std::unique_ptr<RooPlot> frame(parameter->frame());

	nll->plotOn(frame.get(), RooFit::ShiftToZero());

	TCanvas canvas((nll_id_ + "__nll_canvas").c_str(), (nll_id_ + "__nll_canvas").c_str(), 1000, 750);
    frame->Draw();
	canvas.SaveAs(plot_name_.c_str());

}

inline void FitManager::PlotProfileNLL(const std::string& nll_id_, const std::string& poi_id_, const std::string& plot_name_){
	/*
	* plot NLL with chaning poi_id_
	*/
	const std::unordered_map<std::string, std::unique_ptr<RooAbsReal>>::iterator it = fit_nlls.find(nll_id_);
	
	if (it == fit_nlls.end()){
		printf("[FitManager::PlotNLL] nll %s does not exist.\n", nll_id_.c_str());
		exit(1);
	}

	RooAbsReal* nll = it->second.get();

	RooRealVar* poi = GetRooRealVar(poi_id_);
	RooArgSet poi_set(*poi);

	std::unique_ptr<RooAbsReal> profile_nll(nll->createProfile(poi_set));

	std::unique_ptr<RooPlot> frame(poi->frame());

	profile_nll->plotOn(frame.get(), RooFit::ShiftToZero());

	TCanvas canvas((nll_id_ + "__profile_nll_canvas").c_str(), (nll_id_ + "__profile_nll_canvas").c_str(), 1000, 750);
    frame->Draw();
	canvas.SaveAs(plot_name_.c_str());

}

inline void FitManager::SaveWorkspace(const std::string& filename_){
	if(filename_.empty()){
		printf("[FitManager::SaveWorkspace] filename must not be empty.\n");
		exit(1);
	}

	if(!workspace){
		printf("[FitManager::SaveWorkspace] workspace is null.\n");
		exit(1);
	}

	if(loaded_file && (loaded_filename == filename_)){
		printf("[FitManager::SaveWorkspace] cannot overwrite  the currently loaded workspace file %s.\n", filename_.c_str());
		exit(1);
	}

    if (!working_datasets.empty()) {
        printf(
            "[FitManager::SaveWorkspace] "
            "cannot save while working datasets exist. "
            "Call FinalizeAllDataSet first.\n"
        );
        exit(1);
    }

	if(!workspace->writeToFile(filename_.c_str(), true)){
		printf("[FitManager::SaveWorkspace] failed to save workspace to %s.\n", filename_.c_str());
		exit(1);
	}
}

inline void FitManager::LoadWorkspace(const std::string& filename_, const std::string& workspace_name_){
	if(filename_.empty()){
		printf("[FitManager::LoadWorkspace] filename must not be empty.\n");
		exit(1);
	}

	if(workspace_name_.empty()){
		printf("[FitManager::LoadWorkspace] workspace must not be empty.\n");
		exit(1);
	}

	std::unique_ptr<TFile> input = std::unique_ptr<TFile>(TFile::Open(filename_.c_str(), "READ"));

	if(!input || input->IsZombie()){
		printf("[FitManager::LoadWorkspace] failed to open file %s.\n", filename_.c_str());
		exit(1);
	}

	RooWorkspace* stored_workspace = nullptr;
	input->GetObject(workspace_name_.c_str(), stored_workspace);

	if(!stored_workspace){
		printf("[FitManager::LoadWorkspace] RooWorkspace %s was not found in file %s.\n", workspace_name_.c_str(), filename_.c_str());
		exit(1);
	}

    working_datasets.clear();
    fit_nlls.clear();

	created_workspace.reset();

	loaded_file = std::move(input);
	loaded_filename = filename_;

	workspace = stored_workspace;
}

inline void FitManager::ExportFitResult(const std::string& filename_, const std::vector<std::string>& fit_ids_){
    TFile file(filename_.c_str(), "RECREATE");
	TTree tree("fit_result", "");

	std::unordered_map<std::string, double> branchnameTotree;
	bool structureDetermined = false;
	std::size_t size_fitresult;

	for(const std::string& fit_id : fit_ids_) {
		RooFitResult* fitres = GetFitResult(fit_id);
		const RooArgList& fitargs = fitres->floatParsFinal();

		for (int i = 0; i < fitargs.getSize(); ++i) {
			const RooRealVar* rrv = dynamic_cast<const RooRealVar*>( fitargs.at(i) );
            std::string name = rrv->GetName();

			if(structureDetermined){
				if(size_fitresult != fitargs.getSize()){
					printf("[FitManager::SaveFitResult] fit id %s holds %d values, while other does not.\n", fit_id.c_str(), fitargs.getSize());
					exit(1);
				}
			}

			if(!structureDetermined){
				tree->Branch((name + "_val").c_str(), &branchnameTotree[name + "_val"]);
				tree->Branch((name + "_err").c_str(), &branchnameTotree[name + "_err"]);
				tree->Branch((name + "_HIerr").c_str(), &branchnameTotree[name + "_HIerr"]);
				tree->Branch((name + "_LOerr").c_str(), &branchnameTotree[name + "_LOerr"]);
			}
			else{
				if(branchnameTotree.find(name + "_val") == branchnameTotree.end()){
					printf("[FitManager::SaveFitResult] fit id %s holds %s value, while other does not.\n", fit_id.c_str(), (name + "_val").c_str());
					exit(1);
				}
				if(branchnameTotree.find(name + "_err") == branchnameTotree.end()){
					printf("[FitManager::SaveFitResult] fit id %s holds %s value, while other does not.\n", fit_id.c_str(), (name + "_err").c_str());
					exit(1);
				}
				if(branchnameTotree.find(name + "_HIerr") == branchnameTotree.end()){
					printf("[FitManager::SaveFitResult] fit id %s holds %s value, while other does not.\n", fit_id.c_str(), (name + "_HIerr").c_str());
					exit(1);
				}
				if(branchnameTotree.find(name + "_LOerr") == branchnameTotree.end()){
					printf("[FitManager::SaveFitResult] fit id %s holds %s value, while other does not.\n", fit_id.c_str(), (name + "_LOerr").c_str());
					exit(1);
				}
			}
            branchnameTotree[name + "_val"] = rrv->getVal();
            branchnameTotree[name + "_err"] = rrv->getError();
            branchnameTotree[name + "_HIerr"] = rrv->getAsymErrorHi();
            branchnameTotree[name + "_LOerr"] = rrv->getAsymErrorLo();

		}

		if(!structureDetermined) {
			size_fitresult = fitargs.getSize();
			structureDetermined = true;
		}

		tree.Fill();
	}

	tree.Write();
    file.Close();

}

#endif 


