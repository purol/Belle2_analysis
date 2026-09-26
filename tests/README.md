# Batch loading integration checks

Run from the repository root in the same ROOT/RooFit and FastBDT environment
used to build the analysis executables. These checks require C++17, as do the
existing `std::variant` and `std::optional` headers.

```bash
mkdir -p tmp
g++ $(root-config --cflags) -std=c++17 -Iinclude -IFastBDT/include \
    tests/batch_loading.cc -o tmp/batch_loading \
    $(root-config --ldflags --glibs) -Llib \
    -lRooFit -lRooStats -lRooFitCore -lMinuit -lFastBDT_static
./tmp/batch_loading
```

The following validation checks must each exit with status 1:

```bash
./tmp/batch_loading missing-key
./tmp/batch_loading empty-key
./tmp/batch_loading zero-size
```

The test creates a unique directory under the system temporary directory and
retains its generated ROOT files for inspection. It checks:

- Entry order, completeness, and event boundaries at several batch targets.
- Default event keys and the 100,000-input-entry soft target for both loaders.
- Cuts that accept only some candidates or reject entire batches/files.
- String and numeric custom event keys, including ROOT string buffer reuse.
- Event counts and one candidate per event after `RandomBCS`.
- Output accumulation across batches and output filename transitions.
- Empty files, multiple files, multiple load modules, and no matching files.

On older GCC versions, linking `std::filesystem` may additionally require
`-lstdc++fs`.

## Compare the random modules with main without ROOT

```bash
python tests/compare_random_modules.py --ref main
```

This script compiles the actual `Module`, `RandomBCS`, `RandomEventSelection`,
`GetRandom`, `Data`, expression-parser and `MemoryDataStore` source from `main`
and the working files. It compares full selected candidate/event identifiers and
generated values, using whole-file processing for `main` and six batch sizes for
the working files. It includes empty batches, cuts, separate generator instances,
file transitions, repeated filenames and stage-store round trips. ROOT classes
are not mocked: they are not part of this focused test at all. This does not
validate the ROOT loader or ROOT ownership.

On Windows, the script can locate an installed Visual Studio x64 compiler even
when it is not on PATH. On Linux it uses `g++` by default; use `--cxx` to override.

Negative control (expected to exit with status 1):

```bash
python tests/compare_random_modules.py --ref main --candidate-ref main
```

That comparison deliberately applies batches to the unmodified `main` modules
to confirm that the checks detect RNG resets. The `report.json`, source snapshots,
compiler logs and comparison outputs are retained in a temporary directory.

## Compare ROOT results with main

In the Linux analysis environment, with ROOT and FastBDT configured:

```bash
python3 tests/compare_root_main.py --ref main
```

On Windows, use a ROOT binary distribution matching the installed Visual Studio
compiler and pass its extracted directory:

```powershell
python tests/compare_root_main.py --ref main --root-dir C:/path/to/root `
    --fastbdt-include C:/path/to/FastBDT/include `
    --fastbdt-lib C:/path/to/fastbdt_build
```

The Windows runner loads `vcvars64.bat` and `thisroot.bat` only in its own process.
It expects `FastBDT_static.lib`, built with the same x64 compiler and `/MD` runtime.
The checked FastBDT revision was `fec22985fac27a223e0ee9ecc580e10c439c4414`;
its `FastBDT.cxx`, `Classifier.cxx` and `FastBDT_IO.cxx` were compiled with
`/std:c++17 /permissive- /FIstring /O2 /MD /EHsc /D_CRT_SECURE_NO_WARNINGS`
and archived with `lib.exe`. `/FIstring` supplies a missing standard include
without editing dependency sources.

Use `--fastbdt-include` and `--fastbdt-lib` if those dependencies are elsewhere.
The script snapshots the `main` headers and working headers into a temporary
directory without changing the branch, builds both versions, and runs the
integration checks above (including directory restoration and histogram lifetime).
It creates identical synthetic ROOT inputs for both versions, then compares
whole-file `main` processing against batch sizes 1, 17 and 100,000, with/without
load-time cuts and with/without random modules.

Comparisons cover branch schemas, entry order and numeric values in intermediate,
separate and combined trees, cutflow, and histogram contents/errors. They compare
the logical data, not ROOT file bytes, timestamps, compression layout or PNG
bytes. Output logs and `report.json` are retained. BDT training, fit results and
user-supplied custom modules are outside this fixture's scope.

The comparison driver sets `TH1::AddDirectory(false)` for both versions. With
ROOT's default ownership policy, the unmodified baseline attaches automatic
histograms to its output file and deletes them twice during cleanup in this
fixture. The standalone integration executable retains ROOT's default policy
and checks the candidate's histogram ownership fix independently.

Both binaries use the same compiler, standard library, ROOT and FastBDT. This is
necessary because the existing `std::hash` and random-distribution behavior is
not a cross-platform reproducibility guarantee.
