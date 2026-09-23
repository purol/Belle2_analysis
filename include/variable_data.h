#ifndef VARIABLE_DATA_H
#define VARIABLE_DATA_H

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using VariableValue = std::variant<int, unsigned int, float, double, std::string*>;
using VariableCounts = std::array<std::size_t, 5>;

// Variable order is shared by candidates. Values keep their original C++ types.
class VariableSchema {
public:
    // These numbers also match the alternatives in VariableValue.
    enum Type { Int, UInt, Float, Double, String };

private:
    std::vector<std::size_t> types;
    std::vector<std::size_t> indices;
    VariableCounts counts = {};

    // Cache schema changes once, instead of copying the schema for every candidate.
    std::array<std::shared_ptr<VariableSchema>, 5> schemas_after_append;
    std::map<std::size_t, std::shared_ptr<VariableSchema>> schemas_after_erase;

    void AppendType(std::size_t type) {
        indices.push_back(counts.at(type));
        types.push_back(type);
        counts.at(type)++;
    }

public:
    VariableSchema() = default;

    explicit VariableSchema(const std::vector<std::string>& VariableTypes) {
        for (const std::string& type : VariableTypes) AppendType(GetTypeIndex(type));
    }

    static std::size_t GetTypeIndex(const std::string& type) {
        if (type == "Int_t") return Int;
        if (type == "UInt_t") return UInt;
        if (type == "Float_t") return Float;
        if (type == "Double_t") return Double;
        if (type == "string") return String;
        throw std::invalid_argument("[VariableSchema] unsupported type: " + type);
    }

    static VariableCounts CountTypes(const std::vector<std::string>& VariableTypes) {
        VariableCounts result = {};
        for (const std::string& type : VariableTypes) result.at(GetTypeIndex(type))++;
        return result;
    }

    std::size_t size() const { return types.size(); }
    std::size_t GetType(std::size_t index) const { return types.at(index); }
    std::size_t GetIndex(std::size_t index) const { return indices.at(index); }
    const VariableCounts& GetCounts() const { return counts; }

    std::shared_ptr<VariableSchema> Append(std::size_t type) {
        // The first candidate creates the new schema. Other candidates reuse it.
        if (schemas_after_append.at(type) == nullptr) {
            std::shared_ptr<VariableSchema> new_schema = std::make_shared<VariableSchema>();
            new_schema->types = types;
            new_schema->indices = indices;
            new_schema->counts = counts;
            new_schema->AppendType(type);
            schemas_after_append.at(type) = new_schema;
        }
        return schemas_after_append.at(type);
    }

    std::shared_ptr<VariableSchema> Erase(std::size_t index) {
        if (index >= types.size()) throw std::out_of_range("[VariableSchema] variable index out of range");

        auto iter = schemas_after_erase.find(index);
        if (iter != schemas_after_erase.end()) return iter->second;

        // Keep the old schema intact because other candidates may still use it.
        std::shared_ptr<VariableSchema> new_schema = std::make_shared<VariableSchema>();
        for (std::size_t i = 0; i < types.size(); i++) {
            if (i != index) new_schema->AppendType(types.at(i));
        }
        schemas_after_erase.insert({index, new_schema});
        return new_schema;
    }
};

class VariableData {
private:
    std::vector<int> int_values;
    std::vector<unsigned int> uint_values;
    std::vector<float> float_values;
    std::vector<double> double_values;
    std::vector<std::string*> string_values;
    std::shared_ptr<VariableSchema> schema;

    void CheckIndex(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("[VariableData] variable index out of range");
    }

    std::size_t GetValueIndex(std::size_t index, std::size_t expected_type) const {
        CheckIndex(index);
        if (schema->GetType(index) != expected_type) throw std::bad_variant_access();
        return schema->GetIndex(index);
    }

public:
    VariableData() = default;

    explicit VariableData(const std::shared_ptr<VariableSchema>& schema_) : schema(schema_) {}

    std::size_t size() const {
        return int_values.size() + uint_values.size() + float_values.size() + double_values.size() + string_values.size();
    }

    bool empty() const { return size() == 0; }

    std::size_t GetType(std::size_t index) const {
        CheckIndex(index);
        return schema->GetType(index);
    }

    // GetValueIndex checks the type and maps the variable number to its array index.
    int& GetInt(std::size_t index) {
        return int_values.at(GetValueIndex(index, VariableSchema::Int));
    }

    const int& GetInt(std::size_t index) const {
        return int_values.at(GetValueIndex(index, VariableSchema::Int));
    }

    unsigned int& GetUInt(std::size_t index) {
        return uint_values.at(GetValueIndex(index, VariableSchema::UInt));
    }

    const unsigned int& GetUInt(std::size_t index) const {
        return uint_values.at(GetValueIndex(index, VariableSchema::UInt));
    }

    float& GetFloat(std::size_t index) {
        return float_values.at(GetValueIndex(index, VariableSchema::Float));
    }

    const float& GetFloat(std::size_t index) const {
        return float_values.at(GetValueIndex(index, VariableSchema::Float));
    }

    double& GetDouble(std::size_t index) {
        return double_values.at(GetValueIndex(index, VariableSchema::Double));
    }

    const double& GetDouble(std::size_t index) const {
        return double_values.at(GetValueIndex(index, VariableSchema::Double));
    }

    std::string*& GetString(std::size_t index) {
        return string_values.at(GetValueIndex(index, VariableSchema::String));
    }

    std::string* const& GetString(std::size_t index) const {
        return string_values.at(GetValueIndex(index, VariableSchema::String));
    }

    // Compatibility for read access. No variants are stored in a candidate.
    VariableValue at(std::size_t index) const {
        const std::size_t type = GetType(index);
        if (type == VariableSchema::Int) return GetInt(index);
        else if (type == VariableSchema::UInt) return GetUInt(index);
        else if (type == VariableSchema::Float) return GetFloat(index);
        else if (type == VariableSchema::Double) return GetDouble(index);
        else if (type == VariableSchema::String) return GetString(index);
        else throw std::bad_variant_access();
    }

    void push_back(const VariableValue& value) {
        const std::size_t type = value.index();
        const std::size_t index = size();
        if (schema == nullptr) schema = std::make_shared<VariableSchema>();

        // Loading uses an existing schema; derived variables extend it.
        std::shared_ptr<VariableSchema> next_schema;
        if (index == schema->size()) next_schema = schema->Append(type);
        else if (schema->GetType(index) != type) throw std::bad_variant_access();

        if (type == VariableSchema::Int) int_values.push_back(std::get<int>(value));
        else if (type == VariableSchema::UInt) uint_values.push_back(std::get<unsigned int>(value));
        else if (type == VariableSchema::Float) float_values.push_back(std::get<float>(value));
        else if (type == VariableSchema::Double) double_values.push_back(std::get<double>(value));
        else if (type == VariableSchema::String) string_values.push_back(std::get<std::string*>(value));
        else throw std::bad_variant_access();

        // Only change the schema after the value was successfully appended.
        if (next_schema != nullptr) schema = std::move(next_schema);
    }

    void reserve(const VariableCounts& counts) {
        int_values.reserve(counts.at(VariableSchema::Int));
        uint_values.reserve(counts.at(VariableSchema::UInt));
        float_values.reserve(counts.at(VariableSchema::Float));
        double_values.reserve(counts.at(VariableSchema::Double));
        string_values.reserve(counts.at(VariableSchema::String));
    }

    VariableCounts GetCapacities() const {
        return {int_values.capacity(), uint_values.capacity(), float_values.capacity(), double_values.capacity(), string_values.capacity()};
    }

    std::size_t GetCapacityBytes() const {
        return int_values.capacity() * sizeof(int) + uint_values.capacity() * sizeof(unsigned int)
            + float_values.capacity() * sizeof(float) + double_values.capacity() * sizeof(double)
            + string_values.capacity() * sizeof(std::string*);
    }

    void Erase(std::size_t index) {
        CheckIndex(index);
        auto next_schema = schema->Erase(index);
        const std::size_t offset = schema->GetIndex(index);
        const std::size_t type = schema->GetType(index);
        if (type == VariableSchema::Int) int_values.erase(int_values.begin() + offset);
        else if (type == VariableSchema::UInt) uint_values.erase(uint_values.begin() + offset);
        else if (type == VariableSchema::Float) float_values.erase(float_values.begin() + offset);
        else if (type == VariableSchema::Double) double_values.erase(double_values.begin() + offset);
        else if (type == VariableSchema::String) string_values.erase(string_values.begin() + offset);
        schema = std::move(next_schema);
    }

    void clear() {
        int_values.clear();
        uint_values.clear();
        float_values.clear();
        double_values.clear();
        string_values.clear();
        schema.reset();
    }
};

#endif
