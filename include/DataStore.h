#ifndef DATASTORE_H
#define DATASTORE_H

#include <deque>

#include "data.h"

class DataStore {
public:
	virtual ~DataStore() {}

	virtual void WriteToBatch(std::deque<Data>&& data, const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) = 0;
	virtual void WriteToBatch(std::deque<Data>&& data) = 0;

	// return false if there is no batch
	virtual bool ReadFromBatch(std::deque<Data>* data, const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) = 0;
	virtual bool ReadFromBatch(std::deque<Data>* data) = 0;

	virtual void SetSchema(const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) = 0;

	virtual void Clear() = 0;
};

class MemoryDataStore : public DataStore {
private:
	// reduced variable_names
	std::vector<std::string> reduced_variable_names;

	// reduced VariableTypes
	std::vector<std::string> reduced_VariableTypes;

	// original variable_names
	std::vector<std::string> original_variable_names;

	// original VariableTypes
	std::vector<std::string> original_VariableTypes;

	// this Data is reduced
	std::deque<std::deque<Data>> batches;

	bool SchemaExists = false;

public:
	void WriteToBatch(std::deque<Data>&& data, const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) override {
		SetSchema(original_variable_names_, original_VariableTypes_, reduced_variable_names_, reduced_VariableTypes_);
		WriteToBatch(std::move(data));
	}

	void WriteToBatch(std::deque<Data>&& data) override {
		if (!SchemaExists) {
			printf("[MemoryDataStore::WriteToBatch] Schema does not exist\n");
			exit(1);
		}

		std::vector<std::size_t> reduced_indices;

		for (const std::string& reduced_variable_name : reduced_variable_names) {
			auto iter = std::find(original_variable_names.begin(), original_variable_names.end(), reduced_variable_name);

			if (iter == original_variable_names.end()) {
				printf("[MemoryDataStore::WriteToBatch] cannot find variable %s in original schema\n", reduced_variable_name.c_str());
				exit(1);
			}

			reduced_indices.push_back(static_cast<std::size_t>(std::distance(original_variable_names.begin(), iter)));
		}

		std::deque<Data> reduced_batch;

		for (Data& original_data : data) {
			if (original_data.variable.size() != original_variable_names.size()) {
				printf("[MemoryDataStore::WriteToBatch] data size mismatch: %zu != %zu\n", original_data.variable.size(), original_variable_names.size());
				exit(1);
			}

			Data reduced_data;
			reduced_data.variable.reserve(reduced_indices.size());

			for (std::size_t i = 0; i < reduced_indices.size(); i++) {
				const std::size_t original_index = reduced_indices.at(i);
				const std::string& type = original_VariableTypes.at(original_index);

				if (type == "string") {
					std::string* value = std::get<std::string*>(original_data.variable.at(original_index));

					if (value != nullptr) reduced_data.PushString(*value);
					else reduced_data.PushString("");
				}
				else {
					reduced_data.variable.push_back(original_data.variable.at(original_index));
				}
			}

			reduced_data.label = std::move(original_data.label);
			reduced_data.filename = std::move(original_data.filename);

			reduced_batch.push_back(std::move(reduced_data));
		}

		batches.push_back(std::move(reduced_batch));
	}

	bool ReadFromBatch(std::deque<Data>* data, const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) override {
		SetSchema(original_variable_names_, original_VariableTypes_, reduced_variable_names_, reduced_VariableTypes_);
		return ReadFromBatch(data);
	}

	bool ReadFromBatch(std::deque<Data>* data) override {
		if (!SchemaExists) {
			printf("[MemoryDataStore::WriteToBatch] Schema does not exist\n");
			exit(1);
		}

		if (batches.empty()) return false;

		std::deque<Data> reduced_batch = std::move(batches.front());
		batches.pop_front();

		std::deque<Data> restored_batch;

		for (Data& reduced_data : reduced_batch) {
			if (reduced_data.variable.size() != reduced_variable_names.size()) {
				printf("[MemoryDataStore::ReadFromBatch] reduced data size mismatch: %zu != %zu\n", reduced_data.variable.size(), reduced_variable_names.size());
				exit(1);
			}

			Data restored_data;
			restored_data.variable.reserve(original_variable_names.size());

			std::size_t reduced_index = 0;

			for (std::size_t original_index = 0; original_index < original_variable_names.size(); original_index++) {

				const std::string& original_name = original_variable_names.at(original_index);
				const std::string& original_type = original_VariableTypes.at(original_index);

				bool variable_is_stored = false;

				if (reduced_index < reduced_variable_names.size()) {
					variable_is_stored = (reduced_variable_names.at(reduced_index) == original_name);
				}

				if (variable_is_stored) {
					const auto& value = reduced_data.variable.at(reduced_index);

					if (original_type == "string") {
						std::string* string_value = std::get<std::string*>(value);

						if (string_value != nullptr) restored_data.PushString(*string_value);
						else restored_data.PushString("");
					}
					else {
						restored_data.variable.push_back(value);
					}

					reduced_index++;
				}
				else {
					// variable was not materialized.
					// Keep its original index by inserting a typed dummy value.
					if (original_type == "Double_t")  restored_data.variable.push_back(double{ 0.0 });
					else if (original_type == "Float_t") restored_data.variable.push_back(float{ 0.0 });
					else if (original_type == "Int_t") restored_data.variable.push_back(int{ 0 });
					else if (original_type == "UInt_t") restored_data.variable.push_back(static_cast<unsigned int>(0));
					else if (original_type == "string") restored_data.PushString("");
					else {
						printf("[MemoryDataStore::ReadFromBatch] unsupported type %s\n",original_type.c_str());
						exit(1);
					}
				}
			}

			if (reduced_index != reduced_variable_names.size()) {
				printf("[MemoryDataStore::ReadFromBatch] reduced schema is not consistent with original schema\n");
				exit(1);
			}

			restored_data.label = std::move(reduced_data.label);
			restored_data.filename = std::move(reduced_data.filename);

			restored_batch.push_back(std::move(restored_data));
		}

		*data = std::move(restored_batch);

		return true;
	}

	void SetSchema(const std::vector<std::string>& original_variable_names_, const std::vector<std::string>& original_VariableTypes_, const std::vector<std::string>& reduced_variable_names_, const std::vector<std::string>& reduced_VariableTypes_) {
		reduced_variable_names = reduced_variable_names_;
		reduced_VariableTypes = reduced_VariableTypes_;
		original_variable_names = original_variable_names_;
		original_VariableTypes = original_VariableTypes_;

		SchemaExists = true;
	}

	void Clear() override {
		reduced_variable_names.clear();
		reduced_VariableTypes.clear();
		original_variable_names.clear();
		original_VariableTypes.clear();

		SchemaExists = false;

		batches.clear();
	}
};

#endif 

