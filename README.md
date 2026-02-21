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
  * **Memory Optimization:** Utilized `std::variant`, `std::deque`, and `std::move` semantics to process large-scale datasets sequentially without causing Out-Of-Memory (OOM) errors.
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
```
