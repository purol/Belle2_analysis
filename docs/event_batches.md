## Event batches

`Load` and `LoadWithCut` read about 10,000 events per batch by default. Values
continue to use `std::vector<std::variant<...>>`. Change the limit before `end()`:

```cpp
Loader loader("tau_lfv");
loader.SetEventBatchSize(10000);
loader.LoadWithCut(input_dir, filename, "sample", selection);
// Register the remaining modules as usual.
loader.end();
```

Use `SetEventBatchSize(0)` for the previous whole-file behavior, or specify input
event identifiers with `SetEventBatchSize(10000, {"run", "event"})`.
The default identifiers match the framework's event modules:
`__experiment__`, `__run__`, `__event__`, `__production__`, `__ncandidates__`.
The limit counts input events before cuts, not surviving candidates.

Before reading candidate values, each input file receives a pass over only its
event-identifier branches. A boundary is used only when the primary event key
and every event module's `EventGrouping()` change. Thus a coarser user-specified
grouping can enlarge a batch. The pass requires these keys to be in nondecreasing
lexicographic order. If keys are missing, non-finite or unordered, the reader
prints a diagnostic and reads the whole file. It never sorts candidates or splits
a repeated event silently. Added/removed event keys and interleaved input modules
also retain whole-file behavior when their semantics cannot be established.

`RandomBCS`, `RandomEventSelection`, and `GetRandom` keep their filename-seeded
random sequence across batches of the same input file. The file identity survives
`MemoryDataStore` handoff and distinguishes equal basenames in different inputs.
`PrintSeparateRootFile` keeps its output tree open between batches and recreates
it only on a new input file, preserving the existing basename-collision behavior.
ROOT directory state is restored with
[`TDirectory::TContext`](https://root.cern.ch/doc/master/classTDirectory_1_1TContext.html).

Custom modules use whole-file input by default. To opt in, override
`SupportsEventBatches()` after verifying that repeated `Process()` calls preserve
results. Event-based modules must also return their key names from
`EventGrouping()`. A custom source should implement `IsInputModule()` and
`ConfigureEventBatches()`, and can reuse `RootBatchReader`. Keep candidates from
one input file in a batch and preserve `Data::input_file_id` when producing rows.

This is a soft event-count limit, not a hard byte limit: one event can contain
many candidates. Training data, RooDataSet contents and `MemoryDataStore` batches
retained across a blocking stage can still grow with the total dataset size.

### Verification

Compile the boundary test without ROOT:

```sh
g++ -std=c++17 -O2 -Iinclude tests/test_event_batch_plan.cc -o /tmp/test_event_batch_plan
/tmp/test_event_batch_plan
```

In an environment with the same ROOT and FastBDT libraries used by the analysis:

```sh
g++ $(root-config --cflags) -std=c++17 -O2 -Iinclude -IFastBDT/include \
    tests/test_event_batch_root.cc -Llib $(root-config --ldflags --glibs) \
    -lRooFit -lRooStats -lRooFitCore -lMinuit -lFastBDT_static \
    -o /tmp/test_event_batch_root
/tmp/test_event_batch_root /tmp/belle2_event_batch_test_new
```

The output directory must not already exist. The integration test compares
whole-file and batch sizes 1, 2, 7 and 10,000, including Load/LoadWithCut, empty
files/batches, BCS ties, random selections, string branches, separate/combined
ROOT output, identical basenames, stage handoff, unordered events and custom
module fallback. It checks stored values and row order, not binary ROOT file
identity. The ROOT integration test requires execution on a ROOT-equipped host.
