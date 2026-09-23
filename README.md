# C++ Data Analysis Framework for High Energy Physics

## Background & Motivation
In the high-energy physics (HEP) field, handling and analyzing massive datasets using the [ROOT](https://github.com/root-project/root) framework is essential. 
Because many physics data analyses share similar data processing pipelines, I developed this custom C++ framework to provide reusable, highly optimized common functions.

This repository provides an object-oriented framework to read massive ROOT files efficiently, parse string-based mathematical conditions, train Machine Learning models ([FastBDT](https://github.com/thomaskeck/FastBDT)), and visualize data distributions.

## Key Engineering Features
* **Languages & Standards:** Modern C++ (C++17)
* **Architecture:** OOP-based modular data pipeline (`Module` base class pattern)
* **Key Features:**
  * **Custom String Expression Parser:** Implemented a custom formula evaluator (Infix to Postfix conversion using Stack) to parse user-defined cut strings dynamically (e.g., `"Btag_deltaE > (-15) * Btag_Mbc + 79.15"`).
  * **Memory Optimization:** Uses typed candidate arrays with shared schemas, per-type capacity planning, `std::deque`, and move semantics to reduce memory use.
  * **Machine Learning Integration:** Seamlessly integrated FastBDT (Boosted Decision Trees) for multivariate analysis and signal classification.

## Basic Usages

**1. Define Loader Class**
```cpp
// The Loader class is the core pipeline manager of this framework.
Loader loader("TTree_name");
```

**2. Read ROOT Files**
```cpp
// Load(directory_name, included_string, label_name)
// Efficiently loads ROOT files and maps branches to memory dynamically.
loader.Load("./SIGNAL", ".root", "SIGNAL");
```

**3. Define Signal and Background Samples**
```cpp
// This classification is used to train BDT and optimize the Figure of Merit (FOM).
loader.SetSignal({ "SIGNAL" });
loader.SetBackground({ "CHG", "MIX" });
```

**4. Apply Cuts and Draw Distributions**
```cpp
// String-based mathematical conditions are dynamically parsed and evaluated.
loader.Cut("Btag_deltaE > (-15) * Btag_Mbc + 79.15");
loader.DrawTH1D("Btag_Mbc", ";Mbc [GeV];", 30, 5.27, 5.29, "Btag_Mbc.png");
```

**5. Advanced Modules**
Through the modular architecture, you can easily plug in additional operations:
1. Train FBDT and calculate AUC
2. Optimize cut criteria using Figure of Merit (FOM) / Punzi FOM
3. Draw Stacked Histograms (TH1/TH2)
4. ABCD background estimation method
5. Best Candidate Selection (BCS) handling

*(Check `./include/module.h` for the full list of implemented pipeline modules.)*

## Example Code & Actual Usage

* **Example:** Detailed example codes are located in `./src/Analysis_main.cc`.
* **Real-world Application:** This framework is actively used for the $\tau \to \mu \mu \mu$ [data analysis](https://github.com/purol/Belle_tau) in the Belle II experiment, processing actual large-scale collision data.

## Candidate variable storage

`Data::variable` stores values in separate `int`, `unsigned int`, `float`, `double`
and `std::string*` arrays. Variable order and type-specific indices are shared
through `VariableSchema`; they are not repeated for every candidate. Schema
transitions for adding/removing variables are cached and shared within the serial
module pipeline. String ownership remains in `Data::string_storage`.

The Loader reserves the maximum number of values **of each type** needed in a
stage, including the input schema. This also handles adding variables after
removal and restoring reduced batches. It still loads a file's selected candidates
together; this change does not implement chunked reading.

Existing read expressions such as `std::get<double>(data.variable.at(i))` work:
`at()` returns a temporary variant value. ROOT branch buffers and event comparison
keys still use variants. Candidate values themselves do not.

For custom modules, prefer direct typed access:

```cpp
double value = data.variable.Get<double>(i);
data.variable.Get<double>(i) = value + 1.0;
data.variable.push_back(0.25f);
data.variable.Erase(i);
```

`at()` no longer returns a reference: use `Get<T>()` for writes or references.
The storage is not a `std::vector`; vector iterators and `erase(begin() + i)` must
be replaced with indexed access and `Erase(i)`. As before, variable additions and
removals in custom modules must also update the Loader's schema at registration.
Borrowed references can be invalidated by additions/removals in the same typed
array. Do not mutate shared schema transitions concurrently from custom threads.

Numeric values retain their original types and precision. The array payload uses
`sizeof(T)` per value rather than `sizeof(std::variant<...>)`; each candidate has
five vector objects and a shared schema handle, so process memory savings also
depend on schema width, strings, allocators and ROOT buffers. A ROOT-enabled
regression test and a standalone storage test are described in [tests/README.md](tests/README.md).
