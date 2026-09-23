# Variable storage regression tests

Run these commands from the repository root. Both tests keep assertions enabled,
including in optimized builds.

## Storage and expression tests (no ROOT required)

```sh
g++ -std=c++17 -O2 -Iinclude tests/test_variable_data.cc -o /tmp/test_variable_data
/tmp/test_variable_data
```

This compares typed storage with variant values, including integer limits, NaN,
negative zero, string ownership after copies and moves, schema changes, expression
results, per-type reservations, and reduced batch restoration. It also reports
array capacity bytes for a mixed numeric schema. This is not a process RSS benchmark.

## ROOT pipeline test

ROOT with RooFit and the framework's FastBDT headers/library must be available.

```sh
g++ $(root-config --cflags) -std=c++17 -O2 -Iinclude -IFastBDT/include \
    tests/test_variable_pipeline.cc -Llib -lFastBDT_static \
    $(root-config --glibs) -lRooFit -lRooStats -lRooFitCore -lMinuit \
    -o /tmp/test_variable_pipeline
/tmp/test_variable_pipeline /tmp/belle2_variable_pipeline_check
```

The output directory must not already exist. The test leaves its small ROOT files
there for inspection. It covers Load, LoadWithCut, weights, cuts, derived variables,
event counts, BCS, deterministic RandomBCS, both ROOT writers, a stage transition,
variable removal at the start of a stage, and adding a Float_t after restoration.
It verifies branch types, values, strings, candidate order and weighted counts.
