# Batch loading validation

Checked on 2026-09-26, in the working tree of `feature/batch_load_new`.
Baseline: local `main`, commit `10ed0540eb444a84e9701b46779ccb8baf6f2ad0`.

## Executed: random-module regression comparison

Compiler: Visual Studio x64 MSVC toolchain `14.51.36231`, C++17, Windows.
Command: `python tests/compare_random_modules.py --ref main`.

The actual class definitions were compiled from the baseline and working headers;
the algorithms were not reimplemented in the test. The driver supplied complete
events in memory. This validates these classes, not ROOT file I/O or the loader.

| Mode | Compared output rows | Batch targets passing |
|---|---:|---|
| RandomBCS | 285 | 1, 2, 3, 7, 31, 100000 |
| RandomEventSelection, partition 0 | 577 | 1, 2, 3, 7, 31, 100000 |
| RandomEventSelection, partition 1 | 543 | 1, 2, 3, 7, 31, 100000 |
| Two GetRandom instances | 1120 | 1, 2, 3, 7, 31, 100000 |
| GetRandom + RandomBCS + RandomEventSelection | 144 | 1, 2, 3, 7, 31, 100000 |

Result: **30/30 comparisons matched exactly**, including selected event/candidate
IDs and generated numeric values. The driver includes partially rejected events,
empty batches, a wholly rejected file, transitions between files, consecutive
equal filenames, and real `MemoryDataStore` round trips preserving file IDs.

Negative control: `--candidate-ref main` deliberately applies batches to the old
modules. It produced **25 mismatches**; the five whole-file-sized cases passed.
This confirms that the checks detect the RNG reset regression rather than merely
checking candidate counts.

## Executed: ROOT integration and full-header comparison

Installed the official ROOT **6.36.10 Windows x64 VC18** ZIP in a temporary
directory, without changing system configuration. Built the pinned FastBDT
revision `fec22985fac27a223e0ee9ecc580e10c439c4414` with the same Visual Studio
x64 MSVC `14.51.36231` toolchain. Dependency source was not modified; forced
inclusion of `<string>` and `/permissive-` resolved its MSVC build requirements.

`compare_root_main.py` compiled the actual full baseline and working headers
against that same ROOT and FastBDT installation. Both builds and the candidate's
`batch_loading.cc` integration executable succeeded.

Integration checks passed: numeric/string event keys, candidate ordering,
load-time cuts, empty files/batches, output accumulation, directory restoration,
histogram survival after a file closes, repeated filenames, multiple load
modules, and event-boundary preservation. With a default target of 100,000,
the boundary fixture produced batches of **100,002 and 1** for both loaders.
All three invalid-configuration checks exited with the expected status 1.

The comparison used two synthetic ROOT files containing **239,994 candidates
in 48,000 events** in total. Result: **12/12 comparisons matched exactly**:

| Loader | Downstream selection | Batch targets passing |
|---|---|---|
| Load | Deterministic BCS | 1, 17, 100000 |
| Load | GetRandom + RandomBCS + RandomEventSelection | 1, 17, 100000 |
| LoadWithCut | Deterministic BCS | 1, 17, 100000 |
| LoadWithCut | GetRandom + RandomBCS + RandomEventSelection | 1, 17, 100000 |

Compared branch schemas, entry order and numeric values in intermediate,
separate and combined trees, cutflow, and explicit TH1D/TH2D bin contents/errors.
Automatic histogram drawing also completed. Logical snapshots were compared
byte-for-byte; ROOT file bytes and rendered PNG bytes were not compared.

The first full comparison exposed a pre-existing baseline histogram ownership
problem: automatic histograms were attached to the output file and deleted
again during cleanup. The comparison driver therefore uses
`TH1::AddDirectory(false)` **for both versions**, without changing the baseline
headers. The separate integration test retains ROOT's default ownership policy
and verifies the candidate's ownership fix. Consequently, the output equivalence
claim is conditional on this shared setting, not on successful baseline execution
with default ROOT ownership.

The complete successful run, source snapshots, compile/run logs and JSON report
are retained under:
`C:/Users/purol/AppData/Local/Temp/belle2_root_verified_adgz8k4_/comparison2`.
The downloaded ROOT and FastBDT build are in its parent directory.

The production MUMU file, peak RSS under LSF, BDT training, fits and custom modules
have not been validated here. These results do not establish that all
analyses on this branch are equivalent to `main`.

See [README.md](README.md) for reproducible commands and dependencies.
