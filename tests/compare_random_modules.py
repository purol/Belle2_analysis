"""Compile the actual random-module class definitions from main and the worktree.

This focused comparison needs a C++17 compiler, but not ROOT. It does not test
ROOT I/O or Loader: the driver supplies complete events in batches. No module
implementation is mocked or rewritten; only unrelated classes are omitted.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[1]


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, encoding="utf-8", errors="replace", **kwargs)


def source(ref, name):
    if ref is None:
        return (REPO / "include" / name).read_text(encoding="utf-8")
    return run(["git", "show", f"{ref}:include/{name}"], cwd=REPO, capture_output=True).stdout


def extract_class(text, name):
    match = re.search(r"^    class " + re.escape(name) + r"(?: : public Module)? \{", text, re.M)
    if match is None:
        raise RuntimeError(f"Cannot locate class {name}")
    end = text.find("\n    };", match.start())
    if end == -1:
        raise RuntimeError(f"Cannot locate end of class {name}")
    return text[match.start():end + len("\n    };")]


def build(directory, ref, compiler):
    directory.mkdir()
    module = source(ref, "module.h")
    data = source(ref, "data.h")
    (directory / "data.h").write_text(data, encoding="utf-8")
    (directory / "DataStore.h").write_text(source(ref, "DataStore.h"), encoding="utf-8")
    (directory / "string_equation.h").write_text(source(ref, "string_equation.h"), encoding="utf-8")
    classes = [extract_class(module, name) for name in ("Module", "RandomBCS", "RandomEventSelection", "GetRandom")]
    header = '#include "data.h"\n#include "string_equation.h"\nclass EventWeight;\nnamespace Module {\n' + "\n".join(classes) + "\n}\n"
    (directory / "random_modules.h").write_text(header, encoding="utf-8")
    driver = directory / "random_module_driver.cc"
    shutil.copyfile(REPO / "tests/random_module_driver.cc", driver)
    executable = directory / ("compare.exe" if os.name == "nt" else "compare")
    has_file_id = "std::size_t file_id" in data
    if Path(compiler).stem.lower() == "cl":
        command = [compiler, "/nologo", "/std:c++17", "/EHsc", "/W3", "/D_CRT_SECURE_NO_WARNINGS"]
        if has_file_id:
            command.append("/DHAS_FILE_ID")
        command += [str(driver), "/Fe:" + str(executable), "/Fo:" + str(directory / "driver.obj")]
    else:
        command = [compiler, "-std=c++17", "-O2", "-Wall"]
        if has_file_id:
            command.append("-DHAS_FILE_ID")
        command += [str(driver), "-o", str(executable)]
    try:
        result = run(command, cwd=directory, capture_output=True)
    except subprocess.CalledProcessError as error:
        (directory / "build.log").write_text(error.stdout + error.stderr, encoding="utf-8")
        print(error.stdout + error.stderr)
        raise
    (directory / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="main")
    parser.add_argument("--candidate-ref", help="Use a Git ref instead of working files; main is a useful negative control")
    parser.add_argument("--cxx", default="cl" if os.name == "nt" else "g++")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if os.name == "nt" and Path(args.cxx).stem.lower() == "cl" and not os.environ.get("INCLUDE"):
        installations = sorted((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob("*/*/VC/Auxiliary/Build/vcvars64.bat"))
        if not installations:
            raise RuntimeError("Run from an x64 Visual Studio developer prompt")
        with tempfile.TemporaryDirectory(prefix="belle2_msvc_") as environment_directory:
            setup = Path(environment_directory) / "setup.cmd"
            setup.write_text(f'@echo off\ncall "{installations[-1]}" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding="utf-8")
            environment = run(["cmd.exe", "/d", "/c", str(setup)], capture_output=True).stdout
            for line in environment.splitlines():
                if "=" in line and not line.startswith("="):
                    key, value = line.split("=", 1)
                    os.environ[key] = value
        compilers = sorted(installations[-1].parents[2].glob("Tools/MSVC/*/bin/Hostx64/x64/cl.exe"))
        if compilers:
            args.cxx = str(compilers[-1])
    output = args.output or Path(tempfile.mkdtemp(prefix="belle2_random_compare_"))
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    baseline = build(output / "baseline", args.ref, args.cxx)
    candidate = build(output / "candidate", args.candidate_ref, args.cxx)
    report = {
        "baseline": run(["git", "rev-parse", args.ref], cwd=REPO, capture_output=True).stdout.strip(),
        "candidate": args.candidate_ref or "working tree",
        "compiler": args.cxx,
        "candidate_module_sha256": hashlib.sha256(source(args.candidate_ref, "module.h").encode()).hexdigest(),
        "scope": "Actual RandomBCS, RandomEventSelection, GetRandom and MemoryDataStore classes; synthetic in-memory events, no ROOT I/O",
        "cases": [],
    }
    for mode in ("bcs", "selection0", "selection1", "random", "pipeline"):
        expected = run([str(baseline), mode, "0"], capture_output=True).stdout
        (output / f"{mode}_main.txt").write_text(expected, encoding="utf-8")
        for size in (1, 2, 3, 7, 31, 100000):
            actual = run([str(candidate), mode, str(size)], capture_output=True).stdout
            (output / f"{mode}_batch{size}.txt").write_text(actual, encoding="utf-8")
            passed = actual == expected
            report["cases"].append({"mode": mode, "batch_size": size, "equal": passed, "rows": len(actual.splitlines())})
            print(f"{'PASS' if passed else 'FAIL'} {mode} batch={size}: {len(actual.splitlines())} rows")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Report: {output / 'report.json'}")
    return 0 if all(case["equal"] for case in report["cases"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
