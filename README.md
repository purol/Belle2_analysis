# Data Analysis Framework

## Background & Motivation
In high energy physics field (HEP), [ROOT](https://github.com/root-project/root) data structure and library are used.
Because several physics data analysis shares the analysis procedure, it is helpful to provide the common functions.
This repository provides useful framework to read ROOT file, optimize cut selection, train [FBDT](https://github.com/thomaskeck/FastBDT) as multivariate analysis technique, and draw plots to check data.

## Key Features

*   **Languages:** C++

## Basic Usages

**1. Define Loader Class**
```C++
Loader loader("TTree_name");
```
This Loader class is the fundamental block to use this framework. Put the name of TTree as input of the constructor.

**2. Read ROOT Files**
```C++
loader.Load("./SIGNAL", ".root", "SIGNAL");
```
Use `Load(directory name, included string in root files, name of label)` to read ROOT file, `name of label` is used to distinguish signal and background samples

**3. Define Signal and Background Samples**
```C++
loader.SetSignal({ "SIGNAL" });
loader.SetBackground({ "CHG", "MIX" });
```
Based on the name of label, loader class defines the signal and background samples. This classification is used to train BDT and optimize Figure of Merit.

**4. Apply Cuts and Draw Distributions**
```C++
loader.Cut("Btag_deltaE > (-15) * Btag_Mbc + 79.15");
loader.DrawTH1D("Btag_Mbc", ";Mbc [GeV];", 30, 5.27, 5.29, "Btag_Mbc.png");
```
You can apply cuts by `Cut(string to apply cut)`.
You can draw distribution of variables by `DrawTH1D`, `DrawTH2D`, and `DrawStack` functions.

**5. Other Functions**
With this framework, you can
1. train BDT and calculate AUC
2. optimize cut criteria by Figure of Merit
3. fill histogram (TH1 of TH2)
4. ABCD method
5. Best candidate selection
6. fill `RooDataSet`
   
Read `./include/module.h` to check full list of function.

## Example Code

Example codes are in `./src/Analysis_main.cc`

## Actual Usage

This framework is used for $\tau \to \mu \mu \mu$ [analysis](https://github.com/purol/Belle_tau) in Belle II experiment. 













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
