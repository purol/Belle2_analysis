#ifndef FIT_MANAGER_H
#define FIT_MANAGER_H

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <optional>

#include <RooAbsArg.h>
#include <RooRealVar.h>
#include <RooWorkspace.h>
#include <RooCategory.h>

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
};

struct ModelDefinition {
	std::vector<std::string> observable_ids;
	std::vector<std::string> parameter_ids;

	// Exact-size model: min_parameters == max_parameters.
	// Variadic model: max_parameters == std::nullopt
	std::size_t N_min_parameters = 0;
	std::optional<std::size_t> N_max_parameters;
};

class FitManager {
private:
	RooWorkspace workspace;
	std::unordered_map<std::string, WorkingDataSet> working_datasets;

	void ImportChecked(const RooAbsArg& object_);
	void EnsureWorkspaceNameAvailable(const std::string& id_) const;

	static const std::unordered_map<std::string, ModelDefinition>& ModelDefinitions();

	static void ValidateModelArguments(const std::string & model_type_, const ModelDefinition& definition_, std::size_t observable_count_, std::size_t parameter_count_);
public:
	explicit FitManager(const std::string& workspace_name_, const std::string& workspace_title_ = "");

	FitManager(const FitManager&) = delete;
	FitManager& operator=(const FitManager&) = delete;
	FitManager(FitManager&&) = delete;
	FitManager& operator=(FitManager&&) = delete;

	~FitManager() = default;

	void DefineObservable(const std::string& id_, const std::string& title_, double maximum_, double minimum_, const std::string& unit_ = "");
	void DefineDataSet(const std::string& id_, const std::vector<std::string> observable_ids_);
	void DefineFitParameter(const std::string& id_, const std::string& title_, double init_value_, double minimum_, double maximum_, const std::string& unit_ = "");
	void DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_ = "");
	void DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::pair<std::string, int>>& states_);

	void SetParameterConstant(const std::string& id_, bool constant_ = true);
	void SetRange(const std::string& variable_id_, const std::string& range_name_, double minimum_, double maximum_);

	RooAbsArg* GetRooAbsArg(const std::string& id_);
	const RooAbsArg* GetRooAbsArg(const std::string& id_) const;
	RooRealVar* GetRooRealVar(const std::string& id_);
	const RooRealVar* GetRooRealVar(const std::string& id_) const;
	RooCategory* GetRooCategory(const std::string& id_);
	const RooCategory* GetRooCategory(const std::string& id_) const;

	void FinalizeDataSet(const std::string& dataset_id_);
	void FinalizeAllDataSet();

	void DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options = {});
};

inline void FitManager::ImportChecked(const RooAbsArg& object_) {
	EnsureWorkspaceNameAvailable(object_.GetName());

	if (workspace.import(object_)) {
		printf("[FitManager::ImportChecked] failed to import RooAbsArg %s", object_.GetName());
		exit(1);
	}
}

inline void FitManager::EnsureWorkspaceNameAvailable(const std::string& id_) const {
	if (id_.empty()) {
		printf("[FitManager::EnsureWorkspaceNameAvailable] Object ID must not be empty.\n");
		exit(1);
	}

	if ((workspace.arg(id_.c_str()) = !nullptr) || (workspace.data(id_.c_str()) = !nullptr) || (workspace.genobj(id_.c_str()) = !nullptr) || (working_datasets.find(id_) != working_datasets.end())) {
		printf("[FitManager::EnsureWorkspaceNameAvailable] Ojbect %s already exists.\n", id_.c_str());
		exit(1);
	}
}

inline static const std::unordered_map<std::string, ModelDefinition>& FitManager::ModelDefinitions() {
	struct ModelDefinition {
		std::vector<std::string> observable_ids;
		std::vector<std::string> parameter_ids;

		// Exact-size model: min_parameters == max_parameters.
		// Variadic model: max_parameters == std::nullopt
		std::size_t N_min_parameters = 0;
		std::optional<std::size_t> N_max_parameters;
	};

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

static void FitManager::ValidateModelArguments(const std::string & model_type_, const ModelDefinition& definition_, std::size_t observable_count_, std::size_t parameter_count_){
	if (observable_count_ != definition_.observable_ids.size()){
		/* to do */
	}
}

inline void FitManager::DefineObservable(const std::string& id_, const std::string& title_, double maximum_, double minimum_, const std::string& unit_) {
	if (minimum_ > maximum_) {
		printf("[FitManager::DefineObservable] minimum is larger than maximum\n");
		exit(1);
	}

	RooRealVar observable(id_.c_str(), title_.c_str(), maximum_, minimum_, unit_.c_str());

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

	RooRealVar parameter(id_.c_str(), title_.c_str(), initial_value_, minimum_, maximum_, unit_.c_str());

	ImportChecked(parameter);
}

inline void FitManager::DefineConstantParameter(const std::string& id_, const std::string& title_, double value_, const std::string& unit_) {
	RooRealVar parameter(id_.c_str(), title_.c_str(), value_, unit_.c_str());

	parameter.setConstant(true);
	ImportChecked(parameter);
}

inline void FitManager::SetParameterConstant(const std::string& id_, bool constant_ = true) {
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

inline void FitManager::DefineCategory(const std::string& id_, const std::string title_, const std::vector<std::pair<std::string, int>>& states_) {
	if (states_.empty()) {
		printf("[FitManager::DefineCategory] at least one state is required.\n");
		exit(1);
	}

	RooCategory category(id_.c_str(), title_.c_str());

	for (const std::pair<std::string, int>& state : states_) {
		if (state.first.empty()) {
			printf("[FitManager::DefineCategory] state label must not be empty.\n");
			exit(1);
		}

		if (category.defineType(state.first, state.second)) {
			printf("[FitManager::DefineCategory] failed to define state %s", state.first.c_str());
		}
	}

	ImportChecked(category);
}

inline RooAbsArg* FitManager::GetRooAbsArg(const std::string& id_) {
	RooAbsArg* arg = workspace.arg(id_.c_str());

	if (!arg) {
		printf("[FitManager::GetRooAbsArg] RooAbsArg %s does not exist.\n", id_.c_str());
	}

	return arg;
}

inline const RooAbsArg* FitManager::GetRooAbsArg(const std::string& id_) const {
	RooAbsArg* arg = workspace.arg(id_.c_str());

	if (!arg) {
		printf("[FitManager::GetRooAbsArg] RooAbsArg %s does not exist.\n", id_.c_str());
	}

	return arg;
}

inline RooRealVar* FitManager::GetRooRealVar(const std::string& id_) {
	RooRealVar* variable = workspace.var(id_.c_str());

	if (!variable) {
		printf("[FitManager::GetRooRealVar] RooRealVar %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return variable;
}

const inline RooRealVar* FitManager::GetRooRealVar(const std::string& id_) const {
	RooRealVar* variable = workspace.var(id_.c_str());

	if (!variable) {
		printf("[FitManager::GetRooRealVar] RooRealVar %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return variable;
}

RooCategory* FitManager::GetRooCategory(const std::string& id_) {
	RooCategory* category = workspace.cat(id_.c_str());

	if (!category) {
		printf("[FitManager::GetRooCategory] RooCategory %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return category
}

const RooCategory* FitManager::GetRooCategory(const std::string& id_) const {
	RooCategory* category = workspace.cat(id_.c_str());

	if (!category) {
		printf("[FitManager::GetRooCategory] RooCategory %s does not exist.\n", id_.c_str());
		exit(1);
	}

	return category
}

inline void FitManager::FinalizeDataSet(const std::string& dataset_id_) {
	std::unordered_map<std::string, WorkingDataSet>::iterator it = working_datasets.find(dataset_id_);

	if (it == working_datasets.end()) {
		printf("[FitManager::FinalizeDataSet] dataset %s does not exist.\n", dataset_id_.c_str());
	}

	RooDataSet& dataset = it->dataset;

	if (workspace.data(dataset_id_.c_str()) != nullptr) {
		printf("[FitManager::FinalizeDataSet] dataset %s already exists in the workspace.\n", dataset_id_.c_str());
		exit(1);
	}

	if (workspace.import(dataset)) {
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

inline void FitManager::DefineModel(const std::string& model_id_, const std::string model_type_, const std::vector<std::string>& observable_ids_, const std::vector<std::string>& parameter_ids_, const ModelOptions& options = {}) {
	EnsureWorkspaceNameAvailable(model_id_);

	const std::unordered_map<std::string, ModelDefinition>::iterator it = ModelDefinitions().find(model_type_);

	if(it == ModelDefinitions().end()){
		printf("[FitManager::DefineModel] cannot find model type %s. Use ImportPdf() for an arbitrary RooAbsPdf.\n", model_type_.c_str());
		exit(1);
	}

	const ModelDefinition& definition = it->second;




}

#endif 

