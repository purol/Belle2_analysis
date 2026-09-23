#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>

#include "data.h"
#include "DataStore.h"
#include "string_equation.h"

void CheckValue(const VariableValue& expected, const VariableValue& actual) {
    assert(expected.index() == actual.index());
    std::visit([&](auto value) {
        using T = decltype(value);
        T result = std::get<T>(actual);
        if constexpr (std::is_floating_point_v<T>) {
            assert(std::memcmp(&value, &result, sizeof(T)) == 0);
        }
        else assert(value == result);
    }, expected);
}

void CheckValues(const std::vector<VariableValue>& expected, const VariableData& actual) {
    assert(expected.size() == actual.size());
    for (std::size_t i = 0; i < expected.size(); i++) CheckValue(expected.at(i), actual.at(i));
}

void TestValuesAndOwnership() {
    using namespace std::string_literals;
    const std::vector<std::string> types = {"Int_t", "UInt_t", "Float_t", "Double_t", "string", "Double_t", "Float_t"};
    auto schema = std::make_shared<VariableSchema>(types);
    Data original(schema);
    original.variable.reserve(schema->GetCounts());
    original.variable.push_back(std::numeric_limits<int>::min());
    original.variable.push_back(std::numeric_limits<unsigned int>::max());
    original.variable.push_back(-0.0f);
    original.variable.push_back(std::numeric_limits<double>::quiet_NaN());
    original.PushString("a long string with an embedded\0tail"s);
    original.variable.push_back(std::numeric_limits<double>::infinity());
    original.variable.push_back(std::numeric_limits<float>::denorm_min());
    std::vector<VariableValue> reference;
    for (std::size_t i = 0; i < original.variable.size(); i++) reference.push_back(original.variable.at(i));

    Data copied = original;
    original.variable.Get<int>(0) = 13;
    assert(copied.variable.Get<int>(0) == std::numeric_limits<int>::min());
    original = Data{};
    CheckValues(reference, copied.variable);
    assert(*copied.variable.Get<std::string*>(4) == "a long string with an embedded\0tail"s);
    Data moved = std::move(copied);
    CheckValues(reference, moved.variable);
    copied = Data{};
    moved.variable.Erase(4);
    reference.erase(reference.begin() + 4);
    moved.PushString("replacement");
    reference.push_back(moved.variable.at(moved.variable.size() - 1));
    CheckValues(reference, moved.variable);

    bool wrong_type = false;
    try { moved.variable.Get<double>(0); }
    catch (const std::bad_variant_access&) { wrong_type = true; }
    assert(wrong_type);
    bool out_of_range = false;
    try { moved.variable.at(moved.variable.size()); }
    catch (const std::out_of_range&) { out_of_range = true; }
    assert(out_of_range);
}

void TestSchemaChanges() {
    std::vector<std::string> types;
    const std::vector<std::string> numeric_types = {"Int_t", "UInt_t", "Float_t", "Double_t"};
    for (int i = 0; i < 80; i++) types.push_back(numeric_types.at(i % 4));
    auto schema = std::make_shared<VariableSchema>(types);
    std::mt19937 generator(8327);

    for (int row = 0; row < 100; row++) {
        VariableData values(schema);
        std::vector<VariableValue> reference;
        for (int i = 0; i < 80; i++) {
            switch (i % 4) {
            case 0: reference.push_back(static_cast<int>(generator() % 1000) - 500); break;
            case 1: reference.push_back(static_cast<unsigned int>(generator())); break;
            case 2: reference.push_back(static_cast<float>(generator()) / 3.0f); break;
            case 3: reference.push_back(static_cast<double>(generator()) / 7.0); break;
            }
            values.push_back(reference.back());
        }
        // The same schema can branch: mutation of one row must not affect others.
        VariableData unchanged = values;
        const auto initial = reference;
        for (int step = 0; step < 30; step++) {
            const std::size_t index = generator() % reference.size();
            values.Erase(index);
            reference.erase(reference.begin() + index);
            VariableValue value = (step % 2 == 0) ? VariableValue(float(step)) : VariableValue(double(step));
            values.push_back(value);
            reference.push_back(value);
            CheckValues(reference, values);
        }
        CheckValues(initial, unchanged);
        values.clear();
        values.push_back(9u);
        assert(values.size() == 1 && values.Get<unsigned int>(0) == 9u);
    }
}

void TestExpressionsAndReservation() {
    const std::vector<std::string> types = {"Int_t", "UInt_t", "Float_t", "Double_t"};
    std::vector<std::string> names = {"i", "u", "f", "d"};
    auto schema = std::make_shared<VariableSchema>(types);
    VariableCounts counts = {1, 1, 12, 70, 0};
    VariableData values(schema);
    values.reserve(counts);
    std::vector<VariableValue> reference = {-17, 4000000000u, 0.125f, -0.0};
    for (const auto& value : reference) values.push_back(value);
    const auto capacities = values.GetCapacities();

    for (const std::string& expression : {"i+u*f-d", "(i < 0) && (f > 0)", "(u/7)^0.5", "d/0"}) {
        std::cerr << "Expression: " << expression << '\n';
        auto postfix = PostfixExpression(replaceVariables(expression, &names), &types);
        double old_result = EvaluatePostfixExpression(postfix, reference, &types);
        double new_result = EvaluatePostfixExpression(postfix, values, &types);
        if (std::isnan(old_result)) assert(std::isnan(new_result));
        else assert(std::memcmp(&old_result, &new_result, sizeof(double)) == 0);
    }

    const float* first_float = &values.Get<float>(2);
    const double* first_double = &values.Get<double>(3);
    for (int i = 0; i < 11; i++) values.push_back(float(i));
    for (int i = 0; i < 69; i++) values.push_back(double(i));
    assert(values.GetCapacities() == capacities);
    assert(&values.Get<float>(2) == first_float);
    assert(&values.Get<double>(3) == first_double);
}

void TestReducedBatches() {
    const std::vector<std::string> names = {"i", "text", "f", "u", "d"};
    const std::vector<std::string> types = {"Int_t", "string", "Float_t", "UInt_t", "Double_t"};
    MemoryDataStore store;
    store.SetSchema(names, types, {"text", "u", "d"}, {"string", "UInt_t", "Double_t"});
    auto schema = std::make_shared<VariableSchema>(types);
    for (int batch = 0; batch < 2; batch++) {
        std::deque<Data> data;
        for (int i = 0; i < 5; i++) {
            Data item(schema);
            item.variable.push_back(-1);
            item.PushString("text " + std::to_string(batch * 5 + i));
            item.variable.push_back(3.0f);
            item.variable.push_back(static_cast<unsigned int>(batch * 5 + i));
            item.variable.push_back(-0.0);
            item.label = "signal";
            item.filename = "input.root";
            data.push_back(std::move(item));
        }
        store.WriteToBatch(std::move(data));
        data.clear();
    }
    VariableCounts counts = {1, 1, 4, 60, 1};
    store.SetReservedVariableCounts(counts);
    for (int batch = 0; batch < 2; batch++) {
        std::deque<Data> data;
        assert(store.ReadFromBatch(&data));
        for (int i = 0; i < 5; i++) {
            Data& item = data.at(i);
            assert(item.variable.Get<int>(0) == 0);
            assert(*item.variable.Get<std::string*>(1) == "text " + std::to_string(batch * 5 + i));
            assert(item.variable.Get<float>(2) == 0.0f);
            assert(item.variable.Get<unsigned int>(3) == static_cast<unsigned int>(batch * 5 + i));
            assert(std::signbit(item.variable.Get<double>(4)));
            assert(item.label == "signal" && item.filename == "input.root");
            const auto capacities = item.variable.GetCapacities();
            item.variable.Erase(0);
            for (int j = 0; j < 59; j++) item.variable.push_back(double(j));
            for (int j = 0; j < 3; j++) item.variable.push_back(float(j));
            assert(item.variable.GetCapacities() == capacities);
        }
    }
    std::deque<Data> empty;
    assert(!store.ReadFromBatch(&empty));
    store.Clear();
    store.SetSchema(names, types, {}, {});
    Data item(schema);
    item.variable.push_back(7);
    item.PushString("discarded");
    item.variable.push_back(2.0f);
    item.variable.push_back(4u);
    item.variable.push_back(9.0);
    std::deque<Data> input;
    input.push_back(std::move(item));
    store.WriteToBatch(std::move(input));
    assert(store.ReadFromBatch(&empty));
    assert(empty.front().variable.Get<std::string*>(1)->empty());
    assert(empty.front().variable.Get<int>(0) == 0);
}

void TestMemory() {
    std::vector<std::string> types;
    for (int i = 0; i < 100; i++) types.push_back("Int_t");
    for (int i = 0; i < 100; i++) types.push_back("Float_t");
    for (int i = 0; i < 133; i++) types.push_back("Double_t");
    auto schema = std::make_shared<VariableSchema>(types);
    VariableData data(schema);
    data.reserve(schema->GetCounts());
    std::vector<VariableValue> previous;
    previous.reserve(types.size());
    const std::size_t old_bytes = previous.capacity() * sizeof(VariableValue);
    const std::size_t new_bytes = data.GetCapacityBytes();
    assert(new_bytes < old_bytes);
    std::cout << "Mixed numeric example, 333 variables: " << old_bytes << " -> " << new_bytes
              << " array bytes per candidate; VariableData object: " << sizeof(VariableData) << " bytes\n";
}

int main() {
    std::cerr << "Values and ownership\n";
    TestValuesAndOwnership();
    std::cerr << "Schema changes\n";
    TestSchemaChanges();
    std::cerr << "Expressions and reservation\n";
    TestExpressionsAndReservation();
    std::cerr << "Reduced batches\n";
    TestReducedBatches();
    TestMemory();
    std::cout << "Variable storage regression tests passed\n";
}
