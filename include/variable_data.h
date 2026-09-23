#ifndef VARIABLE_DATA_H
#define VARIABLE_DATA_H

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using VariableValue = std::variant<int, unsigned int, float, double, std::string*>;
using VariableCounts = std::array<std::size_t, 5>;

// Variable order is shared by candidates. Values keep their original C++ types.
class VariableSchema {
private:
    std::vector<std::size_t> types;
    std::vector<std::size_t> indices;
    VariableCounts counts = {};

    // Cache schema changes once, instead of copying the schema for every candidate.
    std::array<std::shared_ptr<VariableSchema>, 5> appended_schemas;
    std::map<std::size_t, std::shared_ptr<VariableSchema>> erased_schemas;

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
        if (type == "Int_t") return 0;
        if (type == "UInt_t") return 1;
        if (type == "Float_t") return 2;
        if (type == "Double_t") return 3;
        if (type == "string") return 4;
        throw std::invalid_argument("[VariableSchema] unsupported type: " + type);
    }

    template <typename T>
    static constexpr std::size_t GetTypeIndex() {
        if constexpr (std::is_same_v<T, int>) return 0;
        else if constexpr (std::is_same_v<T, unsigned int>) return 1;
        else if constexpr (std::is_same_v<T, float>) return 2;
        else if constexpr (std::is_same_v<T, double>) return 3;
        else {
            static_assert(std::is_same_v<T, std::string*>, "unsupported variable type");
            return 4;
        }
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
        auto& result = appended_schemas.at(type);
        if (!result) {
            result = std::make_shared<VariableSchema>();
            result->types = types;
            result->indices = indices;
            result->counts = counts;
            result->AppendType(type);
        }
        return result;
    }

    std::shared_ptr<VariableSchema> Erase(std::size_t index) {
        types.at(index);
        auto& result = erased_schemas[index];
        if (!result) {
            result = std::make_shared<VariableSchema>();
            for (std::size_t i = 0; i < types.size(); i++) {
                if (i != index) result->AppendType(types.at(i));
            }
        }
        return result;
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

    template <typename T>
    std::vector<T>& Values() {
        if constexpr (std::is_same_v<T, int>) return int_values;
        else if constexpr (std::is_same_v<T, unsigned int>) return uint_values;
        else if constexpr (std::is_same_v<T, float>) return float_values;
        else if constexpr (std::is_same_v<T, double>) return double_values;
        else {
            static_assert(std::is_same_v<T, std::string*>, "unsupported variable type");
            return string_values;
        }
    }

    template <typename T>
    const std::vector<T>& Values() const {
        if constexpr (std::is_same_v<T, int>) return int_values;
        else if constexpr (std::is_same_v<T, unsigned int>) return uint_values;
        else if constexpr (std::is_same_v<T, float>) return float_values;
        else if constexpr (std::is_same_v<T, double>) return double_values;
        else {
            static_assert(std::is_same_v<T, std::string*>, "unsupported variable type");
            return string_values;
        }
    }

    void CheckIndex(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("[VariableData] variable index out of range");
    }

public:
    VariableData() = default;

    explicit VariableData(const std::shared_ptr<VariableSchema>& schema_) : schema(schema_) {}

    std::size_t size() const {
        return int_values.size() + uint_values.size() + float_values.size() + double_values.size() + string_values.size();
    }

    bool empty() const { return size() == 0; }

    template <typename T>
    bool Is(std::size_t index) const {
        CheckIndex(index);
        return schema->GetType(index) == VariableSchema::GetTypeIndex<T>();
    }

    template <typename T>
    const T& Get(std::size_t index) const {
        if (!Is<T>(index)) throw std::bad_variant_access();
        return Values<T>().at(schema->GetIndex(index));
    }

    template <typename T>
    T& Get(std::size_t index) {
        if (!Is<T>(index)) throw std::bad_variant_access();
        return Values<T>().at(schema->GetIndex(index));
    }

    // Compatibility for read access. No variants are stored in a candidate.
    VariableValue at(std::size_t index) const {
        CheckIndex(index);
        switch (schema->GetType(index)) {
        case 0: return Get<int>(index);
        case 1: return Get<unsigned int>(index);
        case 2: return Get<float>(index);
        case 3: return Get<double>(index);
        case 4: return Get<std::string*>(index);
        default: throw std::bad_variant_access();
        }
    }

    template <typename T>
    void push_back(T value) {
        const std::size_t type = VariableSchema::GetTypeIndex<T>();
        const std::size_t index = size();
        if (!schema) schema = std::make_shared<VariableSchema>();
        if (index == schema->size()) {
            auto next_schema = schema->Append(type);
            Values<T>().push_back(value);
            schema = std::move(next_schema);
        }
        else {
            if (schema->GetType(index) != type) throw std::bad_variant_access();
            Values<T>().push_back(value);
        }
    }

    void push_back(const VariableValue& value) {
        std::visit([&](auto item) { push_back(item); }, value);
    }

    void reserve(const VariableCounts& counts) {
        int_values.reserve(counts.at(0));
        uint_values.reserve(counts.at(1));
        float_values.reserve(counts.at(2));
        double_values.reserve(counts.at(3));
        string_values.reserve(counts.at(4));
    }

    // Legacy count-only reservation. Loader uses exact per-type counts instead.
    void reserve(std::size_t count) {
        VariableCounts counts = schema ? schema->GetCounts() : VariableCounts{};
        const std::size_t schema_size = schema ? schema->size() : 0;
        if (count > schema_size) counts.at(3) += count - schema_size;
        reserve(counts);
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
        switch (schema->GetType(index)) {
        case 0: int_values.erase(int_values.begin() + offset); break;
        case 1: uint_values.erase(uint_values.begin() + offset); break;
        case 2: float_values.erase(float_values.begin() + offset); break;
        case 3: double_values.erase(double_values.begin() + offset); break;
        case 4: string_values.erase(string_values.begin() + offset); break;
        }
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

template <typename T>
const T& GetVariableValue(const VariableData& variables, std::size_t index) {
    return variables.Get<T>(index);
}

template <typename T>
const T& GetVariableValue(const std::vector<VariableValue>& variables, std::size_t index) {
    return std::get<T>(variables.at(index));
}

#endif
